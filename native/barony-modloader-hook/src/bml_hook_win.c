#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static void append_ascii(char** cursor, const char* end, const char* text)
{
    while (*text && *cursor < end) {
        **cursor = *text;
        ++(*cursor);
        ++text;
    }
}


static DWORD get_env_wide(const wchar_t* name, wchar_t* buffer, DWORD count)
{
    DWORD n = GetEnvironmentVariableW(name, buffer, count);
    if (n == 0 || n >= count) buffer[0] = L'\0';
    return n;
}

static void ensure_dir(const wchar_t* path)
{
    CreateDirectoryW(path, NULL);
}

static int file_exists(const wchar_t* path)
{
    if (!path || !*path) return 0;
    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
static int path_has_value_wide(const wchar_t* path)
{
    return path != NULL && path[0] != L'\0';
}

static int join_path_wide(wchar_t* out, DWORD out_count, const wchar_t* base, const wchar_t* relative)
{
    size_t base_len;
    size_t rel_len;
    if (!out || out_count == 0U || !path_has_value_wide(base) || !path_has_value_wide(relative)) {
        if (out && out_count) out[0] = L'\0';
        return -1;
    }
    base_len = (size_t)lstrlenW(base);
    rel_len = (size_t)lstrlenW(relative);
    if (base_len + 1U + rel_len + 1U > out_count) {
        out[0] = L'\0';
        return -1;
    }
    lstrcpynW(out, base, (int)out_count);
    if (base[base_len - 1U] != L'\\' && base[base_len - 1U] != L'/') {
        lstrcatW(out, L"\\");
    }
    lstrcatW(out, relative);
    return 0;
}

static int mkdir_p_wide(const wchar_t* path)
{
    wchar_t scratch[MAX_PATH * 4];
    DWORD index;
    if (!path_has_value_wide(path)) return -1;
    if (lstrlenW(path) + 1 >= (int)(sizeof(scratch) / sizeof(scratch[0]))) return -1;
    lstrcpynW(scratch, path, (int)(sizeof(scratch) / sizeof(scratch[0])));
    for (index = 0; scratch[index] != L'\0'; ++index) {
        if (scratch[index] == L'\\' || scratch[index] == L'/') {
            wchar_t saved = scratch[index];
            scratch[index] = L'\0';
            if (scratch[0] != L'\0' && !(index == 2 && scratch[1] == L':')) {
                ensure_dir(scratch);
            }
            scratch[index] = saved;
        }
    }
    ensure_dir(scratch);
    return 0;
}

#define BML_STASH_STATE_DIR_RELATIVE_PATH L"BaronyModLoader\\state"
#define BML_STASH_INVENTORY_RELATIVE_PATH L"BaronyModLoader\\state\\stash-inventory-v1.tsv"
#define BML_STASH_DIAGNOSTICS_RELATIVE_PATH L"BaronyModLoader\\state\\stash-diagnostics.jsonl"
#define BML_STASH_CORE_BEHAVIOR_REPORT_RELATIVE_PATH L"BaronyModLoader\\reports\\stash-core-behavior-report.json"
#define BML_STASH_STAT_VOID_CHEST_INVENTORY_OFFSET ((uintptr_t)0x9e8U)

#define BML_DETOUR_PATCH_BYTES 14U
#define BML_DETOUR_MAX_COPY_BYTES 32U
#define BML_DETOUR_NEAR_SEARCH_RANGE ((uintptr_t)0x70000000ULL)
#define BML_DETOUR_NEAR_SEARCH_STEP ((uintptr_t)0x10000ULL)
#define BML_DETOUR_MIN_MMAP_ADDRESS ((uintptr_t)0x10000ULL)
#define BML_DETOUR_MAX_INSTRUCTIONS 32U
#define BML_DETOUR_MAX_RELOCATED_BYTES ((BML_DETOUR_MAX_COPY_BYTES * 8U) + BML_DETOUR_PATCH_BYTES)
#define BML_GAME_MODE_MANAGER_RVA ((uintptr_t)0x00CC7AF0ULL)
#define BML_CHALLENGE_RUN_RVA ((uintptr_t)0x00CC7B28ULL)
#define BML_CHALLENGE_RUN_INUSE_OFFSET 0U
#define BML_CHALLENGE_RUN_EVENTTYPE_OFFSET 0x98U
#define BML_GAME_MODE_CUSTOM_RUN 4
#define BML_CHEVENT_SHOPPING_SPREE 2
#define BML_LOADING_SAVEGAME_RVA ((uintptr_t)0x010A0874ULL)
#define BML_LOADING_LOBBYKEY_RVA ((uintptr_t)0x010A0878ULL)
#define BML_WINDOWS_UNKNOWN_RUNTIME_ID "barony-bml-runtime-windows-unknown"
#define BML_WINDOWS_NOOP_RUNTIME_ID "barony-bml-runtime-windows-noop"
#define BML_WINDOWS_STASH_RUNTIME_ID "barony-bml-runtime-stash-windows"
#define BML_WINDOWS_RUNTIME_VERSION "0.1.0"
#define BML_STASH_ENTITY_OFFSET_UID ((uintptr_t)104U)
#define BML_STASH_ENTITY_OFFSET_X ((uintptr_t)208U)
#define BML_STASH_ENTITY_OFFSET_Y ((uintptr_t)216U)
#define BML_STASH_ENTITY_OFFSET_Z ((uintptr_t)224U)
#define BML_STASH_ENTITY_OFFSET_YAW ((uintptr_t)232U)
#define BML_STASH_ENTITY_OFFSET_FOCALX ((uintptr_t)256U)
#define BML_STASH_ENTITY_OFFSET_FOCALZ ((uintptr_t)272U)
#define BML_STASH_ENTITY_OFFSET_SIZEX ((uintptr_t)304U)
#define BML_STASH_ENTITY_OFFSET_SIZEY ((uintptr_t)308U)
#define BML_STASH_ENTITY_OFFSET_SPRITE ((uintptr_t)312U)
#define BML_STASH_ENTITY_OFFSET_SKILL ((uintptr_t)640U)
#define BML_STASH_ENTITY_OFFSET_SKILL17 ((uintptr_t)(BML_STASH_ENTITY_OFFSET_SKILL + 17U * sizeof(int32_t)))
#define BML_STASH_ENTITY_OFFSET_SKILL58 ((uintptr_t)(BML_STASH_ENTITY_OFFSET_SKILL + 58U * sizeof(int32_t)))
#define BML_STASH_CHEST_VOID_STATE_PERMANENT ((int32_t)-1)
#define BML_STASH_INTERNAL_MARKER_SKILL58 ((int32_t)0x424D4C00)
#define BML_STASH_ENTITY_OFFSET_FLAGS ((uintptr_t)880U)
#define BML_STASH_ENTITY_OFFSET_CHILDREN ((uintptr_t)920U)
#define BML_STASH_ENTITY_OFFSET_PARENT ((uintptr_t)936U)
#define BML_STASH_ENTITY_OFFSET_BEHAVIOR ((uintptr_t)4936U)
#define BML_STASH_MAP_OFFSET_ENTITIES ((uintptr_t)208U)
#define BML_STASH_LOBBY_PLACEMENT_X 232.0
#define BML_STASH_LOBBY_PLACEMENT_Y 280.0
#define BML_STASH_PI 3.14159265358979323846
#define BML_STASH_YAW_EAST 0.0
#define BML_STASH_YAW_SOUTH (BML_STASH_PI / 2.0)
#define BML_STASH_YAW_WEST BML_STASH_PI
#define BML_STASH_YAW_NORTH (3.0 * BML_STASH_PI / 2.0)
#define BML_STASH_LOBBY_PLACEMENT_YAW BML_STASH_YAW_NORTH
#define BML_STASH_PLACEMENT_LID_HINGE_OFFSET 3.0
#define BML_STASH_PLACEMENT_LID_OFFSET_Z (-2.75)
#define BML_STASH_SPRITE_CHEST_SPAWN 21
#define BML_STASH_SPRITE_CHEST_VOID_VISUAL 1791
#define BML_STASH_ACT_CHEST_WRAPPER_RVA ((uintptr_t)0x002E23C0ULL)
#define BML_STASH_ACT_CHEST_LID_BEHAVIOR_RVA ((uintptr_t)0x002E3560ULL)
#define BML_STASH_SPRITE_LID_SPAWN 216
#define BML_STASH_SPRITE_LID_VOID_VISUAL 1790
#define BML_STASH_MULTIPLAYER_CLIENT 2
#define BML_STASH_MULTIPLAYER_DIRECTCLIENT 4
#define BML_STASH_PLAYABLE_BEHAVIOR_REPORT_RELATIVE_PATH L"BaronyModLoader\\reports\\stash-playable-behavior-report.json"
typedef struct BmlBaronyNode {
    struct BmlBaronyNode* next;
    struct BmlBaronyNode* prev;
    struct BmlBaronyList* list;
    void* element;
    void (*deconstructor)(void* data);
    uint32_t size;
} BmlBaronyNode;

typedef struct BmlBaronyList {
    BmlBaronyNode* first;
    BmlBaronyNode* last;
} BmlBaronyList;

typedef struct BmlBaronyItem {
    int type;
    int status;
    int16_t beatitude;
    int16_t count;
    uint32_t appearance;
    bool identified;
} BmlBaronyItem;

typedef void* (*BmlWindowsStashNewItemFunction)(int type, int status, short beatitude, short count, unsigned int appearance, bool identified, void* inventory);
typedef void (*BmlWindowsStashListFreeAllFunction)(BmlBaronyList* list);
typedef void* (*BmlWindowsStashGetChestInventoryListFunction)(void* entity);
typedef void* (*BmlWindowsStashAddItemToChestFunction)(void* entity, void* item, bool force_new_stack, void* specific_destination_stack);
typedef void* (*BmlWindowsStashGetItemFromChestFunction)(void* entity, void* item, int amount, bool get_info_only);
typedef void* (*BmlWindowsStashAddItemToVoidChestServerFunction)(int player, void* item, bool force_new_stack, void* picked_up_stack);
typedef bool (*BmlWindowsStashStartMapNameFunction)(const char* name);
typedef BmlBaronyNode* (*BmlWindowsStashListAddNodeFirstFunction)(BmlBaronyList* list);
typedef void (*BmlWindowsStashEmptyDeconstructorFunction)(void* data);
typedef void* (*BmlWindowsStashNewEntityFunction)(int sprite, unsigned int pos, void* entity_list, void* creature_list);
typedef void (*BmlWindowsStashSetSpriteAttributesFunction)(void* entity, void* source, void* parent);
typedef int (*BmlWindowsStashGenerateDungeonFunction)(char* levelset, unsigned int seed, void* tuple_arg);
typedef void (*BmlWindowsStashAssignActionsFunction)(void* map_argument);
typedef void* (*BmlWindowsStashSummonNoSmokeFunction)(int creature, int x, int y, bool forceLocation);
typedef bool (*BmlWindowsStashRemoveItemFromVoidChestServerFunction)(int player, void* item, int count);
typedef void (*BmlWindowsStashCloseChestFunction)(void* entity);

static bool g_windows_stash_core_behavior_active = false;
static bool g_windows_stash_core_behavior_loaded = false;
static bool g_windows_stash_core_behavior_dirty = false;
static int g_windows_stash_core_behavior_loads = 0;
static int g_windows_stash_core_behavior_saves = 0;
static int g_windows_stash_core_behavior_dirty_marks = 0;
static wchar_t g_windows_stash_state_dir_path[MAX_PATH * 4];
static wchar_t g_windows_stash_inventory_path[MAX_PATH * 4];
static wchar_t g_windows_stash_diagnostics_path[MAX_PATH * 4];
static wchar_t g_windows_stash_core_behavior_report_path[MAX_PATH * 4];
static BmlBaronyList* g_windows_stash_core_behavior_inventory = NULL;

static BmlWindowsStashNewItemFunction g_windows_stash_new_item = NULL;
static BmlWindowsStashListFreeAllFunction g_windows_stash_list_free_all = NULL;
static BmlWindowsStashGetChestInventoryListFunction g_windows_stash_get_inventory_original = NULL;
static BmlWindowsStashAddItemToChestFunction g_windows_stash_add_item_to_chest_original = NULL;
static BmlWindowsStashGetItemFromChestFunction g_windows_stash_get_item_from_chest_original = NULL;
static BmlWindowsStashAddItemToVoidChestServerFunction g_windows_stash_add_item_to_void_original = NULL;
static BmlWindowsStashRemoveItemFromVoidChestServerFunction g_windows_stash_remove_item_from_void_original = NULL;
static BmlWindowsStashCloseChestFunction g_windows_stash_close_chest_original = NULL;
static BmlWindowsStashCloseChestFunction g_windows_stash_close_chest_server_original = NULL;
static void* g_windows_stash_stats_symbol = NULL;
static void* g_windows_resolved_shoparea_symbol = NULL;
static void* g_windows_resolved_multiplayer_symbol = NULL;
static void* g_windows_resolved_clientnum_symbol = NULL;
static int g_windows_stash_playable_active = 0;
static BmlWindowsStashNewEntityFunction g_windows_stash_new_entity_original = NULL;
static BmlWindowsStashSetSpriteAttributesFunction g_windows_stash_set_sprite_attributes_original = NULL;
static BmlWindowsStashGenerateDungeonFunction g_windows_stash_generate_dungeon_original = NULL;
static BmlWindowsStashAssignActionsFunction g_windows_stash_assign_actions_original = NULL;
static BmlWindowsStashSummonNoSmokeFunction g_windows_stash_summon_no_smoke_original = NULL;
static BmlWindowsStashListAddNodeFirstFunction g_windows_stash_list_add_node_first = NULL;
static BmlWindowsStashEmptyDeconstructorFunction g_windows_stash_empty_deconstructor = NULL;
static int g_windows_stash_playable_entity_list_logged = 0;
static void* g_windows_stash_playable_expected_entity_list = NULL;
static void* g_windows_stash_playable_observed_entity_list = NULL;
static uintptr_t g_windows_stash_playable_entity_list_offset = BML_STASH_MAP_OFFSET_ENTITIES;
static uintptr_t g_windows_stash_playable_creature_list_offset = BML_STASH_MAP_OFFSET_ENTITIES + sizeof(void*);
static int g_windows_stash_capture_new_entity_list = 0;
static void* g_windows_stash_playable_pending_shop_map = NULL;
static int g_windows_stash_playable_shop_retry_active = 0;
static int g_windows_stash_playable_shop_wait_logged = 0;
static void* g_windows_resolved_map_symbol = NULL;
static wchar_t g_windows_stash_playable_behavior_report_path[MAX_PATH * 4];
static int g_windows_stash_shopkeeper_spawn_seen = 0;
static double g_windows_stash_shopkeeper_spawn_x = 0.0;
static double g_windows_stash_shopkeeper_spawn_y = 0.0;
static void* g_windows_stash_playable_last_shop_map = NULL;
static void* g_windows_stash_playable_last_placed_shop_chest = NULL;
static void* g_windows_stash_playable_last_placed_shop_lid = NULL;
static bool* g_windows_owned_shoparea = NULL;
static size_t g_windows_owned_shoparea_cells = 0U;
#define BML_WINDOWS_RUNTIME_STRATEGY "installed-binary-hook"
static void force_windows_custom_run_shopping_spree(void)
{
    unsigned char* image_base = (unsigned char*)GetModuleHandleW(NULL);
    int* current_mode;
    unsigned char* challenge_run;
    if (!image_base) {
        return;
    }
    current_mode = (int*)(image_base + BML_GAME_MODE_MANAGER_RVA);
    challenge_run = image_base + BML_CHALLENGE_RUN_RVA;
    *current_mode = BML_GAME_MODE_CUSTOM_RUN;
    *(uint32_t*)(image_base + BML_LOADING_SAVEGAME_RVA) = 0U;
    *(uint32_t*)(image_base + BML_LOADING_LOBBYKEY_RVA) = 0U;
    *(unsigned char*)(challenge_run + BML_CHALLENGE_RUN_INUSE_OFFSET) = 1U;
    *(int*)(challenge_run + BML_CHALLENGE_RUN_EVENTTYPE_OFFSET) = BML_CHEVENT_SHOPPING_SPREE;
}

typedef struct BmlPatchInstruction {
    size_t source_offset;
    size_t source_length;
    size_t relocated_offset;
    size_t relocated_length;
} BmlPatchInstruction;

static bool bml_byte_is_short_relative_branch(unsigned char byte) {
    return byte >= 0x70U && byte <= 0x7fU;
}

static size_t bml_return_instruction_length(unsigned char byte) {
    if (byte == 0xc3U || byte == 0xcbU) {
        return 1U;
    }
    if (byte == 0xc2U || byte == 0xcaU) {
        return 3U;
    }
    return 0U;
}

static bool bml_modrm_is_register_only(unsigned char modrm) {
    return (modrm & 0xc0U) == 0xc0U;
}

static bool bml_modrm_uses_rip_relative(unsigned char modrm) {
    return (modrm & 0xc7U) == 0x05U;
}
static bool bml_opcode_uses_supported_modrm(unsigned char op) {
    return op == 0x31U || op == 0x33U || op == 0x39U || op == 0x3bU || op == 0x63U || op == 0x85U || op == 0x89U || op == 0x8bU || op == 0x8dU;
}

static bool bml_opcode_uses_supported_modrm_immediate(unsigned char op) {
    return op == 0x81U || op == 0x83U;
}

static int bml_decode_modrm_copyable_length(const unsigned char* code, size_t offset, size_t limit, size_t opcode_length, size_t* out_length, const char** out_code, const char** out_message, const char* truncated_message) {
    const size_t modrm_offset = offset + opcode_length;
    unsigned char modrm;
    unsigned char mod;
    unsigned char rm;
    size_t length = opcode_length + 1U;

    if (offset + opcode_length + 1U > limit) {
        *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
        *out_message = truncated_message;
        return -1;
    }

    modrm = code[modrm_offset];
    if (bml_modrm_is_register_only(modrm)) {
        *out_length = length;
        return 0;
    }

    mod = (unsigned char)(modrm & 0xc0U);
    rm = (unsigned char)(modrm & 0x07U);
    if (bml_modrm_uses_rip_relative(modrm)) {
        length += 4U;
    } else {
        if (rm == 0x04U) {
            unsigned char sib;
            if (offset + length + 1U > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = truncated_message;
                return -1;
            }
            sib = code[offset + length];
            length += 1U;
            if (mod == 0x00U && (sib & 0x07U) == 0x05U) {
                *out_code = "BML_DETOUR_MEMORY_OPERAND_UNSUPPORTED";
                *out_message = "Detour target prologue uses displacement-only SIB memory addressing outside this conservative decoder subset.";
                return -1;
            }
        }

        if (mod == 0x40U) {
            length += 1U;
        } else if (mod == 0x80U) {
            length += 4U;
        }
    }

    if (offset + length > limit) {
        *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
        *out_message = truncated_message;
        return -1;
    }

    *out_length = length;
    return 0;
}

static int bml_decode_register_only_group_copyable_length(const unsigned char* code, size_t offset, size_t limit, size_t opcode_length, size_t* out_length, const char** out_code, const char** out_message, const char* truncated_message, const char* unsupported_message) {
    const size_t modrm_offset = offset + opcode_length;
    unsigned char modrm;
    unsigned char reg_opcode;
    if (offset + opcode_length + 1U > limit) {
        *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
        *out_message = truncated_message;
        return -1;
    }
    modrm = code[modrm_offset];
    reg_opcode = (unsigned char)((modrm >> 3U) & 0x07U);
    if (!bml_modrm_is_register_only(modrm) || reg_opcode == 0U || reg_opcode == 1U) {
        *out_code = "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
        *out_message = unsupported_message;
        return -1;
    }
    *out_length = opcode_length + 1U;
    return 0;
}

static int bml_decode_modrm_immediate_copyable_length(const unsigned char* code, size_t offset, size_t limit, size_t opcode_length, size_t immediate_length, size_t* out_length, const char** out_code, const char** out_message, const char* truncated_message, const char* unsupported_message) {
    size_t base_length = 0U;
    unsigned char modrm;
    unsigned char reg_opcode;

    if (offset + opcode_length + 1U > limit) {
        *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
        *out_message = truncated_message;
        return -1;
    }

    modrm = code[offset + opcode_length];
    reg_opcode = (unsigned char)((modrm >> 3U) & 0x07U);
    if (reg_opcode != 0U && reg_opcode != 5U && reg_opcode != 7U) {
        *out_code = "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
        *out_message = unsupported_message;
        return -1;
    }

    if (bml_decode_modrm_copyable_length(code, offset, limit, opcode_length, &base_length, out_code, out_message, truncated_message) != 0) {
        return -1;
    }
    if (base_length > SIZE_MAX - immediate_length || offset + base_length + immediate_length > limit) {
        *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
        *out_message = truncated_message;
        return -1;
    }

    *out_length = base_length + immediate_length;
    return 0;
}

static int bml_decode_supported_x86_64_instruction(const unsigned char* code, size_t offset, size_t limit, size_t* out_length, const char** out_code, const char** out_message) {
    const unsigned char op = (offset < limit) ? code[offset] : 0U;

    *out_length = 0U;
    *out_code = "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
    *out_message = "Detour target prologue contains an instruction outside the conservative fixture-safe decoder subset.";

    if (offset >= limit) {
        *out_code = "BML_DETOUR_PATCH_WINDOW_TOO_LARGE";
        *out_message = "Detour decoder reached the bounded scan limit before finding a safe patch window.";
        return -1;
    }

    {
        const size_t return_length = bml_return_instruction_length(op);
        if (return_length > 0U) {
            if (offset + return_length > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = "Detour target prologue ended in the middle of a return instruction.";
                return -1;
            }
            if (offset + return_length < BML_DETOUR_PATCH_BYTES) {
                *out_code = "BML_DETOUR_EARLY_RETURN_UNSUPPORTED";
                *out_message = "Detour target returns before the absolute-jump patch window can be reserved.";
                return -1;
            }
            *out_length = return_length;
            return 0;
        }
    }

    if (op == 0xe8U || op == 0xe9U) {
        if (offset + 5U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported relative control-flow instruction.";
            return -1;
        }
        *out_length = 5U;
        return 0;
    }

    if (op == 0xebU || bml_byte_is_short_relative_branch(op)) {
        if (offset + 2U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported short relative control-flow instruction.";
            return -1;
        }
        *out_length = 2U;
        return 0;
    }

    if (op == 0x0fU && offset + 1U < limit && code[offset + 1U] >= 0x80U && code[offset + 1U] <= 0x8fU) {
        if (offset + 6U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported near conditional branch instruction.";
            return -1;
        }
        *out_length = 6U;
        return 0;
    }

    if (op == 0x0fU && offset + 1U < limit && (code[offset + 1U] == 0x1fU || code[offset + 1U] == 0xb6U || code[offset + 1U] == 0xb7U)) {
        return bml_decode_modrm_copyable_length(code, offset, limit, 2U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported two-byte ModRM instruction.");
    }

    if (op == 0x66U) {
        if (offset + 3U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported operand-size-prefixed instruction.";
            return -1;
        }
        if (code[offset + 1U] == 0x0fU && (code[offset + 2U] == 0x1fU || code[offset + 2U] == 0xefU)) {
            return bml_decode_modrm_copyable_length(code, offset, limit, 3U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported operand-size-prefixed ModRM instruction.");
        }
    }

    if (op >= 0x40U && op <= 0x4fU) {
        unsigned char next;
        const bool rex_w = (op & 0x08U) != 0U;
        if (offset + 2U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended after a REX prefix.";
            return -1;
        }
        next = code[offset + 1U];
        if (next >= 0x50U && next <= 0x5fU) {
            *out_length = 2U;
            return 0;
        }
        if (next >= 0xb8U && next <= 0xbfU) {
            const size_t length = rex_w ? 10U : 6U;
            if (offset + length > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = rex_w ? "Detour target prologue ended in the middle of a supported REX.W movabs immediate instruction." : "Detour target prologue ended in the middle of a supported REX mov immediate instruction.";
                return -1;
            }
            *out_length = length;
            return 0;
        }
        if (bml_opcode_uses_supported_modrm(next)) {
            return bml_decode_modrm_copyable_length(code, offset, limit, 2U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported REX ModRM instruction.");
        }
        if (next == 0x83U) {
            return bml_decode_modrm_immediate_copyable_length(code, offset, limit, 2U, 1U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported REX imm8 arithmetic/comparison instruction.", "Detour target prologue uses an unsupported REX imm8 arithmetic/comparison form.");
        }
        if (next == 0x81U) {
            return bml_decode_modrm_immediate_copyable_length(code, offset, limit, 2U, 4U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported REX imm32 arithmetic/comparison instruction.", "Detour target prologue uses an unsupported REX imm32 arithmetic/comparison form.");
        }
        if (next == 0xf7U) {
            return bml_decode_register_only_group_copyable_length(code, offset, limit, 2U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported REX register-only group instruction.", "Detour target prologue uses an unsupported REX group instruction form.");
        }
    }

    if (op == 0x90U || (op >= 0x50U && op <= 0x57U) || (op >= 0x58U && op <= 0x5fU)) {
        *out_length = 1U;
        return 0;
    }

    if (op >= 0xb8U && op <= 0xbfU) {
        if (offset + 5U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported mov immediate instruction.";
            return -1;
        }
        *out_length = 5U;
        return 0;
    }

    if (bml_opcode_uses_supported_modrm(op)) {
        return bml_decode_modrm_copyable_length(code, offset, limit, 1U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported ModRM instruction.");
    }

    if (op == 0x83U) {
        return bml_decode_modrm_immediate_copyable_length(code, offset, limit, 1U, 1U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported imm8 arithmetic/comparison instruction.", "Detour target prologue uses an unsupported imm8 arithmetic/comparison form.");
    }

    if (op == 0x81U) {
        return bml_decode_modrm_immediate_copyable_length(code, offset, limit, 1U, 4U, out_length, out_code, out_message, "Detour target prologue ended in the middle of a supported imm32 arithmetic/comparison instruction.", "Detour target prologue uses an unsupported imm32 arithmetic/comparison form.");
    }

    return -1;
}
static const char* find_ascii_token(const char* haystack, const char* needle)
{
    if (!haystack || !needle || !*needle) return NULL;
    for (const char* p = haystack; *p; ++p) {
        const char* h = p;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            ++h;
            ++n;
        }
        if (!*n) return p;
    }
    return NULL;
}

typedef struct BmlWindowsRuntimeReportInfo {
    char profile_id[128];
    char runtime_id[128];
    char runtime_version[32];
    char runtime_strategy[64];
    char game_revision[64];
    char stash_version[32];
    int has_stash;
} BmlWindowsRuntimeReportInfo;

typedef enum BmlWindowsRuntimeKind {
    BML_WINDOWS_RUNTIME_UNKNOWN = 0,
    BML_WINDOWS_RUNTIME_NOOP = 1,
    BML_WINDOWS_RUNTIME_STASH = 2,
} BmlWindowsRuntimeKind;

static void copy_ascii_string(char* out, DWORD out_count, const char* value)
{
    DWORD index = 0U;
    if (!out || out_count == 0U) return;
    if (!value) {
        out[0] = '\0';
        return;
    }
    while (value[index] && index + 1U < out_count) {
        out[index] = value[index];
        ++index;
    }
    out[index] = '\0';
}

static char* read_small_ascii_file(const wchar_t* path, SIZE_T max_bytes)
{
    HANDLE file;
    LARGE_INTEGER size = {0};
    char* buffer;
    DWORD read = 0;
    BOOL ok;
    if (!file_exists(path) || max_bytes == 0U) return NULL;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || (ULONGLONG)size.QuadPart > (ULONGLONG)max_bytes) {
        CloseHandle(file);
        return NULL;
    }
    buffer = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart + 1U);
    if (!buffer) {
        CloseHandle(file);
        return NULL;
    }
    ok = ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return NULL;
    }
    buffer[read] = '\0';
    return buffer;
}

static int build_quoted_ascii_token(const char* text, char* out, DWORD out_count)
{
    DWORD index = 0U;
    if (!text || !out || out_count < 3U) return 0;
    out[index++] = '"';
    while (*text && index + 2U < out_count) {
        out[index++] = *text;
        ++text;
    }
    if (*text) {
        out[0] = '\0';
        return 0;
    }
    out[index++] = '"';
    out[index] = '\0';
    return 1;
}

static int find_json_string_value(const char* json, const char* key, char* out, DWORD out_count)
{
    char needle[64];
    const char* value;
    DWORD index = 0U;
    if (!out || out_count < 2U) return 0;
    out[0] = '\0';
    if (!json || !key || !build_quoted_ascii_token(key, needle, (DWORD)(sizeof(needle) / sizeof(needle[0])))) {
        return 0;
    }
    value = find_ascii_token(json, needle);
    if (!value) return 0;
    value = find_ascii_token(value + lstrlenA(needle), ":");
    if (!value) return 0;
    ++value;
    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
        ++value;
    }
    if (*value != '"') return 0;
    ++value;
    while (value[index] && value[index] != '"' && index + 1U < out_count) {
        if (value[index] == '\\') {
            out[0] = '\0';
            return 0;
        }
        out[index] = value[index];
        ++index;
    }
    if (value[index] != '"' || index == 0U) {
        out[0] = '\0';
        return 0;
    }
    out[index] = '\0';
    return 1;
}

static int bml_runtime_manifest_has_mod(const char* manifest_json, const char* mod_id)
{
    char needle[128];
    if (!manifest_json || !mod_id || !build_quoted_ascii_token(mod_id, needle, (DWORD)(sizeof(needle) / sizeof(needle[0])))) {
        return 0;
    }
    return find_ascii_token(manifest_json, needle) != NULL;
}

static int bml_extract_mod_version(const char* manifest_json, const char* mod_id, char* out, DWORD out_count)
{
    char needle[128];
    const char* mod;
    if (!out || out_count < 2U) return 0;
    out[0] = '\0';
    if (!manifest_json || !mod_id || !build_quoted_ascii_token(mod_id, needle, (DWORD)(sizeof(needle) / sizeof(needle[0])))) {
        return 0;
    }
    mod = find_ascii_token(manifest_json, needle);
    if (!mod) return 0;
    return find_json_string_value(mod, "version", out, out_count);
}

static void bml_windows_runtime_report_info_init(BmlWindowsRuntimeReportInfo* info)
{
    if (!info) return;
    copy_ascii_string(info->profile_id, (DWORD)(sizeof(info->profile_id) / sizeof(info->profile_id[0])), "unknown-profile");
    copy_ascii_string(info->runtime_id, (DWORD)(sizeof(info->runtime_id) / sizeof(info->runtime_id[0])), BML_WINDOWS_UNKNOWN_RUNTIME_ID);
    copy_ascii_string(info->runtime_version, (DWORD)(sizeof(info->runtime_version) / sizeof(info->runtime_version[0])), BML_WINDOWS_RUNTIME_VERSION);
    copy_ascii_string(info->runtime_strategy, (DWORD)(sizeof(info->runtime_strategy) / sizeof(info->runtime_strategy[0])), BML_WINDOWS_RUNTIME_STRATEGY);
    copy_ascii_string(info->game_revision, (DWORD)(sizeof(info->game_revision) / sizeof(info->game_revision[0])), "unknown");
    copy_ascii_string(info->stash_version, (DWORD)(sizeof(info->stash_version) / sizeof(info->stash_version[0])), BML_WINDOWS_RUNTIME_VERSION);
    info->has_stash = 0;
}

static void bml_populate_windows_report_from_runtime_manifest(BmlWindowsRuntimeReportInfo* info, const char* manifest_json)
{
    char value[128];
    if (!info || !manifest_json) return;
    if (find_json_string_value(manifest_json, "profileId", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->profile_id, (DWORD)(sizeof(info->profile_id) / sizeof(info->profile_id[0])), value);
    }
    if (find_json_string_value(manifest_json, "runtimeId", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->runtime_id, (DWORD)(sizeof(info->runtime_id) / sizeof(info->runtime_id[0])), value);
    }
    if (find_json_string_value(manifest_json, "runtimeVersion", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->runtime_version, (DWORD)(sizeof(info->runtime_version) / sizeof(info->runtime_version[0])), value);
    }
    if (find_json_string_value(manifest_json, "runtimeStrategy", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->runtime_strategy, (DWORD)(sizeof(info->runtime_strategy) / sizeof(info->runtime_strategy[0])), value);
    }
    if (find_json_string_value(manifest_json, "gameVersionString", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->game_revision, (DWORD)(sizeof(info->game_revision) / sizeof(info->game_revision[0])), value);
    }
    if (find_json_string_value(manifest_json, "steamExecutableBuildId", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->game_revision, (DWORD)(sizeof(info->game_revision) / sizeof(info->game_revision[0])), value);
    }
    if (bml_extract_mod_version(manifest_json, "jml.stash", value, (DWORD)(sizeof(value) / sizeof(value[0])))) {
        copy_ascii_string(info->stash_version, (DWORD)(sizeof(info->stash_version) / sizeof(info->stash_version[0])), value);
    }
    info->has_stash = bml_runtime_manifest_has_mod(manifest_json, "jml.stash");
}

static BmlWindowsRuntimeKind bml_windows_runtime_kind_from_id(const char* runtime_id)
{
    if (!runtime_id || !*runtime_id) return BML_WINDOWS_RUNTIME_UNKNOWN;
    if (lstrcmpA(runtime_id, BML_WINDOWS_NOOP_RUNTIME_ID) == 0) return BML_WINDOWS_RUNTIME_NOOP;
    if (lstrcmpA(runtime_id, BML_WINDOWS_STASH_RUNTIME_ID) == 0) return BML_WINDOWS_RUNTIME_STASH;
    return BML_WINDOWS_RUNTIME_UNKNOWN;
}

static int extract_smoke_mod_id_from_manifest_json(const char* buffer, char* out, DWORD out_count, char* version_out, DWORD version_out_count)
{
    const char* mods;
    const char* object;
    const char* object_end = NULL;
    const char* capabilities;
    const char* capabilities_start;
    const char* capabilities_end = NULL;
    const char* package_id_key;
    const char* package_version_key;
    const char* capability_id_key;
    if (!buffer || !out || out_count < 2U || !version_out || version_out_count < 2U) return 0;
    out[0] = '\0';
    version_out[0] = '\0';
    mods = find_ascii_token(buffer, "\"mods\"");
    object = find_ascii_token(mods ? mods : buffer, "{");
    capabilities = NULL;
    capabilities_start = NULL;
    package_id_key = NULL;
    package_version_key = NULL;
    capability_id_key = NULL;
    if (object) {
        int depth = 0;
        for (const char* p = object; *p; ++p) {
            if (*p == '{') depth++;
            else if (*p == '}') {
                depth--;
                if (depth == 0) {
                    object_end = p;
                    break;
                }
            }
        }
    }
    if (object && object_end) {
        package_id_key = find_ascii_token(object, "\"id\"");
        package_version_key = package_id_key ? find_ascii_token(package_id_key + 1, "\"version\"") : NULL;
        capabilities = find_ascii_token(object, "\"capabilities\"");
        capabilities_start = capabilities ? find_ascii_token(capabilities, "[") : NULL;
        if (capabilities_start) {
            int depth = 0;
            for (const char* p = capabilities_start; *p; ++p) {
                if (*p == '[') depth++;
                else if (*p == ']') {
                    depth--;
                    if (depth == 0) {
                        capabilities_end = p;
                        break;
                    }
                }
            }
        }
        capability_id_key = capabilities_start ? find_ascii_token(capabilities_start, "\"id\"") : NULL;
    }
    if (!mods || !object || !object_end || !package_id_key || !package_version_key || !capabilities_start || !capabilities_end || !capability_id_key || package_id_key > capabilities_start || package_version_key > capabilities_start || capability_id_key > capabilities_end) {
        return 0;
    }
    {
        const char* another_capability_id = find_ascii_token(capability_id_key + 1, "\"id\"");
        if (another_capability_id && another_capability_id < capabilities_end) {
            return 0;
        }
    }
    {
        char capability_value[64];
        if (!find_json_string_value(capability_id_key, "id", capability_value, (DWORD)(sizeof(capability_value) / sizeof(capability_value[0])))) {
            return 0;
        }
        if (lstrcmpA(capability_value, "runtime_load_smoke") != 0) {
            return 0;
        }
    }
    if (!find_json_string_value(package_id_key, "id", out, out_count)) {
        return 0;
    }
    if (!find_json_string_value(package_version_key, "version", version_out, version_out_count)) {
        out[0] = '\0';
        return 0;
    }
    return out[0] != '\0' && version_out[0] != '\0';
}

static int extract_smoke_mod_id(const wchar_t* path, char* out, DWORD out_count, char* version_out, DWORD version_out_count)
{
    char* buffer = read_small_ascii_file(path, 1024U * 1024U);
    int ok;
    if (!buffer) return 0;
    ok = extract_smoke_mod_id_from_manifest_json(buffer, out, out_count, version_out, version_out_count);
    HeapFree(GetProcessHeap(), 0, buffer);
    return ok;
}

static int extract_resolved_symbol_rva(const wchar_t* path, const char* symbol_id, DWORD* out_rva)
{
    if (!file_exists(path) || !symbol_id || !*symbol_id || !out_rva) return 0;
    *out_rva = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 2 * 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart + 1);
    if (!buffer) {
        CloseHandle(file);
        return 0;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    buffer[read] = '\0';
    char needle[256];
    char* needle_cursor = needle;
    char* needle_end = needle + sizeof(needle) - 1;
    append_ascii(&needle_cursor, needle_end, "\"id\": \"");
    append_ascii(&needle_cursor, needle_end, symbol_id);
    append_ascii(&needle_cursor, needle_end, "\"");
    *needle_cursor = '\0';
    const char* id_key = find_ascii_token(buffer, needle);
    if (!id_key) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    const char* object_start = id_key;
    while (object_start > buffer && *object_start != '{') {
        --object_start;
    }
    const char* object_end = NULL;
    if (*object_start == '{') {
        int depth = 0;
        for (const char* p = object_start; *p; ++p) {
            if (*p == '{') depth++;
            else if (*p == '}') {
                depth--;
                if (depth == 0) {
                    object_end = p;
                    break;
                }
            }
        }
    }
    if (!object_end) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    const char* status_key = find_ascii_token(object_start, "\"status\"");
    const char* rva_key = find_ascii_token(object_start, "\"rva\"");
    if (!status_key || !rva_key || status_key > object_end || rva_key > object_end) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    if (!find_ascii_token(status_key, "\"resolved\"") || find_ascii_token(status_key, "\"resolved\"") > object_end) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    const char* colon = rva_key;
    while (*colon && *colon != ':' && colon < object_end) {
        ++colon;
    }
    if (*colon != ':') {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    ++colon;
    while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') {
        ++colon;
    }
    if (*colon < '0' || *colon > '9') {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    DWORD value = 0;
    while (*colon >= '0' && *colon <= '9') {
        value = value * 10U + (DWORD)(*colon - '0');
        ++colon;
    }
    *out_rva = value;
    HeapFree(GetProcessHeap(), 0, buffer);
    return value != 0U;
}
static int extract_resolved_symbol_address(const wchar_t* path, const char* symbol_id, void** out_address)
{
    DWORD rva = 0U;
    if (!out_address) return 0;
    *out_address = NULL;
    if (!extract_resolved_symbol_rva(path, symbol_id, &rva) || rva == 0U) {
        return 0;
    }
    *out_address = (unsigned char*)GetModuleHandleW(NULL) + rva;
    return 1;
}

static size_t windows_stash_inventory_count(const BmlBaronyList* inventory)
{
    size_t count = 0U;
    const BmlBaronyNode* node;
    if (!inventory) return 0U;
    for (node = inventory->first; node != NULL; node = node->next) {
        if (node->element) {
            count += 1U;
        }
    }
    return count;
}

static size_t windows_count_stash_inventory_file_rows(void)
{
    char* buffer;
    char* line;
    size_t rows = 0U;
    if (!path_has_value_wide(g_windows_stash_inventory_path)) return 0U;
    buffer = read_small_ascii_file(g_windows_stash_inventory_path, 1024U * 1024U);
    if (!buffer) return 0U;
    line = buffer;
    while (line && *line) {
        char* next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            ++next;
        }
        if (line[0] != '\0' && line[0] != '#') {
            rows += 1U;
        }
        line = next;
    }
    HeapFree(GetProcessHeap(), 0, buffer);
    return rows;
}
static void append_uint(char** cursor, const char* end, unsigned int value);
static void windows_append_stash_diagnostic_event(const char* event, const char* kind, const char* map_name, int has_position, double x, double y, int rows)
{
    HANDLE file;
    char json[512];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    DWORD written = 0;
    if (!path_has_value_wide(g_windows_stash_diagnostics_path) || !event || !*event) {
        return;
    }
    if (mkdir_p_wide(g_windows_stash_state_dir_path) != 0) {
        return;
    }
    file = CreateFileW(g_windows_stash_diagnostics_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    append_ascii(&cursor, end, "{\"event\": \"");
    append_ascii(&cursor, end, event);
    append_ascii(&cursor, end, "\"");
    if (kind && *kind) {
        append_ascii(&cursor, end, ", \"kind\": \"");
        append_ascii(&cursor, end, kind);
        append_ascii(&cursor, end, "\"");
    }
    if (map_name && *map_name) {
        append_ascii(&cursor, end, ", \"map\": \"");
        append_ascii(&cursor, end, map_name);
        append_ascii(&cursor, end, "\"");
    }
    if (has_position) {
        int len = snprintf(cursor, (size_t)(end - cursor), ", \"x\": %.3f, \"y\": %.3f", x, y);
        if (len > 0 && cursor + len < end) {
            cursor += len;
        }
    }
    if (rows >= 0) {
        append_ascii(&cursor, end, ", \"rows\": ");
        append_uint(&cursor, end, (unsigned int)rows);
    }
    append_ascii(&cursor, end, ", \"reportedAt\": \"2026-07-04T00:00:00Z\"}\n");
    *cursor = '\0';
    (void)WriteFile(file, json, (DWORD)(cursor - json), &written, NULL);
    CloseHandle(file);
}

static BmlBaronyList* windows_stash_stats_void_chest_inventory(void)
{
    void* stats_zero = NULL;
    if (!g_windows_stash_stats_symbol) {
        return NULL;
    }
    memcpy(&stats_zero, g_windows_stash_stats_symbol, sizeof(stats_zero));
    if (!stats_zero) {
        return NULL;
    }
    return (BmlBaronyList*)((unsigned char*)stats_zero + BML_STASH_STAT_VOID_CHEST_INVENTORY_OFFSET);
}

static int windows_stash_is_stats_void_chest_inventory(void* inventory)
{
    BmlBaronyList* stats_inventory = windows_stash_stats_void_chest_inventory();
    return inventory != NULL && stats_inventory != NULL && inventory == (void*)stats_inventory;
}

static int windows_stash_entity_uses_stats_void_chest(void* entity, BmlBaronyList** inventory_out)
{
    void* inventory = NULL;
    if (g_windows_stash_get_inventory_original) {
        inventory = g_windows_stash_get_inventory_original(entity);
    }
    if (inventory_out) {
        *inventory_out = (BmlBaronyList*)inventory;
    }
    return windows_stash_is_stats_void_chest_inventory(inventory);
}
static int32_t windows_entity_get_uid(void* entity)
{
    int32_t uid = 0;
    if (!entity) return 0;
    memcpy(&uid, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_UID, sizeof(uid));
    return uid;
}

static void windows_entity_set_s32(void* entity, uintptr_t offset, int32_t value)
{
    if (!entity) return;
    memcpy((unsigned char*)entity + offset, &value, sizeof(value));
}

static void windows_entity_set_real(void* entity, uintptr_t offset, double value)
{
    if (!entity) return;
    memcpy((unsigned char*)entity + offset, &value, sizeof(value));
}

static void windows_entity_set_skill(void* entity, size_t index, int32_t value)
{
    if (!entity) return;
    memcpy((unsigned char*)entity + BML_STASH_ENTITY_OFFSET_SKILL + index * sizeof(int32_t), &value, sizeof(value));
}

static void windows_entity_set_parent(void* entity, int32_t parent_uid)
{
    if (!entity) return;
    memcpy((unsigned char*)entity + BML_STASH_ENTITY_OFFSET_PARENT, &parent_uid, sizeof(parent_uid));
}

static void windows_entity_set_flag(void* entity, int flag_bit, int value)
{
    unsigned char byte_value = value ? 1U : 0U;
    if (!entity || flag_bit < 0) return;
    memcpy((unsigned char*)entity + BML_STASH_ENTITY_OFFSET_FLAGS + (uintptr_t)flag_bit, &byte_value, sizeof(byte_value));
}
static void reset_windows_stash_playable_state(void)
{
    g_windows_stash_playable_active = 0;
    g_windows_stash_new_entity_original = NULL;
    g_windows_stash_set_sprite_attributes_original = NULL;
    g_windows_stash_generate_dungeon_original = NULL;
    g_windows_stash_assign_actions_original = NULL;
    g_windows_stash_summon_no_smoke_original = NULL;
    g_windows_stash_list_add_node_first = NULL;
    g_windows_stash_empty_deconstructor = NULL;
    g_windows_stash_playable_expected_entity_list = NULL;
    g_windows_stash_playable_observed_entity_list = NULL;
    g_windows_stash_capture_new_entity_list = 0;
    g_windows_stash_playable_pending_shop_map = NULL;
    g_windows_stash_playable_shop_retry_active = 0;
    g_windows_stash_playable_shop_wait_logged = 0;
    g_windows_stash_shopkeeper_spawn_seen = 0;
    g_windows_stash_shopkeeper_spawn_x = 0.0;
    g_windows_stash_shopkeeper_spawn_y = 0.0;
    g_windows_stash_playable_last_shop_map = NULL;
    g_windows_stash_playable_last_placed_shop_chest = NULL;
    g_windows_stash_playable_last_placed_shop_lid = NULL;
    g_windows_owned_shoparea = NULL;
    g_windows_owned_shoparea_cells = 0U;
    g_windows_stash_playable_entity_list_offset = BML_STASH_MAP_OFFSET_ENTITIES;
    g_windows_stash_playable_creature_list_offset = BML_STASH_MAP_OFFSET_ENTITIES + sizeof(void*);
    g_windows_resolved_multiplayer_symbol = NULL;
    g_windows_resolved_clientnum_symbol = NULL;
}

static int windows_stash_playable_is_start_map_name(const char* name)
{
    if (!name || !*name) {
        return 0;
    }
    return lstrcmpA(name, "fake-lobby") == 0 || lstrcmpA(name, "Start Map") == 0;
}
static int windows_stash_playable_allows_walkable_shop_fallback(const char* name)
{
    if (!name || !*name) {
        return 0;
    }
    return lstrcmpA(name, "Minetown") == 0 ||
        lstrcmpA(name, "Mages Guild") == 0 ||
        strstr(name, "Shop") != NULL ||
        strstr(name, "shop") != NULL ||
        strstr(name, "Guild") != NULL;
}

static int windows_stash_playable_read_int_symbol(void* symbol, int* value_out)
{
    if (symbol == NULL || value_out == NULL) {
        return 0;
    }
    memcpy(value_out, symbol, sizeof(*value_out));
    return 1;
}

static int windows_stash_playable_is_multiplayer_client(void)
{
    int multiplayer_value = 0;
    int clientnum_value = 0;
    (void)windows_stash_playable_read_int_symbol(g_windows_resolved_multiplayer_symbol, &multiplayer_value);
    (void)windows_stash_playable_read_int_symbol(g_windows_resolved_clientnum_symbol, &clientnum_value);
    (void)clientnum_value;
    return multiplayer_value == BML_STASH_MULTIPLAYER_CLIENT || multiplayer_value == BML_STASH_MULTIPLAYER_DIRECTCLIENT;
}

static void* windows_stash_act_chest_behavior_pointer(void)
{
    return (unsigned char*)GetModuleHandleW(NULL) + BML_STASH_ACT_CHEST_WRAPPER_RVA;
}

static void* windows_stash_act_chest_lid_behavior_pointer(void)
{
    return (unsigned char*)GetModuleHandleW(NULL) + BML_STASH_ACT_CHEST_LID_BEHAVIOR_RVA;
}
static void* windows_stash_call_new_entity_with_diagnostics(const char* kind, int sprite, unsigned int pos, void* entity_list, void* creature_list)
{
    windows_append_stash_diagnostic_event("stash_access_point_step", kind, NULL, 1, (double)(uintptr_t)entity_list, (double)(uintptr_t)creature_list, sprite);
    return g_windows_stash_new_entity_original != NULL ? g_windows_stash_new_entity_original(sprite, pos, entity_list, creature_list) : NULL;
}

static void windows_empty_deconstructor(void* data)
{
    (void)data;
}

static int windows_pointer_readable(const void* pointer, size_t minimum_size)
{
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T queried;
    if (pointer == NULL || minimum_size == 0U) {
        return 0;
    }
    queried = VirtualQuery(pointer, &mbi, sizeof(mbi));
    if (queried != sizeof(mbi) || mbi.State != MEM_COMMIT) {
        return 0;
    }
    if ((mbi.Protect & PAGE_GUARD) != 0 || mbi.Protect == PAGE_NOACCESS) {
        return 0;
    }
    return ((uintptr_t)mbi.BaseAddress + mbi.RegionSize) >= ((uintptr_t)pointer + minimum_size);
}

static int windows_is_plausible_list_pointer(BmlBaronyList* entity_list)
{
    BmlBaronyNode* first = NULL;
    BmlBaronyNode* last = NULL;
    if (entity_list == NULL || (uintptr_t)entity_list < 0x10000U || (((uintptr_t)entity_list) & (sizeof(void*) - 1U)) != 0U) {
        return 0;
    }
    if (!windows_pointer_readable(entity_list, sizeof(BmlBaronyList))) {
        return 0;
    }
    memcpy(&first, &entity_list->first, sizeof(first));
    memcpy(&last, &entity_list->last, sizeof(last));
    if (first == NULL && last == NULL) {
        return 1;
    }
    if (first != NULL && windows_pointer_readable(first, sizeof(BmlBaronyNode))) {
        BmlBaronyList* node_list = NULL;
        memcpy(&node_list, &first->list, sizeof(node_list));
        if (node_list == entity_list) {
            return 1;
        }
    }
    if (last != NULL && windows_pointer_readable(last, sizeof(BmlBaronyNode))) {
        BmlBaronyList* node_list = NULL;
        memcpy(&node_list, &last->list, sizeof(node_list));
        if (node_list == entity_list) {
            return 1;
        }
    }
    return 0;
}

static BmlBaronyList* windows_playable_get_map_entity_list(void* map_argument)
{
    BmlBaronyList* entity_list = NULL;
    uintptr_t offset;
    if (!map_argument) return NULL;
    memcpy(&entity_list, (unsigned char*)map_argument + g_windows_stash_playable_entity_list_offset, sizeof(entity_list));
    if (windows_is_plausible_list_pointer(entity_list)) {
        return entity_list;
    }
    for (offset = 64U; offset + sizeof(void*) * 3U <= 1024U; offset += sizeof(void*)) {
        void* first = NULL;
        void* second = NULL;
        void* third = NULL;
        memcpy(&first, (unsigned char*)map_argument + offset, sizeof(first));
        memcpy(&second, (unsigned char*)map_argument + offset + sizeof(void*), sizeof(second));
        memcpy(&third, (unsigned char*)map_argument + offset + sizeof(void*) * 2U, sizeof(third));
        if (windows_is_plausible_list_pointer((BmlBaronyList*)first) &&
            windows_is_plausible_list_pointer((BmlBaronyList*)second) &&
            windows_is_plausible_list_pointer((BmlBaronyList*)third)) {
            g_windows_stash_playable_entity_list_offset = offset;
            g_windows_stash_playable_creature_list_offset = offset + sizeof(void*);
            return (BmlBaronyList*)first;
        }
    }
    return NULL;
}

static BmlBaronyList* windows_playable_get_map_creature_list(void* map_argument)
{
    BmlBaronyList* creature_list = NULL;
    if (!map_argument) return NULL;
    memcpy(&creature_list, (unsigned char*)map_argument + g_windows_stash_playable_creature_list_offset, sizeof(creature_list));
    if (windows_is_plausible_list_pointer(creature_list)) {
        return creature_list;
    }
    (void)windows_playable_get_map_entity_list(map_argument);
    memcpy(&creature_list, (unsigned char*)map_argument + g_windows_stash_playable_creature_list_offset, sizeof(creature_list));
    return windows_is_plausible_list_pointer(creature_list) ? creature_list : NULL;
}

typedef struct BmlWindowsPlacementMapPrefix {
    char name[32];
    char author[32];
    unsigned int width;
    unsigned int height;
    unsigned int skybox;
    int32_t flags[16];
    int32_t* tiles;
} BmlWindowsPlacementMapPrefix;

static bool* windows_stash_shoparea_pointer(void)
{
    bool* shoparea = NULL;
    if (g_windows_resolved_shoparea_symbol != NULL) {
        memcpy(&shoparea, g_windows_resolved_shoparea_symbol, sizeof(shoparea));
    }
    return shoparea;
}
static void windows_stash_shop_tile_center(unsigned int tile_x, unsigned int tile_y, double* world_x_out, double* world_y_out)
{
    if (world_x_out != NULL) {
        *world_x_out = (double)tile_x * 16.0 + 8.0;
    }
    if (world_y_out != NULL) {
        *world_y_out = (double)tile_y * 16.0 + 8.0;
    }
}
static double windows_stash_shop_world_to_tile(double world_coordinate)
{
    return (world_coordinate - 8.0) / 16.0;
}

static int windows_stash_find_nearest_shoparea_tile(BmlWindowsPlacementMapPrefix* map_prefix, const bool* shoparea, double anchor_world_x, double anchor_world_y, double* world_x_out, double* world_y_out)
{
    unsigned int x;
    unsigned int y;
    int found = 0;
    double best_distance_sq = 0.0;
    double best_world_x = 0.0;
    double best_world_y = 0.0;
    double anchor_tile_x = windows_stash_shop_world_to_tile(anchor_world_x);
    double anchor_tile_y = windows_stash_shop_world_to_tile(anchor_world_y);
    if (map_prefix == NULL || shoparea == NULL) {
        return 0;
    }
    for (x = 0U; x < map_prefix->width; ++x) {
        for (y = 0U; y < map_prefix->height; ++y) {
            double dx;
            double dy;
            double distance_sq;
            if (!shoparea[y + x * map_prefix->height]) {
                continue;
            }
            dx = (double)x - anchor_tile_x;
            dy = (double)y - anchor_tile_y;
            distance_sq = dx * dx + dy * dy;
            if (found && distance_sq >= best_distance_sq) {
                continue;
            }
            found = 1;
            best_distance_sq = distance_sq;
            windows_stash_shop_tile_center(x, y, &best_world_x, &best_world_y);
        }
    }
    if (!found) {
        return 0;
    }
    if (world_x_out != NULL) {
        *world_x_out = best_world_x;
    }
    if (world_y_out != NULL) {
        *world_y_out = best_world_y;
    }
    return 1;
}
static int windows_stash_find_nearest_walkable_tile(BmlWindowsPlacementMapPrefix* map_prefix, double anchor_world_x, double anchor_world_y, double* world_x_out, double* world_y_out)
{
    unsigned int x;
    unsigned int y;
    int found = 0;
    double best_distance_sq = 0.0;
    double best_world_x = 0.0;
    double best_world_y = 0.0;
    double anchor_tile_x = windows_stash_shop_world_to_tile(anchor_world_x);
    double anchor_tile_y = windows_stash_shop_world_to_tile(anchor_world_y);
    if (map_prefix == NULL || map_prefix->tiles == NULL || map_prefix->width == 0U || map_prefix->height == 0U) {
        return 0;
    }
    for (x = 1U; x + 1U < map_prefix->width; ++x) {
        for (y = 1U; y + 1U < map_prefix->height; ++y) {
            size_t base = (size_t)y * 3U + (size_t)x * 3U * (size_t)map_prefix->height;
            int32_t floor_tile = map_prefix->tiles[base];
            int32_t obstacle_tile = map_prefix->tiles[base + 1U];
            double dx;
            double dy;
            double distance_sq;
            if (floor_tile == 0 || obstacle_tile != 0) {
                continue;
            }
            dx = (double)x - anchor_tile_x;
            dy = (double)y - anchor_tile_y;
            distance_sq = dx * dx + dy * dy;
            if (found && distance_sq >= best_distance_sq) {
                continue;
            }
            found = 1;
            best_distance_sq = distance_sq;
            windows_stash_shop_tile_center(x, y, &best_world_x, &best_world_y);
        }
    }
    if (!found) {
        return 0;
    }
    if (world_x_out != NULL) {
        *world_x_out = best_world_x;
    }
    if (world_y_out != NULL) {
        *world_y_out = best_world_y;
    }
    return 1;
}
static int windows_stash_ensure_shoparea_for_map(BmlWindowsPlacementMapPrefix* map_prefix)
{
    bool* shoparea = windows_stash_shoparea_pointer();
    size_t cells;
    if (shoparea != NULL) {
        return 1;
    }
    if (g_windows_resolved_shoparea_symbol == NULL || map_prefix == NULL || map_prefix->width == 0U || map_prefix->height == 0U) {
        return 0;
    }
    cells = (size_t)map_prefix->width * (size_t)map_prefix->height;
    if (cells == 0U) {
        return 0;
    }
    g_windows_owned_shoparea = NULL;
    g_windows_owned_shoparea_cells = 0U;
    g_windows_owned_shoparea = (bool*)calloc(cells, sizeof(bool));
    if (g_windows_owned_shoparea == NULL) {
        return 0;
    }
    g_windows_owned_shoparea_cells = cells;
    memcpy(g_windows_resolved_shoparea_symbol, &g_windows_owned_shoparea, sizeof(g_windows_owned_shoparea));
    windows_append_stash_diagnostic_event("stash_access_point_step", "shoparea_allocated", map_prefix->name, 0, 0.0, 0.0, (int)cells);
    return 1;
}

static int windows_load_stash_inventory_if_needed(BmlBaronyList* inventory, char* error_code, size_t error_code_size, char* error_message, size_t error_message_size);
static int windows_stash_playable_place_chest_and_lid_at(void* map_argument, double chest_x, double chest_y, double chest_yaw, void** chest_out, void** lid_out)
{
    BmlBaronyList* entity_list = NULL;
    BmlBaronyList* void_inventory = NULL;
    void* chest = NULL;
    void* lid = NULL;
    windows_append_stash_diagnostic_event("stash_access_point_step", "before_map_entities", NULL, 1, chest_x, chest_y, -1);
    entity_list = windows_playable_get_map_entity_list(map_argument);
    g_windows_stash_playable_expected_entity_list = entity_list;
    windows_append_stash_diagnostic_event("stash_access_point_step", "after_map_entities", NULL, 1, (double)(uintptr_t)entity_list, (double)g_windows_stash_playable_entity_list_offset, -1);
    void_inventory = windows_stash_stats_void_chest_inventory();
    windows_append_stash_diagnostic_event("stash_access_point_attempt", NULL, NULL, 1, chest_x, chest_y, -1);
    int32_t chest_uid;
    int32_t lid_uid;
    double chest_z = 6.0;
    double lid_x = chest_x;
    double lid_y = chest_y;
    double lid_z = chest_z + BML_STASH_PLACEMENT_LID_OFFSET_Z;
    if (chest_out) *chest_out = NULL;
    if (lid_out) *lid_out = NULL;
    if (!g_windows_stash_playable_active || entity_list == NULL || g_windows_stash_new_entity_original == NULL || windows_stash_playable_is_multiplayer_client()) {
        return 0;
    }
    windows_append_stash_diagnostic_event("stash_access_point_step", "before_stats_inventory", NULL, 1, chest_x, chest_y, -1);
    if (void_inventory != NULL) {
        g_windows_stash_core_behavior_inventory = void_inventory;
    }
    windows_append_stash_diagnostic_event("stash_access_point_step", "after_stats_inventory", NULL, 1, chest_x, chest_y, -1);
    if (chest_yaw == BML_STASH_YAW_EAST) lid_x = chest_x - BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    else if (chest_yaw == BML_STASH_YAW_SOUTH) lid_y = chest_y - BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    else if (chest_yaw == BML_STASH_YAW_WEST) lid_x = chest_x + BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    else if (chest_yaw == BML_STASH_YAW_NORTH) lid_y = chest_y + BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    else lid_y = chest_y - BML_STASH_PLACEMENT_LID_HINGE_OFFSET;

    chest = windows_stash_call_new_entity_with_diagnostics("before_chest_new_entity", BML_STASH_SPRITE_CHEST_SPAWN, 1U, entity_list, NULL);
    if (chest != NULL) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "after_chest_new_entity", NULL, 1, chest_x, chest_y, -1);
        {
            void* behavior = windows_stash_act_chest_behavior_pointer();
            if (g_windows_stash_set_sprite_attributes_original != NULL) {
                g_windows_stash_set_sprite_attributes_original(chest, NULL, NULL);
            }
            windows_append_stash_diagnostic_event("stash_access_point_step", "after_chest_set_sprite", NULL, 1, chest_x, chest_y, -1);
            windows_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_X, chest_x);
            windows_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_Y, chest_y);
            windows_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_Z, chest_z);
            windows_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_YAW, chest_yaw);
            windows_entity_set_s32(chest, BML_STASH_ENTITY_OFFSET_SIZEX, 3);
            windows_entity_set_s32(chest, BML_STASH_ENTITY_OFFSET_SIZEY, 2);
            windows_entity_set_s32(chest, BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_CHEST_VOID_VISUAL);
            memcpy((unsigned char*)chest + BML_STASH_ENTITY_OFFSET_BEHAVIOR, &behavior, sizeof(behavior));
            windows_entity_set_skill(chest, 0, 1);
            windows_entity_set_skill(chest, 3, 9999);
            windows_entity_set_skill(chest, 8, 9999);
            windows_entity_set_skill(chest, 15, 9999);
            windows_entity_set_skill(chest, 4, 0);
            windows_entity_set_skill(chest, 10, 1);
            windows_entity_set_skill(chest, 12, 0);
            windows_entity_set_skill(chest, 17, BML_STASH_CHEST_VOID_STATE_PERMANENT);
            windows_entity_set_skill(chest, 58, BML_STASH_INTERNAL_MARKER_SKILL58);
        }
        windows_append_stash_diagnostic_event("stash_access_point_step", "chest_initialized", NULL, 1, chest_x, chest_y, -1);
    }

    lid = windows_stash_call_new_entity_with_diagnostics("before_lid_new_entity", BML_STASH_SPRITE_LID_SPAWN, 0U, entity_list, NULL);
    if (lid != NULL) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "after_lid_new_entity", NULL, 1, lid_x, lid_y, -1);
        {
            void* behavior = windows_stash_act_chest_lid_behavior_pointer();
            if (g_windows_stash_set_sprite_attributes_original != NULL) {
                g_windows_stash_set_sprite_attributes_original(lid, NULL, NULL);
            }
            windows_append_stash_diagnostic_event("stash_access_point_step", "after_lid_set_sprite", NULL, 1, lid_x, lid_y, -1);
            windows_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_X, lid_x);
            windows_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_Y, lid_y);
            windows_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_Z, lid_z);
            windows_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_YAW, chest_yaw);
            windows_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_FOCALX, 3.0);
            windows_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_FOCALZ, -0.75);
            windows_entity_set_s32(lid, BML_STASH_ENTITY_OFFSET_SIZEX, 2);
            windows_entity_set_s32(lid, BML_STASH_ENTITY_OFFSET_SIZEY, 2);
            windows_entity_set_s32(lid, BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_LID_VOID_VISUAL);
            memcpy((unsigned char*)lid + BML_STASH_ENTITY_OFFSET_BEHAVIOR, &behavior, sizeof(behavior));
            windows_entity_set_flag(lid, 12, 1);
            windows_entity_set_skill(lid, 58, BML_STASH_INTERNAL_MARKER_SKILL58);
        }
    }
    if (lid != NULL) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "before_parent_link", NULL, 1, lid_x, lid_y, -1);
    }

    if (chest != NULL && lid != NULL) {
        chest_uid = windows_entity_get_uid(chest);
        lid_uid = windows_entity_get_uid(lid);
        windows_entity_set_parent(lid, chest_uid);
        windows_entity_set_parent(chest, lid_uid);
        if (g_windows_stash_list_add_node_first != NULL) {
            BmlBaronyList* children = (BmlBaronyList*)((unsigned char*)chest + BML_STASH_ENTITY_OFFSET_CHILDREN);
            BmlBaronyNode* node = g_windows_stash_list_add_node_first(children);
            if (node != NULL) {
                node->element = NULL;
                node->deconstructor = g_windows_stash_empty_deconstructor;
            }
        }
        if (chest_out) *chest_out = chest;
        if (lid_out) *lid_out = lid;
        return 1;
    }
    return 0;
}

static int windows_stash_playable_try_place_lobby_chest_and_lid(void* map_argument)
{
    BmlWindowsPlacementMapPrefix* map_prefix = (BmlWindowsPlacementMapPrefix*)map_argument;
    int placed;
    windows_append_stash_diagnostic_event("stash_assign_actions_before_original", "lobby", map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
    if (g_windows_stash_assign_actions_original != NULL) {
        g_windows_stash_assign_actions_original(map_argument);
    }
    windows_append_stash_diagnostic_event("stash_assign_actions_after_original", "lobby", map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
    {
        double x_pos = BML_STASH_LOBBY_PLACEMENT_X;
        double y_pos = BML_STASH_LOBBY_PLACEMENT_Y;
        (void)windows_stash_find_nearest_walkable_tile(map_prefix, x_pos, y_pos, &x_pos, &y_pos);
        placed = windows_stash_playable_place_chest_and_lid_at(map_argument, x_pos, y_pos, BML_STASH_LOBBY_PLACEMENT_YAW, NULL, NULL);
        if (placed) {
            windows_append_stash_diagnostic_event("stash_access_point_created", "lobby", map_prefix ? map_prefix->name : NULL, 1, x_pos, y_pos, -1);
        }
    }
    return placed;
}

static int windows_stash_playable_try_place_shop_chest_and_lid(void* map_argument)
{
    BmlWindowsPlacementMapPrefix* map_prefix = (BmlWindowsPlacementMapPrefix*)map_argument;
    bool* shoparea = windows_stash_shoparea_pointer();
    unsigned int x;
    unsigned int y;
    void* chest = NULL;
    void* lid = NULL;
    if (!g_windows_stash_playable_active) {
        return 0;
    }
    if (g_windows_stash_playable_last_shop_map == map_argument && g_windows_stash_playable_last_placed_shop_chest != NULL) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "shop_already_placed", map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
        return 0;
    }
    if (map_prefix == NULL || map_prefix->width == 0U || map_prefix->height == 0U) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "shop_precondition_failed", map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
        return 0;
    }
    if (shoparea == NULL && windows_stash_ensure_shoparea_for_map(map_prefix)) {
        shoparea = windows_stash_shoparea_pointer();
    }
    if (shoparea == NULL) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "shoparea_missing", map_prefix->name, 0, 0.0, 0.0, -1);
        return 0;
    }
    for (x = 0U; x < map_prefix->width; ++x) {
        for (y = 0U; y < map_prefix->height; ++y) {
            if (shoparea[y + x * map_prefix->height]) {
                windows_append_stash_diagnostic_event("stash_access_point_step", "shop_tile_found", map_prefix->name, 1, (double)x * 16.0 + 8.0, (double)y * 16.0 + 8.0, -1);
                {
                    int placed = windows_stash_playable_place_chest_and_lid_at(map_argument, (double)x * 16.0 + 8.0, (double)y * 16.0 + 8.0, 0.0, &chest, &lid);
                    if (placed) {
                        g_windows_stash_playable_last_shop_map = map_argument;
                        g_windows_stash_playable_last_placed_shop_chest = chest;
                        g_windows_stash_playable_last_placed_shop_lid = lid;
                        windows_append_stash_diagnostic_event("stash_access_point_created", "shop", map_prefix->name, 1, (double)x * 16.0 + 8.0, (double)y * 16.0 + 8.0, -1);
                    } else {
                        windows_append_stash_diagnostic_event("stash_access_point_step", "shop_placement_failed", map_prefix->name, 1, (double)x * 16.0 + 8.0, (double)y * 16.0 + 8.0, -1);
                    }
                    return placed;
                }
            }
        }
    }
    windows_append_stash_diagnostic_event("stash_access_point_step", "shop_tile_not_found", map_prefix->name, 0, 0.0, 0.0, -1);
    {
        BmlBaronyList* creature_list = windows_playable_get_map_creature_list(map_argument);
        if (creature_list == NULL) {
            windows_append_stash_diagnostic_event("stash_access_point_step", "shop_creature_list_missing", map_prefix->name, 0, 0.0, 0.0, -1);
            return 0;
        }
        for (BmlBaronyNode* node = creature_list->first; node != NULL; node = node->next) {
            void* entity = node->element;
            int32_t sprite = 0;
            double x_pos = 0.0;
            double y_pos = 0.0;
            if (entity == NULL) {
                continue;
            }
            memcpy(&sprite, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_SPRITE, sizeof(sprite));
            if (sprite != 35) {
                continue;
            }
            memcpy(&x_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_X, sizeof(x_pos));
            memcpy(&y_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_Y, sizeof(y_pos));
            windows_append_stash_diagnostic_event("stash_access_point_step", "shop_marker_found", map_prefix->name, 1, x_pos, y_pos, sprite);
            {
                int placed = windows_stash_playable_place_chest_and_lid_at(map_argument, x_pos, y_pos, 0.0, &chest, &lid);
                if (placed) {
                    g_windows_stash_playable_last_shop_map = map_argument;
                    g_windows_stash_playable_last_placed_shop_chest = chest;
                    g_windows_stash_playable_last_placed_shop_lid = lid;
                    windows_append_stash_diagnostic_event("stash_access_point_created", "shop", map_prefix->name, 1, x_pos, y_pos, -1);
                } else {
                    windows_append_stash_diagnostic_event("stash_access_point_step", "shop_marker_placement_failed", map_prefix->name, 1, x_pos, y_pos, -1);
                }
                return placed;
            }
        }
        windows_append_stash_diagnostic_event("stash_access_point_step", "shop_marker_not_found", map_prefix->name, 0, 0.0, 0.0, -1);
        if (g_windows_stash_shopkeeper_spawn_seen) {
            double anchor_world_x = g_windows_stash_shopkeeper_spawn_x;
            double anchor_world_y = g_windows_stash_shopkeeper_spawn_y;
            double anchor_tile_x = windows_stash_shop_world_to_tile(anchor_world_x);
            double anchor_tile_y = windows_stash_shop_world_to_tile(anchor_world_y);
            double x_pos = 0.0;
            double y_pos = 0.0;
            int have_position = 0;
            g_windows_stash_shopkeeper_spawn_seen = 0;
            have_position = windows_stash_find_nearest_shoparea_tile(map_prefix, shoparea, anchor_world_x, anchor_world_y, &x_pos, &y_pos);
            if (!have_position &&
                shoparea == g_windows_owned_shoparea &&
                anchor_tile_x >= 0.0 &&
                anchor_tile_y >= 0.0 &&
                anchor_tile_x < (double)map_prefix->width &&
                anchor_tile_y < (double)map_prefix->height) {
                unsigned int tile_x = (unsigned int)anchor_tile_x;
                unsigned int tile_y = (unsigned int)anchor_tile_y;
                g_windows_owned_shoparea[tile_y + tile_x * map_prefix->height] = true;
                windows_stash_shop_tile_center(tile_x, tile_y, &x_pos, &y_pos);
                have_position = 1;
                windows_append_stash_diagnostic_event("stash_access_point_step", "shopkeeper_anchor_seeded_shoparea", map_prefix->name, 1, x_pos, y_pos, -1);
            }
            if (!have_position) {
                windows_append_stash_diagnostic_event("stash_access_point_step", "shopkeeper_anchor_no_shop_tile", map_prefix->name, 1, anchor_world_x, anchor_world_y, -1);
            } else {
                windows_append_stash_diagnostic_event("stash_access_point_step", "shopkeeper_anchor_used", map_prefix->name, 1, x_pos, y_pos, -1);
                {
                    int placed = windows_stash_playable_place_chest_and_lid_at(map_argument, x_pos, y_pos, 0.0, &chest, &lid);
                    if (placed) {
                        g_windows_stash_playable_last_shop_map = map_argument;
                        g_windows_stash_playable_last_placed_shop_chest = chest;
                        g_windows_stash_playable_last_placed_shop_lid = lid;
                        windows_append_stash_diagnostic_event("stash_access_point_created", "shop", map_prefix->name, 1, x_pos, y_pos, -1);
                    } else {
                        windows_append_stash_diagnostic_event("stash_access_point_step", "shopkeeper_anchor_placement_failed", map_prefix->name, 1, x_pos, y_pos, -1);
                    }
                    return placed;
                }
            }
        }
        {
            int sample_index = 0;
            BmlBaronyList* entity_list = windows_playable_get_map_entity_list(map_argument);
            BmlBaronyList* worldui_list = NULL;
            memcpy(&worldui_list, (unsigned char*)map_argument + g_windows_stash_playable_creature_list_offset + sizeof(void*), sizeof(worldui_list));
            for (BmlBaronyNode* node = creature_list->first; node != NULL && sample_index < 8; node = node->next, ++sample_index) {
                void* entity = node->element;
                int32_t sprite = 0;
                double x_pos = 0.0;
                double y_pos = 0.0;
                if (entity == NULL) {
                    continue;
                }
                memcpy(&sprite, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_SPRITE, sizeof(sprite));
                memcpy(&x_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_X, sizeof(x_pos));
                memcpy(&y_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_Y, sizeof(y_pos));
                windows_append_stash_diagnostic_event("stash_access_point_step", "shop_creature_sample", map_prefix->name, 1, x_pos, y_pos, sprite);
            }
            sample_index = 0;
            if (entity_list != NULL) {
                for (BmlBaronyNode* node = entity_list->first; node != NULL && sample_index < 8; node = node->next, ++sample_index) {
                    void* entity = node->element;
                    int32_t sprite = 0;
                    double x_pos = 0.0;
                    double y_pos = 0.0;
                    if (entity == NULL) {
                        continue;
                    }
                    memcpy(&sprite, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_SPRITE, sizeof(sprite));
                    memcpy(&x_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_X, sizeof(x_pos));
                    memcpy(&y_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_Y, sizeof(y_pos));
                    windows_append_stash_diagnostic_event("stash_access_point_step", "shop_entity_sample", map_prefix->name, 1, x_pos, y_pos, sprite);
                }
            }
            sample_index = 0;
            if (windows_is_plausible_list_pointer(worldui_list)) {
                for (BmlBaronyNode* node = worldui_list->first; node != NULL && sample_index < 8; node = node->next, ++sample_index) {
                    void* entity = node->element;
                    int32_t sprite = 0;
                    double x_pos = 0.0;
                    double y_pos = 0.0;
                    if (entity == NULL) {
                        continue;
                    }
                    memcpy(&sprite, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_SPRITE, sizeof(sprite));
                    memcpy(&x_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_X, sizeof(x_pos));
                    memcpy(&y_pos, (unsigned char*)entity + BML_STASH_ENTITY_OFFSET_Y, sizeof(y_pos));
                    windows_append_stash_diagnostic_event("stash_access_point_step", "shop_worldui_sample", map_prefix->name, 1, x_pos, y_pos, sprite);
                }
            }
        }
    }
    if (!windows_stash_playable_allows_walkable_shop_fallback(map_prefix->name)) {
        windows_append_stash_diagnostic_event("stash_access_point_step", "shop_walkable_fallback_skipped", map_prefix->name, 0, 0.0, 0.0, -1);
    } else {
        double x_pos = 0.0;
        double y_pos = 0.0;
        double anchor_x = ((double)map_prefix->width * 16.0) / 2.0;
        double anchor_y = ((double)map_prefix->height * 16.0) / 2.0;
        if (windows_stash_find_nearest_walkable_tile(map_prefix, anchor_x, anchor_y, &x_pos, &y_pos)) {
            int placed;
            windows_append_stash_diagnostic_event("stash_access_point_step", "shop_walkable_fallback_used", map_prefix->name, 1, x_pos, y_pos, -1);
            placed = windows_stash_playable_place_chest_and_lid_at(map_argument, x_pos, y_pos, 0.0, &chest, &lid);
            if (placed) {
                g_windows_stash_playable_last_shop_map = map_argument;
                g_windows_stash_playable_last_placed_shop_chest = chest;
                g_windows_stash_playable_last_placed_shop_lid = lid;
                windows_append_stash_diagnostic_event("stash_access_point_created", "shop", map_prefix->name, 1, x_pos, y_pos, -1);
            } else {
                windows_append_stash_diagnostic_event("stash_access_point_step", "shop_walkable_fallback_failed", map_prefix->name, 1, x_pos, y_pos, -1);
            }
            return placed;
        }
        windows_append_stash_diagnostic_event("stash_access_point_step", "shop_walkable_fallback_missing", map_prefix->name, 0, 0.0, 0.0, -1);
    }
    return 0;
}

static int windows_stash_resolve_inventory_functions(const wchar_t* hook_manifest, char* error_code, size_t error_code_size, char* error_message, size_t error_message_size)
{
    void* new_item_address = NULL;
    void* list_free_all_address = NULL;
    void* stats_symbol = NULL;
    if (!extract_resolved_symbol_address(hook_manifest, "new_item", &new_item_address) ||
        !extract_resolved_symbol_address(hook_manifest, "list_free_all", &list_free_all_address) ||
        !extract_resolved_symbol_address(hook_manifest, "stats", &stats_symbol)) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_SYMBOL_MISSING");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior requires resolved new_item, list_free_all, and stats symbols.");
        return -1;
    }
    memcpy(&g_windows_stash_new_item, &new_item_address, sizeof(g_windows_stash_new_item));
    memcpy(&g_windows_stash_list_free_all, &list_free_all_address, sizeof(g_windows_stash_list_free_all));
    g_windows_stash_stats_symbol = stats_symbol;
    return 0;
}

static int windows_configure_stash_core_behavior(const wchar_t* profile_dir, const wchar_t* hook_manifest, char* error_code, size_t error_code_size, char* error_message, size_t error_message_size)
{
    memset(g_windows_stash_state_dir_path, 0, sizeof(g_windows_stash_state_dir_path));
    memset(g_windows_stash_inventory_path, 0, sizeof(g_windows_stash_inventory_path));
    memset(g_windows_stash_diagnostics_path, 0, sizeof(g_windows_stash_diagnostics_path));
    memset(g_windows_stash_core_behavior_report_path, 0, sizeof(g_windows_stash_core_behavior_report_path));
    if (join_path_wide(g_windows_stash_state_dir_path, (DWORD)(sizeof(g_windows_stash_state_dir_path) / sizeof(g_windows_stash_state_dir_path[0])), profile_dir, BML_STASH_STATE_DIR_RELATIVE_PATH) != 0 ||
        join_path_wide(g_windows_stash_inventory_path, (DWORD)(sizeof(g_windows_stash_inventory_path) / sizeof(g_windows_stash_inventory_path[0])), profile_dir, BML_STASH_INVENTORY_RELATIVE_PATH) != 0 ||
        join_path_wide(g_windows_stash_diagnostics_path, (DWORD)(sizeof(g_windows_stash_diagnostics_path) / sizeof(g_windows_stash_diagnostics_path[0])), profile_dir, BML_STASH_DIAGNOSTICS_RELATIVE_PATH) != 0 ||
        join_path_wide(g_windows_stash_core_behavior_report_path, (DWORD)(sizeof(g_windows_stash_core_behavior_report_path) / sizeof(g_windows_stash_core_behavior_report_path[0])), profile_dir, BML_STASH_CORE_BEHAVIOR_REPORT_RELATIVE_PATH) != 0) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_PATH_TOO_LONG");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior state path exceeded MAX_PATH.");
        return -1;
    }
    if (windows_stash_resolve_inventory_functions(hook_manifest, error_code, error_code_size, error_message, error_message_size) != 0) {
        return -1;
    }
    g_windows_stash_core_behavior_active = true;
    g_windows_stash_core_behavior_loaded = false;
    g_windows_stash_core_behavior_dirty = false;
    g_windows_stash_core_behavior_loads = 0;
    g_windows_stash_core_behavior_saves = 0;
    g_windows_stash_core_behavior_dirty_marks = 0;
    g_windows_stash_core_behavior_inventory = NULL;
    return 0;
}

static void windows_mark_stash_inventory_dirty(void)
{
    if (g_windows_stash_core_behavior_active) {
        g_windows_stash_core_behavior_dirty = true;
        g_windows_stash_core_behavior_dirty_marks += 1;
    }
}

static int windows_load_stash_inventory_if_needed(BmlBaronyList* inventory, char* error_code, size_t error_code_size, char* error_message, size_t error_message_size)
{
    char* buffer;
    char* line;
    if (!g_windows_stash_core_behavior_active || inventory == NULL) return 0;
    if (g_windows_stash_core_behavior_loaded) return 0;
    if (g_windows_stash_list_free_all == NULL || g_windows_stash_new_item == NULL) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_SYMBOL_MISSING");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior cannot load inventory before list/newItem helpers resolve.");
        return -1;
    }
    g_windows_stash_list_free_all(inventory);
    inventory->first = NULL;
    inventory->last = NULL;
    buffer = read_small_ascii_file(g_windows_stash_inventory_path, 1024U * 1024U);
    if (buffer != NULL) {
        line = buffer;
        while (line && *line) {
            int type = 0, status = 0, beatitude = 0, count = 0, identified = 0;
            uint32_t appearance = 0U;
            char* next = strchr(line, '\n');
            if (next) {
                *next = '\0';
                ++next;
            }
            if (line[0] != '\0' && line[0] != '#') {
                if (sscanf(line, "%d %d %d %d %" SCNu32 " %d", &type, &status, &beatitude, &count, &appearance, &identified) == 6) {
                    if (count < 1) count = 1;
                    (void)g_windows_stash_new_item(type, status, (int16_t)beatitude, (int16_t)count, appearance, identified != 0, inventory);
                }
            }
            line = next;
        }
        HeapFree(GetProcessHeap(), 0, buffer);
    }
    g_windows_stash_core_behavior_inventory = inventory;
    g_windows_stash_core_behavior_loaded = true;
    g_windows_stash_core_behavior_dirty = false;
    g_windows_stash_core_behavior_loads += 1;
    windows_append_stash_diagnostic_event("stash_inventory_loaded", NULL, NULL, 0, 0.0, 0.0, (int)windows_stash_inventory_count(inventory));
    return 0;
}

static int windows_save_stash_inventory_if_dirty(char* error_code, size_t error_code_size, char* error_message, size_t error_message_size)
{
    HANDLE file;
    DWORD wrote = 0;
    const BmlBaronyNode* node;
    char line[128];
    if (!g_windows_stash_core_behavior_active || !g_windows_stash_core_behavior_dirty) return 0;
    if (g_windows_stash_core_behavior_inventory == NULL) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_INVENTORY_MISSING");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior had dirty state but no bound inventory list.");
        return -1;
    }
    if (mkdir_p_wide(g_windows_stash_state_dir_path) != 0) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_DIR_FAILED");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior could not create the Stash state directory.");
        return -1;
    }
    file = CreateFileW(g_windows_stash_inventory_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_WRITE_FAILED");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior could not open the Stash inventory state file for writing.");
        return -1;
    }
    if (!WriteFile(file, "# bml-stash-inventory-v1\n", 25U, &wrote, NULL)) {
        CloseHandle(file);
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_WRITE_FAILED");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior failed while writing the header row.");
        return -1;
    }
    for (node = g_windows_stash_core_behavior_inventory->first; node != NULL; node = node->next) {
        const BmlBaronyItem* item = (const BmlBaronyItem*)node->element;
        int len;
        if (!item) continue;
        len = snprintf(line, (int)sizeof(line), "%d %d %d %d %" PRIu32 " %d\n", item->type, item->status, (int)item->beatitude, (int)item->count, item->appearance, item->identified ? 1 : 0);
        if (len <= 0 || len >= (int)sizeof(line) || !WriteFile(file, line, (DWORD)len, &wrote, NULL)) {
            CloseHandle(file);
            copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_WRITE_FAILED");
            copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior failed while writing an inventory row.");
            return -1;
        }
    }
    if (!CloseHandle(file)) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_CLOSE_FAILED");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash core behavior failed while closing the Stash inventory state file.");
        return -1;
    }
    g_windows_stash_core_behavior_dirty = false;
    g_windows_stash_core_behavior_saves += 1;
    windows_append_stash_diagnostic_event("stash_inventory_saved", NULL, NULL, 0, 0.0, 0.0, (int)windows_stash_inventory_count(g_windows_stash_core_behavior_inventory));
    return 0;
}

static void append_uint(char** cursor, const char* end, unsigned int value)
{
    char digits[16];
    unsigned int count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value && count < (unsigned int)(sizeof(digits) / sizeof(digits[0])));
    while (count > 0U && *cursor < end) {
        **cursor = digits[--count];
        ++(*cursor);
    }
}

static void append_uintptr(char** cursor, const char* end, uintptr_t value)
{
    char digits[32];
    unsigned int count = 0;
    do {
        digits[count++] = (char)('0' + (value % (uintptr_t)10U));
        value /= (uintptr_t)10U;
    } while (value && count < (unsigned int)(sizeof(digits) / sizeof(digits[0])));
    while (count > 0U && *cursor < end) {
        **cursor = digits[--count];
        ++(*cursor);
    }
}

static int extract_manifest_codeview_identity(const wchar_t* path, char* guid_out, DWORD guid_count, DWORD* age_out)
{
    if (!file_exists(path) || !guid_out || guid_count < 37 || !age_out) return 0;
    guid_out[0] = '\0';
    *age_out = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 2 * 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }
    char* buffer = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart + 1);
    if (!buffer) {
        CloseHandle(file);
        return 0;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    buffer[read] = '\0';
    const char* code_view = find_ascii_token(buffer, "\"codeView\"");
    const char* guid_key = code_view ? find_ascii_token(code_view, "\"guid\": \"") : NULL;
    const char* age_key = code_view ? find_ascii_token(code_view, "\"age\":") : NULL;
    if (!guid_key || !age_key) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    const char* start = guid_key + 9;
    DWORD idx = 0;
    while (start[idx] && start[idx] != '"' && idx + 1 < guid_count) {
        char ch = start[idx];
        guid_out[idx] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
        ++idx;
    }
    guid_out[idx] = '\0';
    const char* colon = age_key + 6;
    while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n') ++colon;
    while (*colon >= '0' && *colon <= '9') {
        *age_out = *age_out * 10U + (DWORD)(*colon - '0');
        ++colon;
    }
    HeapFree(GetProcessHeap(), 0, buffer);
    return guid_out[0] != '\0' && *age_out != 0U;
}

static int map_rva_to_file_offset(const unsigned char* buffer, SIZE_T size, DWORD pe_offset, DWORD rva, DWORD* out_offset)
{
    WORD section_count = *(const WORD*)(buffer + pe_offset + 6U);
    WORD optional_size = *(const WORD*)(buffer + pe_offset + 20U);
    SIZE_T section_offset = (SIZE_T)pe_offset + 24U + optional_size;
    for (WORD index = 0; index < section_count; ++index) {
        SIZE_T offset = section_offset + (SIZE_T)index * 40U;
        if (offset + 40U > size) break;
        DWORD virtual_size = *(const DWORD*)(buffer + offset + 8U);
        DWORD virtual_address = *(const DWORD*)(buffer + offset + 12U);
        DWORD raw_size = *(const DWORD*)(buffer + offset + 16U);
        DWORD raw_pointer = *(const DWORD*)(buffer + offset + 20U);
        DWORD mapped_size = virtual_size > raw_size ? virtual_size : raw_size;
        if (rva >= virtual_address && rva < virtual_address + mapped_size) {
            *out_offset = raw_pointer + (rva - virtual_address);
            return *out_offset < size;
        }
    }
    return 0;
}

static void format_guid_bytes(const unsigned char* bytes, char* out, DWORD out_count)
{
    if (!bytes || !out || out_count < 37) {
        if (out && out_count) out[0] = '\0';
        return;
    }
    wsprintfA(
        out,
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8U) | ((unsigned int)bytes[2] << 16U) | ((unsigned int)bytes[3] << 24U),
        (unsigned int)bytes[4] | ((unsigned int)bytes[5] << 8U),
        (unsigned int)bytes[6] | ((unsigned int)bytes[7] << 8U),
        bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
    );
}

static int extract_executable_codeview_identity(const wchar_t* path, char* guid_out, DWORD guid_count, DWORD* age_out)
{
    if (!file_exists(path) || !guid_out || guid_count < 37 || !age_out) return 0;
    guid_out[0] = '\0';
    *age_out = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 512 * 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }
    unsigned char* buffer = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart);
    if (!buffer) {
        CloseHandle(file);
        return 0;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok || read < 0x100U || buffer[0] != 'M' || buffer[1] != 'Z') {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    DWORD pe_offset = *(const DWORD*)(buffer + 0x3cU);
    if ((SIZE_T)pe_offset + 24U > (SIZE_T)read || memcmp(buffer + pe_offset, "PE\0\0", 4) != 0) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    WORD optional_size = *(const WORD*)(buffer + pe_offset + 20U);
    if ((SIZE_T)pe_offset + 24U + optional_size > (SIZE_T)read) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    WORD magic = *(const WORD*)(buffer + pe_offset + 24U);
    DWORD data_directory_offset = pe_offset + 24U + (magic == 0x20b ? 0x70U : 0x60U);
    if ((SIZE_T)data_directory_offset + 56U > (SIZE_T)read) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    DWORD debug_rva = *(const DWORD*)(buffer + data_directory_offset + 48U);
    DWORD debug_size = *(const DWORD*)(buffer + data_directory_offset + 52U);
    DWORD debug_offset = 0;
    if (!debug_rva || !debug_size || !map_rva_to_file_offset(buffer, (SIZE_T)read, pe_offset, debug_rva, &debug_offset)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }
    for (DWORD offset = 0; offset + 28U <= debug_size; offset += 28U) {
        const unsigned char* entry = buffer + debug_offset + offset;
        DWORD debug_type = *(const DWORD*)(entry + 12U);
        DWORD size_of_data = *(const DWORD*)(entry + 16U);
        DWORD address_of_raw_data = *(const DWORD*)(entry + 20U);
        DWORD pointer_to_raw_data = *(const DWORD*)(entry + 24U);
        if (debug_type != 2U || size_of_data < 24U) continue;
        DWORD rsds_offset = pointer_to_raw_data;
        if (!rsds_offset && !map_rva_to_file_offset(buffer, (SIZE_T)read, pe_offset, address_of_raw_data, &rsds_offset)) {
            continue;
        }
        if ((SIZE_T)rsds_offset + size_of_data > (SIZE_T)read) continue;
        const unsigned char* blob = buffer + rsds_offset;
        if (memcmp(blob, "RSDS", 4) != 0) continue;
        format_guid_bytes(blob + 4U, guid_out, guid_count);
        *age_out = *(const DWORD*)(blob + 20U);
        HeapFree(GetProcessHeap(), 0, buffer);
        return guid_out[0] != '\0' && *age_out != 0U;
    }
    HeapFree(GetProcessHeap(), 0, buffer);
    return 0;
}

static int write_text_file(const wchar_t* path, const char* text, DWORD len)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(file, text, len, &written, NULL);
    CloseHandle(file);
    return ok && written == len;
}

static void write_abs_jump(unsigned char* destination, const void* target)
{
    uintptr_t absolute = (uintptr_t)target;
    destination[0] = 0xffU;
    destination[1] = 0x25U;
    destination[2] = 0x00U;
    destination[3] = 0x00U;
    destination[4] = 0x00U;
    destination[5] = 0x00U;
    for (unsigned int index = 0U; index < 8U; ++index) {
        destination[6U + index] = (unsigned char)((absolute >> (index * 8U)) & 0xffU);
    }
}
static void bml_write_abs_call(unsigned char* location, const void* destination)
{
    uintptr_t address = (uintptr_t)destination;
    location[0] = 0xffU;
    location[1] = 0x15U;
    location[2] = 0x02U;
    location[3] = 0x00U;
    location[4] = 0x00U;
    location[5] = 0x00U;
    location[6] = 0xebU;
    location[7] = 0x08U;
    for (unsigned int index = 0U; index < 8U; ++index) {
        location[8U + index] = (unsigned char)((address >> (index * 8U)) & 0xffU);
    }
}

static bool bml_instruction_is_short_jcc(const unsigned char* code, size_t offset)
{
    return bml_byte_is_short_relative_branch(code[offset]);
}

static bool bml_instruction_is_near_jcc(const unsigned char* code, size_t offset, size_t source_length)
{
    return source_length >= 2U && code[offset] == 0x0fU && code[offset + 1U] >= 0x80U && code[offset + 1U] <= 0x8fU;
}

static size_t bml_relocated_instruction_length(const unsigned char* code, size_t offset, size_t source_length)
{
    const unsigned char op = code[offset];
    if (op == 0xe8U) {
        return BML_DETOUR_PATCH_BYTES + 2U;
    }
    if (op == 0xe9U || op == 0xebU) {
        return BML_DETOUR_PATCH_BYTES;
    }
    if (bml_instruction_is_short_jcc(code, offset) || bml_instruction_is_near_jcc(code, offset, source_length)) {
        return BML_DETOUR_PATCH_BYTES + 2U;
    }
    return source_length;
}

static int bml_relative_target_offset(const unsigned char* code, size_t offset, size_t source_length, int64_t* out_target_offset)
{
    const unsigned char op = code[offset];
    if (op == 0xe8U || op == 0xe9U) {
        int32_t displacement = 0;
        if (source_length < 5U) {
            return -1;
        }
        memcpy(&displacement, code + offset + 1U, sizeof(displacement));
        *out_target_offset = (int64_t)offset + 5 + (int64_t)displacement;
        return 0;
    }
    if (op == 0xebU || bml_instruction_is_short_jcc(code, offset)) {
        const int8_t displacement = (int8_t)code[offset + 1U];
        if (source_length < 2U) {
            return -1;
        }
        *out_target_offset = (int64_t)offset + 2 + (int64_t)displacement;
        return 0;
    }
    if (bml_instruction_is_near_jcc(code, offset, source_length)) {
        int32_t displacement = 0;
        if (source_length < 6U) {
            return -1;
        }
        memcpy(&displacement, code + offset + 2U, sizeof(displacement));
        *out_target_offset = (int64_t)offset + 6 + (int64_t)displacement;
        return 0;
    }
    return -1;
}

static int bml_resolve_relocated_destination(const unsigned char* target_bytes, const BmlPatchInstruction* instructions, size_t instruction_count, size_t patch_size, const unsigned char* trampoline, int64_t target_offset, const void** out_destination)
{
    if (target_offset < 0) {
        *out_destination = (const void*)((uintptr_t)target_bytes + (uintptr_t)target_offset);
        return 0;
    }
    if ((uint64_t)target_offset < (uint64_t)patch_size) {
        for (size_t index = 0U; index < instruction_count; ++index) {
            if ((uint64_t)instructions[index].source_offset == (uint64_t)target_offset) {
                *out_destination = trampoline + instructions[index].relocated_offset;
                return 0;
            }
        }
        return -1;
    }
    *out_destination = target_bytes + target_offset;
    return 0;
}

static bool bml_find_rip_relative_displacement_offset(const unsigned char* code, size_t offset, size_t source_length, size_t* out_displacement_offset)
{
    const unsigned char op = code[offset];
    size_t opcode_length = 0U;
    size_t modrm_offset;
    size_t displacement_offset;
    unsigned char modrm;

    if (op == 0x0fU && source_length >= 3U && (code[offset + 1U] == 0x1fU || code[offset + 1U] == 0xb6U || code[offset + 1U] == 0xb7U)) {
        opcode_length = 2U;
    } else if (op == 0x66U && source_length >= 4U && code[offset + 1U] == 0x0fU && (code[offset + 2U] == 0x1fU || code[offset + 2U] == 0xefU)) {
        opcode_length = 3U;
    } else if (op >= 0x40U && op <= 0x4fU && source_length >= 3U && (bml_opcode_uses_supported_modrm(code[offset + 1U]) || bml_opcode_uses_supported_modrm_immediate(code[offset + 1U]))) {
        opcode_length = 2U;
    } else if (source_length >= 2U && (bml_opcode_uses_supported_modrm(op) || bml_opcode_uses_supported_modrm_immediate(op))) {
        opcode_length = 1U;
    } else {
        return false;
    }

    modrm_offset = offset + opcode_length;
    if (modrm_offset >= offset + source_length) {
        return false;
    }
    modrm = code[modrm_offset];
    if (!bml_modrm_uses_rip_relative(modrm)) {
        return false;
    }

    displacement_offset = modrm_offset + 1U;
    if (displacement_offset > SIZE_MAX - 4U || displacement_offset + 4U > offset + source_length) {
        return false;
    }

    *out_displacement_offset = displacement_offset;
    return true;
}

static int bml_adjust_rip_relative_displacement(const unsigned char* target_bytes, size_t source, size_t source_length, unsigned char* destination)
{
    size_t displacement_offset = 0U;
    size_t relocated_displacement_offset;
    int32_t old_displacement = 0;
    int64_t original_next;
    int64_t absolute_target;
    int64_t relocated_next;
    int64_t new_displacement64;
    int32_t new_displacement;

    if (!bml_find_rip_relative_displacement_offset(target_bytes, source, source_length, &displacement_offset)) {
        return 0;
    }

    relocated_displacement_offset = displacement_offset - source;
    memcpy(&old_displacement, target_bytes + displacement_offset, sizeof(old_displacement));
    original_next = (int64_t)(uintptr_t)(target_bytes + source + source_length);
    absolute_target = original_next + (int64_t)old_displacement;
    relocated_next = (int64_t)(uintptr_t)(destination + source_length);
    new_displacement64 = absolute_target - relocated_next;
    if (new_displacement64 < (int64_t)INT32_MIN || new_displacement64 > (int64_t)INT32_MAX) {
        return -1;
    }

    new_displacement = (int32_t)new_displacement64;
    memcpy(destination + relocated_displacement_offset, &new_displacement, sizeof(new_displacement));
    return 0;
}

static int bml_relocate_patch_window(const unsigned char* target_bytes, size_t patch_size, unsigned char* trampoline, size_t trampoline_capacity, size_t* out_trampoline_length)
{
    BmlPatchInstruction instructions[BML_DETOUR_MAX_INSTRUCTIONS];
    size_t instruction_count = 0U;
    size_t source_offset = 0U;
    size_t relocated_offset = 0U;

    memset(instructions, 0, sizeof(instructions));
    *out_trampoline_length = 0U;

    while (source_offset < patch_size) {
        size_t instruction_length = 0U;
        const char* decode_code = NULL;
        const char* decode_message = NULL;
        size_t relocated_length;
        (void)decode_code;
        (void)decode_message;

        if (instruction_count >= BML_DETOUR_MAX_INSTRUCTIONS) {
            return -1;
        }
        if (bml_decode_supported_x86_64_instruction(target_bytes, source_offset, patch_size, &instruction_length, &decode_code, &decode_message) != 0 ||
            instruction_length == 0U || source_offset + instruction_length > patch_size) {
            return -1;
        }

        relocated_length = bml_relocated_instruction_length(target_bytes, source_offset, instruction_length);
        if (relocated_offset + relocated_length + BML_DETOUR_PATCH_BYTES > trampoline_capacity) {
            return -1;
        }

        instructions[instruction_count].source_offset = source_offset;
        instructions[instruction_count].source_length = instruction_length;
        instructions[instruction_count].relocated_offset = relocated_offset;
        instructions[instruction_count].relocated_length = relocated_length;
        instruction_count += 1U;
        source_offset += instruction_length;
        relocated_offset += relocated_length;
    }

    for (size_t index = 0U; index < instruction_count; ++index) {
        const BmlPatchInstruction* instruction = &instructions[index];
        const size_t source = instruction->source_offset;
        unsigned char* destination = trampoline + instruction->relocated_offset;
        const unsigned char op = target_bytes[source];

        if (op == 0xe8U || op == 0xe9U || op == 0xebU || bml_instruction_is_short_jcc(target_bytes, source) || bml_instruction_is_near_jcc(target_bytes, source, instruction->source_length)) {
            int64_t target_offset = 0;
            const void* absolute_destination = NULL;
            if (bml_relative_target_offset(target_bytes, source, instruction->source_length, &target_offset) != 0 ||
                bml_resolve_relocated_destination(target_bytes, instructions, instruction_count, patch_size, trampoline, target_offset, &absolute_destination) != 0) {
                return -1;
            }
            if (op == 0xe8U) {
                bml_write_abs_call(destination, absolute_destination);
            } else if (op == 0xe9U || op == 0xebU) {
                write_abs_jump(destination, absolute_destination);
            } else {
                const unsigned char condition = (op == 0x0fU) ? (unsigned char)(target_bytes[source + 1U] & 0x0fU) : (unsigned char)(op & 0x0fU);
                destination[0] = (unsigned char)(0x70U | (condition ^ 0x01U));
                destination[1] = (unsigned char)BML_DETOUR_PATCH_BYTES;
                write_abs_jump(destination + 2U, absolute_destination);
            }
        } else {
            memcpy(destination, target_bytes + source, instruction->source_length);
            if (bml_adjust_rip_relative_displacement(target_bytes, source, instruction->source_length, destination) != 0) {
                return -1;
            }
        }
    }

    write_abs_jump(trampoline + relocated_offset, target_bytes + patch_size);
    *out_trampoline_length = relocated_offset + BML_DETOUR_PATCH_BYTES;
    return 0;
}

static void* bml_try_virtualalloc_trampoline_at(uintptr_t candidate, size_t size)
{
    void* requested;
    void* mapping;
    if (candidate < BML_DETOUR_MIN_MMAP_ADDRESS || size == 0U || (uintptr_t)size > UINTPTR_MAX - candidate) {
        return NULL;
    }
    requested = (void*)candidate;
    mapping = VirtualAlloc(requested, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (mapping != requested) {
        if (mapping) {
            VirtualFree(mapping, 0, MEM_RELEASE);
        }
        return NULL;
    }
    return mapping;
}

static void* bml_virtualalloc_trampoline_near_target(const void* target, size_t size)
{
    SYSTEM_INFO system_info;
    void* mapping;
    GetSystemInfo(&system_info);
    if (target != NULL) {
        const uintptr_t granularity = (uintptr_t)(system_info.dwAllocationGranularity ? system_info.dwAllocationGranularity : 0x10000U);
        const uintptr_t granularity_mask = granularity - 1U;
        const uintptr_t target_page = (uintptr_t)target & ~granularity_mask;
        uintptr_t step = BML_DETOUR_NEAR_SEARCH_STEP;
        uintptr_t distance;

        if (step < granularity) {
            step = granularity;
        }
        step = (step + granularity_mask) & ~granularity_mask;

        for (distance = step; distance <= BML_DETOUR_NEAR_SEARCH_RANGE; distance += step) {
            if (target_page >= distance) {
                mapping = bml_try_virtualalloc_trampoline_at(target_page - distance, size);
                if (mapping != NULL) {
                    return mapping;
                }
            }
            if (target_page <= UINTPTR_MAX - distance) {
                mapping = bml_try_virtualalloc_trampoline_at(target_page + distance, size);
                if (mapping != NULL) {
                    return mapping;
                }
            }
            if (BML_DETOUR_NEAR_SEARCH_RANGE - distance < step) {
                break;
            }
        }
    }

    return NULL;
}

static int bml_measure_supported_patch_window(const unsigned char* target_bytes, size_t minimum_patch_size, size_t* out_patch_size, const char** out_code, const char** out_message)
{
    size_t patch_size = 0U;
    const size_t required_patch_size = minimum_patch_size > BML_DETOUR_PATCH_BYTES ? minimum_patch_size : BML_DETOUR_PATCH_BYTES;
    *out_patch_size = 0U;
    *out_code = NULL;
    *out_message = NULL;

    while (patch_size < required_patch_size) {
        size_t instruction_length = 0U;
        const char* decode_code = NULL;
        const char* decode_message = NULL;

        if (bml_decode_supported_x86_64_instruction(target_bytes, patch_size, BML_DETOUR_MAX_COPY_BYTES, &instruction_length, &decode_code, &decode_message) != 0 ||
            instruction_length == 0U || patch_size + instruction_length > BML_DETOUR_MAX_COPY_BYTES) {
            *out_code = decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
            *out_message = decode_message != NULL ? decode_message : "Detour target prologue is not safe for the conservative decoder.";
            *out_patch_size = patch_size;
            return -1;
        }
        patch_size += instruction_length;
    }

    *out_patch_size = patch_size;
    return 0;
}

static int build_trampoline_bytes(const unsigned char* source, SIZE_T patch_size, unsigned char* trampoline, SIZE_T trampoline_capacity, SIZE_T* out_length)
{
    SIZE_T branch_offset = patch_size;
    unsigned char branch_cc = 0U;
    int32_t branch_disp = 0;
    for (SIZE_T offset = 0U; offset < patch_size; ++offset) {
        if (offset + 2U <= patch_size && (source[offset] == 0xebU || (source[offset] >= 0x70U && source[offset] <= 0x7fU))) {
            return 0;
        }
        if (offset + 6U <= patch_size && source[offset] == 0x0fU && (source[offset + 1U] >= 0x80U && source[offset + 1U] <= 0x8fU)) {
            if (branch_offset != patch_size) {
                return 0;
            }
            branch_offset = offset;
            branch_cc = (unsigned char)(source[offset + 1U] - 0x80U);
            memcpy(&branch_disp, source + offset + 2U, sizeof(branch_disp));
        }
        if (offset + 5U <= patch_size && (source[offset] == 0xe8U || source[offset] == 0xe9U)) {
            return 0;
        }
    }
    if (branch_offset == patch_size) {
        if (patch_size + 14U > trampoline_capacity) {
            return 0;
        }
        memcpy(trampoline, source, patch_size);
        write_abs_jump(trampoline + patch_size, source + patch_size);
        *out_length = patch_size + 14U;
        return 1;
    }
    {
        const SIZE_T before_length = branch_offset;
        const SIZE_T after_offset = branch_offset + 6U;
        const SIZE_T after_length = patch_size - after_offset;
        const SIZE_T total_length = before_length + 2U + 14U + after_length + 14U;
        const unsigned char inverse_short_jcc = (unsigned char)(0x70U | ((branch_cc ^ 1U) & 0x0fU));
        const int64_t target_offset = (int64_t)after_offset + (int64_t)branch_disp;
        const unsigned char* branch_target = NULL;
        SIZE_T cursor = 0U;
        if (total_length > trampoline_capacity) {
            return 0;
        }
        if (target_offset >= 0 && target_offset < (int64_t)patch_size) {
            if ((SIZE_T)target_offset < branch_offset) {
                branch_target = trampoline + (SIZE_T)target_offset;
            } else if ((SIZE_T)target_offset >= after_offset) {
                branch_target = trampoline + before_length + 2U + 14U + ((SIZE_T)target_offset - after_offset);
            } else {
                return 0;
            }
        } else {
            branch_target = source + target_offset;
        }
        memcpy(trampoline + cursor, source, before_length);
        cursor += before_length;
        trampoline[cursor++] = inverse_short_jcc;
        trampoline[cursor++] = 14U;
        write_abs_jump(trampoline + cursor, branch_target);
        cursor += 14U;
        memcpy(trampoline + cursor, source + after_offset, after_length);
        cursor += after_length;
        write_abs_jump(trampoline + cursor, source + patch_size);
        cursor += 14U;
        *out_length = cursor;
        return 1;
    }
}

static int __declspec(noinline) windows_detour_selftest_replacement(void)
{
    return 11;
}

static void* g_windows_fake_stash_trampoline = NULL;

static void* __declspec(noinline) windows_fake_stash_replacement(int player, void* item, bool forceNewStack, void* parent)
{
    uintptr_t original = g_windows_fake_stash_trampoline ? (uintptr_t)((void* (*)(int, void*, bool, void*))g_windows_fake_stash_trampoline)(player, item, forceNewStack, parent) : (uintptr_t)0;
    return (void*)(original + (uintptr_t)13U);
}
static void* windows_stash_add_item_to_chest_replacement(void* entity, void* item, bool force_new_stack, void* specific_destination_stack)
{
    BmlBaronyList* inventory = NULL;
    void* result = NULL;
    char error_code[256];
    char error_message[256];
    int stash_inventory;
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    stash_inventory = windows_stash_entity_uses_stats_void_chest(entity, &inventory);
    if (stash_inventory) {
        (void)windows_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message));
    }
    if (g_windows_stash_add_item_to_chest_original != NULL) {
        result = g_windows_stash_add_item_to_chest_original(entity, item, force_new_stack, specific_destination_stack);
    }
    if (g_windows_stash_core_behavior_active && stash_inventory && result != NULL) {
        windows_mark_stash_inventory_dirty();
    }
    return result;
}

static void* windows_stash_get_item_from_chest_replacement(void* entity, void* item, int amount, bool get_info_only)
{
    BmlBaronyList* inventory = NULL;
    void* result = NULL;
    char error_code[256];
    char error_message[256];
    int stash_inventory;
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    stash_inventory = windows_stash_entity_uses_stats_void_chest(entity, &inventory);
    if (stash_inventory) {
        (void)windows_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message));
    }
    if (g_windows_stash_get_item_from_chest_original != NULL) {
        result = g_windows_stash_get_item_from_chest_original(entity, item, amount, get_info_only);
    }
    if (g_windows_stash_core_behavior_active && stash_inventory && !get_info_only && result != NULL) {
        windows_mark_stash_inventory_dirty();
    }
    return result;
}

static void* windows_stash_add_item_to_void_chest_server_replacement(int player, void* item, bool force_new_stack, void* picked_up_stack)
{
    BmlBaronyList* inventory = windows_stash_stats_void_chest_inventory();
    void* result = NULL;
    char error_code[256];
    char error_message[256];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    if (inventory != NULL) {
        (void)windows_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message));
    }
    if (g_windows_stash_add_item_to_void_original != NULL) {
        result = g_windows_stash_add_item_to_void_original(player, item, force_new_stack, picked_up_stack);
    }
    if (g_windows_stash_core_behavior_active && result != NULL) {
        windows_mark_stash_inventory_dirty();
    }
    return result;
}

static void* windows_stash_get_chest_inventory_list_replacement(void* entity)
{
    void* inventory = NULL;
    char error_code[256];
    char error_message[256];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    if (g_windows_stash_get_inventory_original != NULL) {
        inventory = g_windows_stash_get_inventory_original(entity);
    }
    if (g_windows_stash_core_behavior_active && windows_stash_is_stats_void_chest_inventory(inventory)) {
        (void)windows_load_stash_inventory_if_needed((BmlBaronyList*)inventory, error_code, sizeof(error_code), error_message, sizeof(error_message));
    }
    return inventory;
}

static bool windows_stash_remove_item_from_void_chest_server_replacement(int player, void* item, int count)
{
    BmlBaronyList* inventory = windows_stash_stats_void_chest_inventory();
    bool removed = false;
    char error_code[256];
    char error_message[256];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    if (inventory != NULL) {
        (void)windows_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message));
    }
    if (g_windows_stash_remove_item_from_void_original != NULL) {
        removed = g_windows_stash_remove_item_from_void_original(player, item, count);
    }
    if (g_windows_stash_core_behavior_active && removed) {
        windows_mark_stash_inventory_dirty();
    }
    return removed;
}

static void windows_stash_close_chest_replacement(void* entity)
{
    char error_code[256];
    char error_message[256];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    if (g_windows_stash_close_chest_original != NULL) {
        g_windows_stash_close_chest_original(entity);
    }
    (void)windows_save_stash_inventory_if_dirty(error_code, sizeof(error_code), error_message, sizeof(error_message));
}

static void windows_stash_close_chest_server_replacement(void* entity)
{
    char error_code[256];
    char error_message[256];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    if (g_windows_stash_close_chest_server_original != NULL) {
        g_windows_stash_close_chest_server_original(entity);
    }
    (void)windows_save_stash_inventory_if_dirty(error_code, sizeof(error_code), error_message, sizeof(error_message));
}

static void* windows_stash_new_entity_replacement(int sprite, unsigned int pos, void* entity_list, void* creature_list)
{
    if (g_windows_stash_capture_new_entity_list && !g_windows_stash_playable_entity_list_logged) {
        g_windows_stash_playable_entity_list_logged = 1;
        g_windows_stash_playable_observed_entity_list = entity_list;
        g_windows_stash_capture_new_entity_list = 0;
        windows_append_stash_diagnostic_event("stash_access_point_step", "observed_new_entity_list", NULL, 1, (double)(uintptr_t)entity_list, (double)(uintptr_t)creature_list, sprite);
    }
    return g_windows_stash_new_entity_original != NULL ? g_windows_stash_new_entity_original(sprite, pos, entity_list, creature_list) : NULL;
}

static void windows_stash_set_sprite_attributes_replacement(void* entity, void* source, void* parent)
{
    if (g_windows_stash_set_sprite_attributes_original != NULL) {
        g_windows_stash_set_sprite_attributes_original(entity, source, parent);
    }
    if (g_windows_stash_playable_active && g_windows_stash_playable_pending_shop_map != NULL && !g_windows_stash_playable_shop_retry_active) {
        bool* shoparea = windows_stash_shoparea_pointer();
        if (shoparea != NULL) {
            void* pending_map = g_windows_stash_playable_pending_shop_map;
            g_windows_stash_playable_pending_shop_map = NULL;
            g_windows_stash_playable_shop_retry_active = 1;
            g_windows_stash_playable_shop_wait_logged = 0;
            windows_append_stash_diagnostic_event("stash_access_point_step", "shop_post_sprite_retry", NULL, 0, 0.0, 0.0, -1);
            (void)windows_stash_playable_try_place_shop_chest_and_lid(pending_map);
            g_windows_stash_playable_shop_retry_active = 0;
        } else if (!g_windows_stash_playable_shop_wait_logged) {
            g_windows_stash_playable_shop_wait_logged = 1;
            windows_append_stash_diagnostic_event("stash_access_point_step", "shop_post_sprite_waiting_for_shoparea", NULL, 0, 0.0, 0.0, -1);
        }
    }
}

static void windows_stash_assign_actions_replacement(void* map_argument)
{
    BmlWindowsPlacementMapPrefix* map_prefix = (BmlWindowsPlacementMapPrefix*)map_argument;
    int is_eligible = 0;
    if (g_windows_stash_playable_pending_shop_map != NULL && g_windows_stash_playable_pending_shop_map != map_argument) {
        g_windows_stash_playable_pending_shop_map = NULL;
    }
    windows_append_stash_diagnostic_event("stash_assign_actions_entered", NULL, map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
    if (g_windows_stash_playable_active) {
        if (map_prefix != NULL) {
            is_eligible = windows_stash_playable_is_start_map_name(map_prefix->name);
        }
        if (is_eligible) {
            windows_append_stash_diagnostic_event("stash_assign_actions_lobby_branch", NULL, map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
            g_windows_stash_capture_new_entity_list = 1;
            g_windows_stash_playable_entity_list_logged = 0;
            g_windows_stash_playable_observed_entity_list = NULL;
            (void)windows_stash_playable_try_place_lobby_chest_and_lid(map_argument);
            g_windows_stash_capture_new_entity_list = 0;
        } else {
            windows_append_stash_diagnostic_event("stash_assign_actions_before_original", "shop", map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
            if (g_windows_stash_assign_actions_original != NULL) {
                g_windows_stash_assign_actions_original(map_argument);
            }
            windows_append_stash_diagnostic_event("stash_assign_actions_after_original", "shop", map_prefix ? map_prefix->name : NULL, 0, 0.0, 0.0, -1);
            g_windows_stash_playable_pending_shop_map = NULL;
            g_windows_stash_playable_shop_retry_active = 1;
            if (!windows_stash_playable_try_place_shop_chest_and_lid(map_argument)) {
                g_windows_stash_playable_pending_shop_map = map_argument;
            }
            g_windows_stash_playable_shop_retry_active = 0;
        }
        return;
    }
    if (g_windows_stash_assign_actions_original != NULL) {
        g_windows_stash_assign_actions_original(map_argument);
    }
}
static int windows_stash_generate_dungeon_replacement(char* levelset, unsigned int seed, void* tuple_arg)
{
    int result = 0;
    windows_append_stash_diagnostic_event("stash_generate_dungeon_entered", NULL, levelset, 0, 0.0, 0.0, (int)seed);
    if (g_windows_stash_generate_dungeon_original != NULL) {
        result = g_windows_stash_generate_dungeon_original(levelset, seed, tuple_arg);
    }
    windows_append_stash_diagnostic_event("stash_generate_dungeon_after_original", NULL, levelset, 0, 0.0, 0.0, (int)seed);
    if (g_windows_stash_playable_active && g_windows_resolved_map_symbol != NULL) {
        g_windows_stash_playable_pending_shop_map = NULL;
        g_windows_stash_playable_shop_retry_active = 1;
        if (!windows_stash_playable_try_place_shop_chest_and_lid(g_windows_resolved_map_symbol)) {
            g_windows_stash_playable_pending_shop_map = g_windows_resolved_map_symbol;
        }
        g_windows_stash_playable_shop_retry_active = 0;
    }
    return result;
}
static void* windows_stash_summon_monster_no_smoke_replacement(int creature, int x, int y, bool forceLocation)
{
    uintptr_t image_base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t return_address = (uintptr_t)__builtin_return_address(0);
    DWORD return_rva = return_address >= image_base ? (DWORD)(return_address - image_base) : 0U;
    void* result = NULL;
    if (g_windows_stash_summon_no_smoke_original != NULL) {
        result = g_windows_stash_summon_no_smoke_original(creature, x, y, forceLocation);
    }
    if (g_windows_stash_playable_active && creature == 20 && forceLocation && return_rva == 5418932U) {
        g_windows_stash_shopkeeper_spawn_seen = 1;
        g_windows_stash_shopkeeper_spawn_x = (double)x;
        g_windows_stash_shopkeeper_spawn_y = (double)y;
        windows_append_stash_diagnostic_event("stash_access_point_step", "shopkeeper_spawn_seen", NULL, 1, (double)x, (double)y, creature);
        if (g_windows_stash_playable_pending_shop_map != NULL && !g_windows_stash_playable_shop_retry_active) {
            void* pending_map = g_windows_stash_playable_pending_shop_map;
            g_windows_stash_playable_pending_shop_map = NULL;
            g_windows_stash_playable_shop_retry_active = 1;
            g_windows_stash_playable_shop_wait_logged = 0;
            windows_append_stash_diagnostic_event("stash_access_point_step", "shop_post_summon_retry", NULL, 0, 0.0, 0.0, -1);
            if (!windows_stash_playable_try_place_shop_chest_and_lid(pending_map)) {
                g_windows_stash_playable_pending_shop_map = pending_map;
            }
            g_windows_stash_playable_shop_retry_active = 0;
        }
    }
    return result;
}

static void* g_windows_get_item_passthrough_trampoline = NULL;
static unsigned int g_windows_get_item_passthrough_calls = 0U;

static void* __declspec(noinline) windows_get_item_passthrough_replacement(void* entity, void* item, int amount, bool getInfoOnly)
{
    ++g_windows_get_item_passthrough_calls;
    return g_windows_get_item_passthrough_trampoline
        ? ((void* (*)(void*, void*, int, bool))g_windows_get_item_passthrough_trampoline)(entity, item, amount, getInfoOnly)
        : NULL;
}
static void* g_windows_add_item_void_probe_trampoline = NULL;
static unsigned int g_windows_add_item_void_probe_calls = 0U;

static void* __declspec(noinline) windows_add_item_void_probe_replacement(int player, void* item, bool forceNewStack, void* parent)
{
    ++g_windows_add_item_void_probe_calls;
    return g_windows_add_item_void_probe_trampoline
        ? ((void* (*)(int, void*, bool, void*))g_windows_add_item_void_probe_trampoline)(player, item, forceNewStack, parent)
        : NULL;
}
static void* g_windows_get_chest_list_probe_trampoline = NULL;
static unsigned int g_windows_get_chest_list_probe_calls = 0U;

static void* __declspec(noinline) windows_get_chest_list_probe_replacement(void* entity)
{
    ++g_windows_get_chest_list_probe_calls;
    return g_windows_get_chest_list_probe_trampoline
        ? ((void* (*)(void*))g_windows_get_chest_list_probe_trampoline)(entity)
        : NULL;
}
static void* g_windows_remove_item_void_probe_trampoline = NULL;
static unsigned int g_windows_remove_item_void_probe_calls = 0U;

static bool __declspec(noinline) windows_remove_item_void_probe_replacement(int player, void* item, int count)
{
    ++g_windows_remove_item_void_probe_calls;
    return g_windows_remove_item_void_probe_trampoline
        ? ((bool (*)(int, void*, int))g_windows_remove_item_void_probe_trampoline)(player, item, count)
        : false;
}
static void* g_windows_close_chest_server_probe_trampoline = NULL;
static unsigned int g_windows_close_chest_server_probe_calls = 0U;

static void windows_close_chest_server_probe_replacement(void* entity)
{
    ++g_windows_close_chest_server_probe_calls;
    if (g_windows_close_chest_server_probe_trampoline) {
        ((void (*)(void*))g_windows_close_chest_server_probe_trampoline)(entity);
    }
}

static void* g_windows_assign_actions_trampoline = NULL;
static unsigned int g_windows_assign_actions_calls = 0U;
static int g_windows_assign_actions_exit_on_fire = 0;
static int g_windows_assign_actions_code_view_match = 0;
static int g_windows_assign_actions_prologue_match = 0;
static DWORD g_windows_assign_actions_rva = 0U;
static DWORD g_windows_assign_actions_last_return_rva = 0U;
static wchar_t g_windows_assign_actions_report_path[MAX_PATH * 4];
static void* g_windows_assign_actions_map_argument;
static void* g_windows_assign_actions_global_map_symbol;
static int g_windows_assign_actions_map_argument_matches_global;
static char g_windows_assign_actions_map_name[33];
static unsigned int g_windows_assign_actions_map_width;
static unsigned int g_windows_assign_actions_map_height;
static unsigned int g_windows_assign_actions_map_skybox;
enum { BML_WINDOWS_ASSIGN_ACTIONS_RETURN_SAMPLE_LIMIT = 8 };
static DWORD g_windows_assign_actions_return_rvas[BML_WINDOWS_ASSIGN_ACTIONS_RETURN_SAMPLE_LIMIT];
static unsigned int g_windows_assign_actions_return_counts[BML_WINDOWS_ASSIGN_ACTIONS_RETURN_SAMPLE_LIMIT];
static unsigned int g_windows_assign_actions_return_sample_count = 0U;
static void record_windows_assign_actions_before(void* map_argument);
static void record_windows_assign_actions_after(void);
static void record_windows_new_entity_sample(int sprite, unsigned int pos, void* entity_list, void* creature_list, void* result);
static void record_windows_set_sprite_sample(void* entity, void* source, void* parent);
static int g_windows_placement_discovery_active;
static int g_windows_assign_actions_depth;
static void* g_windows_assign_actions_map_argument;
static int g_windows_assign_actions_new_entity_calls_before;
static int g_windows_assign_actions_set_sprite_calls_before;
static int g_windows_assign_actions_new_entity_delta_total;
static int g_windows_assign_actions_set_sprite_delta_total;
static unsigned int g_windows_set_sprite_calls;
static unsigned int g_windows_new_entity_calls;
static unsigned int g_windows_new_entity_sprite_188_calls;
static unsigned int g_windows_new_entity_sprite_1484_calls;
static unsigned int g_windows_new_entity_sprite_1790_calls;
static unsigned int g_windows_new_entity_sprite_1791_calls;
static int write_windows_placement_discovery_report(void);
static void clear_windows_probe_side_effects(void);

static int write_windows_assign_actions_probe_report(const char* status)
{
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"assign_actions\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-fired-probe\",\n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, g_windows_assign_actions_code_view_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, g_windows_assign_actions_rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, g_windows_assign_actions_prologue_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_assign_actions_calls);
    append_ascii(&cursor, end, ",\n  \"lastReturnRva\": ");
    append_uint(&cursor, end, g_windows_assign_actions_last_return_rva);
    append_ascii(&cursor, end, ",\n  \"returnSites\": [");
    for (unsigned int index = 0U; index < g_windows_assign_actions_return_sample_count; ++index) {
        append_ascii(&cursor, end, index == 0U ? "\n    {\"rva\": " : ",\n    {\"rva\": ");
        append_uint(&cursor, end, g_windows_assign_actions_return_rvas[index]);
        append_ascii(&cursor, end, ", \"count\": ");
        append_uint(&cursor, end, g_windows_assign_actions_return_counts[index]);
        append_ascii(&cursor, end, "}");
    }
    if (g_windows_assign_actions_return_sample_count > 0U) {
        append_ascii(&cursor, end, "\n  ");
    }
    append_ascii(&cursor, end, "],\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_assign_actions_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"map\": ");
    if (g_windows_assign_actions_map_argument != NULL) {
        append_ascii(&cursor, end, "{\"argument\": ");
        append_uintptr(&cursor, end, (uintptr_t)g_windows_assign_actions_map_argument);
        append_ascii(&cursor, end, ", \"globalMapSymbol\": ");
        append_uintptr(&cursor, end, (uintptr_t)g_windows_assign_actions_global_map_symbol);
        append_ascii(&cursor, end, ", \"argumentMatchesGlobal\": ");
        append_ascii(&cursor, end, g_windows_assign_actions_map_argument_matches_global ? "true" : "false");
        append_ascii(&cursor, end, ", \"name\": \"");
        append_ascii(&cursor, end, g_windows_assign_actions_map_name);
        append_ascii(&cursor, end, "\", \"width\": ");
        append_uint(&cursor, end, g_windows_assign_actions_map_width);
        append_ascii(&cursor, end, ", \"height\": ");
        append_uint(&cursor, end, g_windows_assign_actions_map_height);
        append_ascii(&cursor, end, ", \"skybox\": ");
        append_uint(&cursor, end, g_windows_assign_actions_map_skybox);
        append_ascii(&cursor, end, "}");
    } else {
        append_ascii(&cursor, end, "null");
    }
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_assign_actions_report_path, json, (DWORD)(cursor - json));
}

static void __declspec(noinline) windows_assign_actions_replacement(void* map_argument)
{
    uintptr_t image_base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t return_address = (uintptr_t)__builtin_return_address(0);
    ++g_windows_assign_actions_calls;
    if (return_address >= image_base) {
        g_windows_assign_actions_last_return_rva = (DWORD)(return_address - image_base);
    } else {
        g_windows_assign_actions_last_return_rva = 0U;
    }
    for (unsigned int index = 0U; index < g_windows_assign_actions_return_sample_count; ++index) {
        if (g_windows_assign_actions_return_rvas[index] == g_windows_assign_actions_last_return_rva) {
            ++g_windows_assign_actions_return_counts[index];
            goto assign_return_recorded;
        }
    }
    if (g_windows_assign_actions_return_sample_count < BML_WINDOWS_ASSIGN_ACTIONS_RETURN_SAMPLE_LIMIT) {
        g_windows_assign_actions_return_rvas[g_windows_assign_actions_return_sample_count] = g_windows_assign_actions_last_return_rva;
        g_windows_assign_actions_return_counts[g_windows_assign_actions_return_sample_count] = 1U;
        ++g_windows_assign_actions_return_sample_count;
    }
assign_return_recorded:
    record_windows_assign_actions_before(map_argument);
    if (g_windows_placement_discovery_active) {
        ++g_windows_assign_actions_depth;
    }
    if (g_windows_assign_actions_trampoline) {
        ((void (*)(void*))g_windows_assign_actions_trampoline)(map_argument);
    }
    if (g_windows_placement_discovery_active && g_windows_assign_actions_depth > 0) {
        --g_windows_assign_actions_depth;
    }
    record_windows_assign_actions_after();
    if (g_windows_placement_discovery_active) {
        (void)write_windows_placement_discovery_report();
    }
    if (g_windows_assign_actions_report_path[0]) {
        (void)write_windows_assign_actions_probe_report("fired");
    }
    if (g_windows_assign_actions_exit_on_fire) {
        ExitProcess(0);
    }
}

typedef struct BmlWindowsNewItemSample {
    int type;
    int status;
    int beatitude;
    int count;
    unsigned int appearance;
    int identified;
    int inventory_null;
} BmlWindowsNewItemSample;

static void* g_windows_new_item_trampoline = NULL;
static unsigned int g_windows_new_item_calls = 0U;
static unsigned int g_windows_new_item_prefix_index = 0U;
static int g_windows_new_item_exit_on_fire = 0;
static int g_windows_new_item_accept_any_fire = 0;
static wchar_t g_windows_new_item_report_path[MAX_PATH * 4];
static BmlWindowsNewItemSample g_windows_new_item_last_sample;

static int write_windows_new_item_probe_report(const char* status)
{
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"new_item\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"");
    append_ascii(&cursor, end, g_windows_new_item_accept_any_fire ? "real-barony-target-fired-probe-relaxed-prefix" : "real-barony-target-fired-probe");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_new_item_calls);
    append_ascii(&cursor, end, ",\n  \"prefixMatches\": ");
    append_uint(&cursor, end, g_windows_new_item_prefix_index);
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_new_item_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"lastSample\": { \"type\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_new_item_last_sample.type);
    append_ascii(&cursor, end, ", \"status\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_new_item_last_sample.status);
    append_ascii(&cursor, end, ", \"beatitude\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_new_item_last_sample.beatitude);
    append_ascii(&cursor, end, ", \"count\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_new_item_last_sample.count);
    append_ascii(&cursor, end, ", \"appearance\": ");
    append_uint(&cursor, end, g_windows_new_item_last_sample.appearance);
    append_ascii(&cursor, end, ", \"identified\": ");
    append_ascii(&cursor, end, g_windows_new_item_last_sample.identified ? "true" : "false");
    append_ascii(&cursor, end, ", \"inventoryNull\": ");
    append_ascii(&cursor, end, g_windows_new_item_last_sample.inventory_null ? "true" : "false");
    append_ascii(&cursor, end, " }");
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_new_item_report_path, json, (DWORD)(cursor - json));
}

static void* __declspec(noinline) windows_new_item_probe_replacement(int type, int status, short beatitude, short count, unsigned int appearance, bool identified, void* inventory)
{
    static const BmlWindowsNewItemSample expected_prefix[4] = {
        {96, 2, 0, 1, 0U, 1, 1},
        {10, 3, 0, 1, 0U, 1, 1},
        {0, 3, 0, 1, 1U, 1, 1},
        {44, 2, 0, 1, 0U, 1, 1},
    };
    BmlWindowsNewItemSample sample;
    void* result = NULL;
    sample.type = type;
    sample.status = status;
    sample.beatitude = beatitude;
    sample.count = count;
    sample.appearance = appearance;
    sample.identified = identified ? 1 : 0;
    sample.inventory_null = inventory == NULL ? 1 : 0;
    g_windows_new_item_last_sample = sample;
    ++g_windows_new_item_calls;
    if (g_windows_new_item_prefix_index < 4U) {
        const BmlWindowsNewItemSample* expected = &expected_prefix[g_windows_new_item_prefix_index];
        if (sample.type == expected->type &&
            sample.status == expected->status &&
            sample.beatitude == expected->beatitude &&
            sample.count == expected->count &&
            sample.appearance == expected->appearance &&
            sample.identified == expected->identified &&
            sample.inventory_null == expected->inventory_null) {
            ++g_windows_new_item_prefix_index;
        } else if (sample.type == expected_prefix[0].type &&
                   sample.status == expected_prefix[0].status &&
                   sample.beatitude == expected_prefix[0].beatitude &&
                   sample.count == expected_prefix[0].count &&
                   sample.appearance == expected_prefix[0].appearance &&
                   sample.identified == expected_prefix[0].identified &&
                   sample.inventory_null == expected_prefix[0].inventory_null) {
            g_windows_new_item_prefix_index = 1U;
        } else {
            g_windows_new_item_prefix_index = 0U;
        }
    }
    if (g_windows_new_item_trampoline) {
        result = ((void* (*)(int, int, short, short, unsigned int, bool, void*))g_windows_new_item_trampoline)(type, status, beatitude, count, appearance, identified, inventory);
    }
    if (g_windows_new_item_report_path[0]) {
        (void)write_windows_new_item_probe_report("fired");
        if (((g_windows_new_item_prefix_index >= 4U) || g_windows_new_item_accept_any_fire) && g_windows_new_item_exit_on_fire) {
            ExitProcess(0);
        }
    }
    return result;
}

typedef struct BmlWindowsNewEntitySample {
    unsigned int sprite;
    unsigned int pos;
    void* entity_list;
    void* creature_list;
    void* result;
} BmlWindowsNewEntitySample;
typedef struct BmlWindowsSetSpriteSample {
    void* entity;
    void* source;
    void* parent;
} BmlWindowsSetSpriteSample;


enum { BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT = 16, BML_WINDOWS_PLACEMENT_TRACK_LIMIT = 512 };

static int g_windows_placement_discovery_active = 0;
static int g_windows_assign_actions_depth = 0;
static void* g_windows_assign_actions_map_argument = NULL;
static void* g_windows_assign_actions_global_map_symbol = NULL;
static int g_windows_assign_actions_map_argument_matches_global = 0;
static char g_windows_assign_actions_map_name[33];
static unsigned int g_windows_assign_actions_map_width = 0U;
static unsigned int g_windows_assign_actions_map_height = 0U;
static unsigned int g_windows_assign_actions_map_skybox = 0U;
static int g_windows_assign_actions_new_entity_calls_before = 0;
static int g_windows_assign_actions_set_sprite_calls_before = 0;
static int g_windows_assign_actions_new_entity_delta_total = 0;
static int g_windows_assign_actions_set_sprite_delta_total = 0;
static BmlWindowsNewEntitySample g_windows_assign_actions_new_entity_samples[BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT];
static unsigned int g_windows_assign_actions_new_entity_sample_count = 0U;
static BmlWindowsSetSpriteSample g_windows_assign_actions_set_sprite_samples[BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT];
static unsigned int g_windows_assign_actions_set_sprite_sample_count = 0U;
static void* g_windows_assign_actions_new_entity_result_track[BML_WINDOWS_PLACEMENT_TRACK_LIMIT];
static unsigned int g_windows_assign_actions_new_entity_result_track_count = 0U;
static unsigned int g_windows_assign_actions_set_sprite_entity_match_count = 0U;
static BmlWindowsSetSpriteSample g_windows_set_sprite_last_sample;
static unsigned int g_windows_set_sprite_calls = 0U;
static BmlWindowsNewEntitySample g_windows_new_entity_samples[BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT];
static unsigned int g_windows_new_entity_sample_count = 0U;
static BmlWindowsSetSpriteSample g_windows_set_sprite_samples[BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT];
static unsigned int g_windows_set_sprite_sample_count = 0U;
static wchar_t g_windows_placement_report_path[MAX_PATH * 4];
static int g_windows_placement_exit_report_registered = 0;

static void reset_windows_placement_discovery_state(void)
{
    g_windows_placement_discovery_active = 0;
    g_windows_assign_actions_depth = 0;
    g_windows_assign_actions_map_argument = NULL;
    g_windows_assign_actions_global_map_symbol = NULL;
    g_windows_assign_actions_map_argument_matches_global = 0;
    memset(g_windows_assign_actions_map_name, 0, sizeof(g_windows_assign_actions_map_name));
    g_windows_assign_actions_map_width = 0U;
    g_windows_assign_actions_map_height = 0U;
    g_windows_assign_actions_map_skybox = 0U;
    g_windows_assign_actions_new_entity_calls_before = 0;
    g_windows_assign_actions_set_sprite_calls_before = 0;
    g_windows_assign_actions_new_entity_delta_total = 0;
    g_windows_assign_actions_set_sprite_delta_total = 0;
    memset(g_windows_assign_actions_new_entity_samples, 0, sizeof(g_windows_assign_actions_new_entity_samples));
    g_windows_assign_actions_new_entity_sample_count = 0U;
    memset(g_windows_assign_actions_set_sprite_samples, 0, sizeof(g_windows_assign_actions_set_sprite_samples));
    g_windows_assign_actions_set_sprite_sample_count = 0U;
    memset(g_windows_assign_actions_new_entity_result_track, 0, sizeof(g_windows_assign_actions_new_entity_result_track));
    g_windows_assign_actions_new_entity_result_track_count = 0U;
    g_windows_assign_actions_set_sprite_entity_match_count = 0U;
    memset(g_windows_new_entity_samples, 0, sizeof(g_windows_new_entity_samples));
    g_windows_new_entity_sample_count = 0U;
    memset(g_windows_set_sprite_samples, 0, sizeof(g_windows_set_sprite_samples));
    g_windows_set_sprite_sample_count = 0U;
    memset(&g_windows_set_sprite_last_sample, 0, sizeof(g_windows_set_sprite_last_sample));
    memset(g_windows_placement_report_path, 0, sizeof(g_windows_placement_report_path));
    g_windows_placement_exit_report_registered = 0;
    g_windows_assign_actions_calls = 0U;
    g_windows_new_entity_calls = 0U;
    g_windows_new_entity_sprite_188_calls = 0U;
    g_windows_new_entity_sprite_1484_calls = 0U;
    g_windows_new_entity_sprite_1790_calls = 0U;
    g_windows_new_entity_sprite_1791_calls = 0U;
    g_windows_set_sprite_calls = 0U;
    clear_windows_probe_side_effects();
    memset(g_windows_assign_actions_report_path, 0, sizeof(g_windows_assign_actions_report_path));
    g_windows_assign_actions_exit_on_fire = 0;
}

static void record_windows_assign_actions_before(void* map_argument)
{
    if (!g_windows_placement_discovery_active && g_windows_assign_actions_report_path[0] == L'\0') {
        return;
    }
    g_windows_assign_actions_map_argument = map_argument;
    g_windows_assign_actions_global_map_symbol = g_windows_resolved_map_symbol;
    g_windows_assign_actions_map_argument_matches_global = (map_argument != NULL && g_windows_assign_actions_global_map_symbol != NULL && map_argument == g_windows_assign_actions_global_map_symbol) ? 1 : 0;
    memset(g_windows_assign_actions_map_name, 0, sizeof(g_windows_assign_actions_map_name));
    g_windows_assign_actions_map_width = 0U;
    g_windows_assign_actions_map_height = 0U;
    g_windows_assign_actions_map_skybox = 0U;
    if (map_argument != NULL) {
        BmlWindowsPlacementMapPrefix* map_prefix = (BmlWindowsPlacementMapPrefix*)map_argument;
        memcpy(g_windows_assign_actions_map_name, map_prefix->name, 32U);
        g_windows_assign_actions_map_name[32] = '\0';
        g_windows_assign_actions_map_width = map_prefix->width;
        g_windows_assign_actions_map_height = map_prefix->height;
        g_windows_assign_actions_map_skybox = map_prefix->skybox;
    }
    g_windows_assign_actions_new_entity_calls_before = (int)g_windows_new_entity_calls;
    g_windows_assign_actions_set_sprite_calls_before = (int)g_windows_set_sprite_calls;
}

static void record_windows_assign_actions_after(void)
{
    if (!g_windows_placement_discovery_active) {
        return;
    }
    if ((int)g_windows_new_entity_calls > g_windows_assign_actions_new_entity_calls_before) {
        g_windows_assign_actions_new_entity_delta_total += (int)g_windows_new_entity_calls - g_windows_assign_actions_new_entity_calls_before;
    }
    if ((int)g_windows_set_sprite_calls > g_windows_assign_actions_set_sprite_calls_before) {
        g_windows_assign_actions_set_sprite_delta_total += (int)g_windows_set_sprite_calls - g_windows_assign_actions_set_sprite_calls_before;
    }
}

static void record_windows_new_entity_sample(int sprite, unsigned int pos, void* entity_list, void* creature_list, void* result)
{
    if (!g_windows_placement_discovery_active) {
        return;
    }
    if (g_windows_new_entity_sample_count < BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT) {
        BmlWindowsNewEntitySample* global_sample = &g_windows_new_entity_samples[g_windows_new_entity_sample_count++];
        global_sample->sprite = (unsigned int)sprite;
        global_sample->pos = pos;
        global_sample->entity_list = entity_list;
        global_sample->creature_list = creature_list;
        global_sample->result = result;
    }
    if (g_windows_assign_actions_depth > 0) {
        if (g_windows_assign_actions_new_entity_result_track_count < BML_WINDOWS_PLACEMENT_TRACK_LIMIT) {
            g_windows_assign_actions_new_entity_result_track[g_windows_assign_actions_new_entity_result_track_count++] = result;
        }
        if (g_windows_assign_actions_new_entity_sample_count < BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT) {
            BmlWindowsNewEntitySample* sample = &g_windows_assign_actions_new_entity_samples[g_windows_assign_actions_new_entity_sample_count++];
            sample->sprite = (unsigned int)sprite;
            sample->pos = pos;
            sample->entity_list = entity_list;
            sample->creature_list = creature_list;
            sample->result = result;
        }
    }
}

static void* g_windows_set_sprite_trampoline = NULL;
static int g_windows_set_sprite_exit_on_fire = 0;
static int g_windows_set_sprite_code_view_match = 0;
static int g_windows_set_sprite_prologue_match = 0;
static DWORD g_windows_set_sprite_rva = 0U;
static wchar_t g_windows_set_sprite_report_path[MAX_PATH * 4];

static void record_windows_set_sprite_sample(void* entity, void* source, void* parent)
{
    if (!g_windows_placement_discovery_active && g_windows_set_sprite_report_path[0] == L'\0') {
        return;
    }
    g_windows_set_sprite_last_sample.entity = entity;
    g_windows_set_sprite_last_sample.source = source;
    g_windows_set_sprite_last_sample.parent = parent;
    ++g_windows_set_sprite_calls;
    if (g_windows_set_sprite_sample_count < BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT) {
        BmlWindowsSetSpriteSample* global_sample = &g_windows_set_sprite_samples[g_windows_set_sprite_sample_count++];
        global_sample->entity = entity;
        global_sample->source = source;
        global_sample->parent = parent;
    }
    if (g_windows_placement_discovery_active) {
        for (unsigned int index = 0U; index < g_windows_assign_actions_new_entity_result_track_count; ++index) {
            if (g_windows_assign_actions_new_entity_result_track[index] == entity) {
                ++g_windows_assign_actions_set_sprite_entity_match_count;
                break;
            }
        }
    }
    if (g_windows_placement_discovery_active && g_windows_assign_actions_depth > 0 && g_windows_assign_actions_set_sprite_sample_count < BML_WINDOWS_PLACEMENT_SAMPLE_LIMIT) {
        BmlWindowsSetSpriteSample* sample = &g_windows_assign_actions_set_sprite_samples[g_windows_assign_actions_set_sprite_sample_count++];
        sample->entity = entity;
        sample->source = source;
        sample->parent = parent;
    }
}

static int write_windows_placement_discovery_report(void)
{
    char json[32768];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    int observed = g_windows_assign_actions_calls > 0U || g_windows_new_entity_calls > 0U || g_windows_set_sprite_calls > 0U;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"schemaVersion\": \"0.1.0\",\n");
    append_ascii(&cursor, end, "  \"test\": \"stash-placement-discovery\",\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, observed ? "observed" : "installed_no_calls");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"non-mutating-placement-context-only\",\n");
    append_ascii(&cursor, end, "  \"summary\": {\n    \"assignActionsCalls\": ");
    append_uint(&cursor, end, g_windows_assign_actions_calls);
    append_ascii(&cursor, end, ",\n    \"newEntityCalls\": ");
    append_uint(&cursor, end, g_windows_new_entity_calls);
    append_ascii(&cursor, end, ",\n    \"setSpriteAttributesCalls\": ");
    append_uint(&cursor, end, g_windows_set_sprite_calls);
    append_ascii(&cursor, end, ",\n    \"assignActionsNewEntityDelta\": ");
    append_uint(&cursor, end, (unsigned int)(g_windows_assign_actions_new_entity_delta_total < 0 ? 0 : g_windows_assign_actions_new_entity_delta_total));
    append_ascii(&cursor, end, ",\n    \"assignActionsSetSpriteAttributesDelta\": ");
    append_uint(&cursor, end, (unsigned int)(g_windows_assign_actions_set_sprite_delta_total < 0 ? 0 : g_windows_assign_actions_set_sprite_delta_total));
    append_ascii(&cursor, end, "\n  },\n");
    append_ascii(&cursor, end, "  \"assignActions\": {\n    \"observed\": ");
    append_ascii(&cursor, end, g_windows_assign_actions_calls > 0U ? "true" : "false");
    append_ascii(&cursor, end, ",\n    \"map\": ");
    if (g_windows_assign_actions_map_argument != NULL) {
        append_ascii(&cursor, end, "{\"argument\": ");
        append_uintptr(&cursor, end, (uintptr_t)g_windows_assign_actions_map_argument);
        append_ascii(&cursor, end, ", \"globalMapSymbol\": ");
        append_uintptr(&cursor, end, (uintptr_t)g_windows_assign_actions_global_map_symbol);
        append_ascii(&cursor, end, ", \"argumentMatchesGlobal\": ");
        append_ascii(&cursor, end, g_windows_assign_actions_map_argument_matches_global ? "true" : "false");
        append_ascii(&cursor, end, ", \"name\": \"");
        append_ascii(&cursor, end, g_windows_assign_actions_map_name);
        append_ascii(&cursor, end, "\", \"width\": ");
        append_uint(&cursor, end, g_windows_assign_actions_map_width);
        append_ascii(&cursor, end, ", \"height\": ");
        append_uint(&cursor, end, g_windows_assign_actions_map_height);
        append_ascii(&cursor, end, ", \"skybox\": ");
        append_uint(&cursor, end, g_windows_assign_actions_map_skybox);
        append_ascii(&cursor, end, "}");
    } else {
        append_ascii(&cursor, end, "null");
    }
    append_ascii(&cursor, end, ",\n    \"scopedNewEntitySampled\": ");
    append_uint(&cursor, end, g_windows_assign_actions_new_entity_sample_count);
    append_ascii(&cursor, end, ",\n    \"scopedNewEntitySamples\": [");
    for (unsigned int index = 0U; index < g_windows_assign_actions_new_entity_sample_count; ++index) {
        BmlWindowsNewEntitySample* sample = &g_windows_assign_actions_new_entity_samples[index];
        append_ascii(&cursor, end, index == 0U ? "\n      {" : ",\n      {");
        append_ascii(&cursor, end, "\"sprite\": ");
        append_uint(&cursor, end, sample->sprite);
        append_ascii(&cursor, end, ", \"pos\": ");
        append_uint(&cursor, end, sample->pos);
        append_ascii(&cursor, end, ", \"entityList\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->entity_list);
        append_ascii(&cursor, end, ", \"creatureList\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->creature_list);
        append_ascii(&cursor, end, ", \"result\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->result);
        append_ascii(&cursor, end, "}");
    }
    if (g_windows_assign_actions_new_entity_sample_count > 0U) {
        append_ascii(&cursor, end, "\n    ");
    }
    append_ascii(&cursor, end, "],\n    \"newEntitySetSpriteMatches\": ");
    append_uint(&cursor, end, g_windows_assign_actions_set_sprite_entity_match_count);
    append_ascii(&cursor, end, ",\n    \"scopedSetSpriteAttributesSampled\": ");
    append_uint(&cursor, end, g_windows_assign_actions_set_sprite_sample_count);
    append_ascii(&cursor, end, ",\n    \"scopedSetSpriteAttributesSamples\": [");
    for (unsigned int index = 0U; index < g_windows_assign_actions_set_sprite_sample_count; ++index) {
        BmlWindowsSetSpriteSample* sample = &g_windows_assign_actions_set_sprite_samples[index];
        append_ascii(&cursor, end, index == 0U ? "\n      {" : ",\n      {");
        append_ascii(&cursor, end, "\"entity\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->entity);
        append_ascii(&cursor, end, ", \"source\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->source);
        append_ascii(&cursor, end, ", \"parent\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->parent);
        append_ascii(&cursor, end, "}");
    }
    if (g_windows_assign_actions_set_sprite_sample_count > 0U) {
        append_ascii(&cursor, end, "\n    ");
    }
    append_ascii(&cursor, end, "]\n  },\n");
    append_ascii(&cursor, end, "  \"newEntity\": {\n    \"selectedSpriteCalls\": { \"188\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_188_calls);
    append_ascii(&cursor, end, ", \"1484\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_1484_calls);
    append_ascii(&cursor, end, ", \"1790\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_1790_calls);
    append_ascii(&cursor, end, ", \"1791\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_1791_calls);
    append_ascii(&cursor, end, " },\n    \"sampled\": ");
    append_uint(&cursor, end, g_windows_new_entity_sample_count);
    append_ascii(&cursor, end, ",\n    \"samples\": [");
    for (unsigned int index = 0U; index < g_windows_new_entity_sample_count; ++index) {
        BmlWindowsNewEntitySample* sample = &g_windows_new_entity_samples[index];
        append_ascii(&cursor, end, index == 0U ? "\n      {" : ",\n      {");
        append_ascii(&cursor, end, "\"sprite\": ");
        append_uint(&cursor, end, sample->sprite);
        append_ascii(&cursor, end, ", \"pos\": ");
        append_uint(&cursor, end, sample->pos);
        append_ascii(&cursor, end, ", \"entityList\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->entity_list);
        append_ascii(&cursor, end, ", \"creatureList\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->creature_list);
        append_ascii(&cursor, end, ", \"result\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->result);
        append_ascii(&cursor, end, "}");
    }
    if (g_windows_new_entity_sample_count > 0U) {
        append_ascii(&cursor, end, "\n    ");
    }
    append_ascii(&cursor, end, "]\n  },\n");
    append_ascii(&cursor, end, "  \"setSpriteAttributes\": {\n    \"sampled\": ");
    append_uint(&cursor, end, g_windows_set_sprite_sample_count);
    append_ascii(&cursor, end, ",\n    \"samples\": [");
    for (unsigned int index = 0U; index < g_windows_set_sprite_sample_count; ++index) {
        BmlWindowsSetSpriteSample* sample = &g_windows_set_sprite_samples[index];
        append_ascii(&cursor, end, index == 0U ? "\n      {" : ",\n      {");
        append_ascii(&cursor, end, "\"entity\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->entity);
        append_ascii(&cursor, end, ", \"source\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->source);
        append_ascii(&cursor, end, ", \"parent\": ");
        append_uintptr(&cursor, end, (uintptr_t)sample->parent);
        append_ascii(&cursor, end, "}");
    }
    if (g_windows_set_sprite_sample_count > 0U) {
        append_ascii(&cursor, end, "\n    ");
    }
    append_ascii(&cursor, end, "]\n  },\n");
    append_ascii(&cursor, end, "  \"notes\": [\n    \"Discovery mode only records call context and argument pointers from the access/placement replacements.\",\n    \"It does not spawn, modify, or claim any Stash access point.\"\n  ],\n");
    append_ascii(&cursor, end, "  \"reportedAt\": \"2026-07-04T00:00:00Z\"\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_placement_report_path, json, (DWORD)(cursor - json));
}

static void configure_windows_placement_discovery(const wchar_t* report_dir)
{
    reset_windows_placement_discovery_state();
    clear_windows_probe_side_effects();
    lstrcpynW(g_windows_placement_report_path, report_dir, (int)(sizeof(g_windows_placement_report_path) / sizeof(g_windows_placement_report_path[0])));
    lstrcatW(g_windows_placement_report_path, L"\\stash-placement-discovery-report.json");
}

static void activate_windows_placement_discovery(void)
{
    g_windows_placement_discovery_active = 1;
    (void)write_windows_placement_discovery_report();
}

static int write_windows_set_sprite_probe_report(const char* status)
{
    char json[2048];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"set_sprite_attributes\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-fired-probe\",\n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, g_windows_set_sprite_code_view_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, g_windows_set_sprite_rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, g_windows_set_sprite_prologue_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_set_sprite_calls);
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_set_sprite_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"lastSample\": { \"entity\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_set_sprite_last_sample.entity);
    append_ascii(&cursor, end, ", \"source\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_set_sprite_last_sample.source);
    append_ascii(&cursor, end, ", \"parent\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_set_sprite_last_sample.parent);
    append_ascii(&cursor, end, " }\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_set_sprite_report_path, json, (DWORD)(cursor - json));
}

static void __declspec(noinline) windows_set_sprite_probe_replacement(void* entity, void* source, void* parent)
{
    record_windows_set_sprite_sample(entity, source, parent);
    if (g_windows_set_sprite_trampoline) {
        ((void (*)(void*, void*, void*))g_windows_set_sprite_trampoline)(entity, source, parent);
    }
    if (g_windows_placement_discovery_active) {
        (void)write_windows_placement_discovery_report();
    }
    if (g_windows_set_sprite_report_path[0]) {
        (void)write_windows_set_sprite_probe_report("fired");
    }
    if (g_windows_set_sprite_exit_on_fire && g_windows_set_sprite_calls > 0U) {
        ExitProcess(0);
    }
}

static void* g_windows_new_entity_trampoline = NULL;
static unsigned int g_windows_new_entity_calls = 0U;
static unsigned int g_windows_new_entity_sprite_188_calls = 0U;
static unsigned int g_windows_new_entity_sprite_1484_calls = 0U;
static unsigned int g_windows_new_entity_sprite_1790_calls = 0U;
static unsigned int g_windows_new_entity_sprite_1791_calls = 0U;
static int g_windows_new_entity_exit_on_fire = 0;
static wchar_t g_windows_new_entity_report_path[MAX_PATH * 4];
static BmlWindowsNewEntitySample g_windows_new_entity_last_sample;

static int write_windows_new_entity_probe_report(const char* status)
{
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"new_entity\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-fired-probe\",\n");
    append_ascii(&cursor, end, "  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_new_entity_calls);
    append_ascii(&cursor, end, ",\n  \"selectedSpriteCalls\": { \"188\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_188_calls);
    append_ascii(&cursor, end, ", \"1484\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_1484_calls);
    append_ascii(&cursor, end, ", \"1790\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_1790_calls);
    append_ascii(&cursor, end, ", \"1791\": ");
    append_uint(&cursor, end, g_windows_new_entity_sprite_1791_calls);
    append_ascii(&cursor, end, " },\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_new_entity_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"lastSample\": { \"sprite\": ");
    append_uint(&cursor, end, g_windows_new_entity_last_sample.sprite);
    append_ascii(&cursor, end, ", \"pos\": ");
    append_uint(&cursor, end, g_windows_new_entity_last_sample.pos);
    append_ascii(&cursor, end, ", \"entityList\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_new_entity_last_sample.entity_list);
    append_ascii(&cursor, end, ", \"creatureList\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_new_entity_last_sample.creature_list);
    append_ascii(&cursor, end, ", \"result\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_new_entity_last_sample.result);
    append_ascii(&cursor, end, " }");
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_new_entity_report_path, json, (DWORD)(cursor - json));
}

static void* __declspec(noinline) windows_new_entity_probe_replacement(int sprite, unsigned int pos, void* entity_list, void* creature_list)
{
    void* result = NULL;
    g_windows_new_entity_last_sample.sprite = (unsigned int)sprite;
    g_windows_new_entity_last_sample.pos = pos;
    g_windows_new_entity_last_sample.entity_list = entity_list;
    g_windows_new_entity_last_sample.creature_list = creature_list;
    ++g_windows_new_entity_calls;
    switch (sprite) {
        case 188: ++g_windows_new_entity_sprite_188_calls; break;
        case 1484: ++g_windows_new_entity_sprite_1484_calls; break;
        case 1790: ++g_windows_new_entity_sprite_1790_calls; break;
        case 1791: ++g_windows_new_entity_sprite_1791_calls; break;
        default: break;
    }
    if (g_windows_new_entity_trampoline) {
        result = ((void* (*)(int, unsigned int, void*, void*))g_windows_new_entity_trampoline)(sprite, pos, entity_list, creature_list);
    }
    g_windows_new_entity_last_sample.result = result;
    record_windows_new_entity_sample(sprite, pos, entity_list, creature_list, result);
    if (g_windows_placement_discovery_active) {
        (void)write_windows_placement_discovery_report();
    }
    if (g_windows_new_entity_calls > 0U && g_windows_new_entity_report_path[0]) {
        (void)write_windows_new_entity_probe_report("fired");
    }
    if (g_windows_new_entity_exit_on_fire) {
        ExitProcess(0);
    }
    return result;
}

static void* g_windows_do_new_game_trampoline = NULL;
static unsigned int g_windows_do_new_game_calls = 0U;
static int g_windows_do_new_game_exit_on_fire = 0;
static int g_windows_do_new_game_last_make_highscore = -1;
static wchar_t g_windows_do_new_game_report_path[MAX_PATH * 4];
static int g_windows_do_new_game_code_view_match = 0;
static int g_windows_do_new_game_prologue_match = 0;
static DWORD g_windows_do_new_game_rva = 0U;

static int write_windows_do_new_game_probe_report(const char* status)
{
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"do_new_game\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-fired-probe\",\n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, g_windows_do_new_game_code_view_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, g_windows_do_new_game_rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, g_windows_do_new_game_prologue_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_do_new_game_calls);
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_do_new_game_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"shoppingSpreeState\": { \"currentMode\": ");
    {
        unsigned char* image_base = (unsigned char*)GetModuleHandleW(NULL);
        unsigned char* challenge_run = image_base ? image_base + BML_CHALLENGE_RUN_RVA : NULL;
        int* current_mode = image_base ? (int*)(image_base + BML_GAME_MODE_MANAGER_RVA) : NULL;
        append_uint(&cursor, end, current_mode ? (unsigned int)(*current_mode) : 0U);
        append_ascii(&cursor, end, ", \"inUse\": ");
        append_ascii(&cursor, end, (challenge_run && *(unsigned char*)(challenge_run + BML_CHALLENGE_RUN_INUSE_OFFSET)) ? "true" : "false");
        append_ascii(&cursor, end, ", \"eventType\": ");
        append_uint(&cursor, end, challenge_run ? (unsigned int)(*(int*)(challenge_run + BML_CHALLENGE_RUN_EVENTTYPE_OFFSET)) : 0U);
        append_ascii(&cursor, end, ", \"loadingsavegame\": ");
        append_uint(&cursor, end, image_base ? *(uint32_t*)(image_base + BML_LOADING_SAVEGAME_RVA) : 0U);
        append_ascii(&cursor, end, ", \"loadinglobbykey\": ");
        append_uint(&cursor, end, image_base ? *(uint32_t*)(image_base + BML_LOADING_LOBBYKEY_RVA) : 0U);
        append_ascii(&cursor, end, "}");
    }
    append_ascii(&cursor, end, ",\n  \"lastMakeHighscore\": ");
    if (g_windows_do_new_game_last_make_highscore < 0) {
        append_ascii(&cursor, end, "null");
    } else {
        append_ascii(&cursor, end, g_windows_do_new_game_last_make_highscore ? "true" : "false");
    }
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_do_new_game_report_path, json, (DWORD)(cursor - json));
}

static void clear_windows_probe_side_effects(void)
{
    memset(g_windows_assign_actions_report_path, 0, sizeof(g_windows_assign_actions_report_path));
    g_windows_assign_actions_exit_on_fire = 0;
    memset(g_windows_new_entity_report_path, 0, sizeof(g_windows_new_entity_report_path));
    g_windows_new_entity_exit_on_fire = 0;
    memset(g_windows_set_sprite_report_path, 0, sizeof(g_windows_set_sprite_report_path));
    g_windows_set_sprite_exit_on_fire = 0;
    g_windows_set_sprite_code_view_match = 0;
    g_windows_set_sprite_prologue_match = 0;
    g_windows_set_sprite_rva = 0U;
}

static void __declspec(noinline) windows_do_new_game_probe_replacement(bool makeHighscore)
{
    ++g_windows_do_new_game_calls;
    g_windows_do_new_game_last_make_highscore = makeHighscore ? 1 : 0;
    if (!makeHighscore && GetEnvironmentVariableW(L"BML_FORCE_SHOPPING_SPREE", NULL, 0) > 0) {
        force_windows_custom_run_shopping_spree();
    }
    if (g_windows_do_new_game_trampoline) {
        ((void (*)(bool))g_windows_do_new_game_trampoline)(makeHighscore);
    }
    if (g_windows_do_new_game_report_path[0]) {
        (void)write_windows_do_new_game_probe_report("fired");
    }
    if (g_windows_do_new_game_exit_on_fire) {
        ExitProcess(0);
    }
}
static void* g_windows_init_class_trampoline = NULL;
static unsigned int g_windows_init_class_calls = 0U;
static unsigned int g_windows_init_class_quickstart_calls = 0U;
static int g_windows_init_class_exit_on_fire = 0;
static int g_windows_init_class_last_player = -1;
static DWORD g_windows_init_class_last_return_rva = 0U;
static wchar_t g_windows_init_class_report_path[MAX_PATH * 4];
static int g_windows_init_class_code_view_match = 0;
static int g_windows_init_class_prologue_match = 0;
static DWORD g_windows_init_class_rva = 0U;

static int write_windows_init_class_probe_report(const char* status)
{
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"init_class\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-fired-probe\",\n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, g_windows_init_class_code_view_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, g_windows_init_class_rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, g_windows_init_class_prologue_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_init_class_calls);
    append_ascii(&cursor, end, ",\n  \"quickstartCalls\": ");
    append_uint(&cursor, end, g_windows_init_class_quickstart_calls);
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_init_class_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"lastPlayer\": ");
    if (g_windows_init_class_last_player < 0) {
        append_ascii(&cursor, end, "null");
    } else {
        append_uint(&cursor, end, (unsigned int)g_windows_init_class_last_player);
    }
    append_ascii(&cursor, end, ",\n  \"lastReturnRva\": ");
    append_uint(&cursor, end, g_windows_init_class_last_return_rva);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_init_class_report_path, json, (DWORD)(cursor - json));
}

static void __declspec(noinline) windows_init_class_probe_replacement(int player)
{
    uintptr_t image_base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t return_address = (uintptr_t)__builtin_return_address(0);
    ++g_windows_init_class_calls;
    g_windows_init_class_last_player = player;
    if (return_address >= image_base) {
        g_windows_init_class_last_return_rva = (DWORD)(return_address - image_base);
        if (g_windows_init_class_last_return_rva == 5032615U) {
            ++g_windows_init_class_quickstart_calls;
        }
    } else {
        g_windows_init_class_last_return_rva = 0U;
    }
    if (g_windows_init_class_trampoline) {
        ((void (*)(int))g_windows_init_class_trampoline)(player);
    }
    if (g_windows_init_class_report_path[0]) {
        (void)write_windows_init_class_probe_report("fired");
    }
    if (g_windows_init_class_exit_on_fire && g_windows_init_class_quickstart_calls > 0U) {
        ExitProcess(0);
    }
}
static void* g_windows_summon_probe_trampoline = NULL;
static unsigned int g_windows_summon_probe_calls = 0U;
static unsigned int g_windows_summon_shopkeeper_calls = 0U;
static int g_windows_summon_probe_exit_on_fire = 0;
static int g_windows_summon_probe_last_creature = -1;
static int g_windows_summon_probe_last_force_location = -1;
static DWORD g_windows_summon_probe_last_return_rva = 0U;
static wchar_t g_windows_summon_probe_report_path[MAX_PATH * 4];
static int g_windows_summon_probe_code_view_match = 0;
static int g_windows_summon_probe_prologue_match = 0;
static DWORD g_windows_summon_probe_rva = 0U;

static int write_windows_summon_probe_report(const char* status)
{
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, status);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"summon_monster_no_smoke\",\n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-fired-probe\",\n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, g_windows_summon_probe_code_view_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, g_windows_summon_probe_rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, g_windows_summon_probe_prologue_match ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_summon_probe_calls);
    append_ascii(&cursor, end, ",\n  \"shopkeeperCalls\": ");
    append_uint(&cursor, end, g_windows_summon_shopkeeper_calls);
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, g_windows_summon_probe_trampoline ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"lastCreature\": ");
    if (g_windows_summon_probe_last_creature < 0) {
        append_ascii(&cursor, end, "null");
    } else {
        append_uint(&cursor, end, (unsigned int)g_windows_summon_probe_last_creature);
    }
    append_ascii(&cursor, end, ",\n  \"lastForceLocation\": ");
    if (g_windows_summon_probe_last_force_location < 0) {
        append_ascii(&cursor, end, "null");
    } else {
        append_ascii(&cursor, end, g_windows_summon_probe_last_force_location ? "true" : "false");
    }
    append_ascii(&cursor, end, ",\n  \"lastReturnRva\": ");
    append_uint(&cursor, end, g_windows_summon_probe_last_return_rva);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(g_windows_summon_probe_report_path, json, (DWORD)(cursor - json));
}

static void* __declspec(noinline) windows_summon_probe_replacement(int creature, int x, int y, bool forceLocation)
{
    uintptr_t image_base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t return_address = (uintptr_t)__builtin_return_address(0);
    void* result = NULL;
    (void)x;
    (void)y;
    ++g_windows_summon_probe_calls;
    if (creature == 20) {
        ++g_windows_summon_shopkeeper_calls;
    }
    g_windows_summon_probe_last_creature = creature;
    g_windows_summon_probe_last_force_location = forceLocation ? 1 : 0;
    if (return_address >= image_base) {
        g_windows_summon_probe_last_return_rva = (DWORD)(return_address - image_base);
    } else {
        g_windows_summon_probe_last_return_rva = 0U;
    }
    if (g_windows_summon_probe_trampoline) {
        result = ((void* (*)(int, int, int, bool))g_windows_summon_probe_trampoline)(creature, x, y, forceLocation);
    }
    if (g_windows_summon_probe_report_path[0]) {
        (void)write_windows_summon_probe_report("fired");
    }
    if (g_windows_summon_probe_exit_on_fire && g_windows_summon_shopkeeper_calls > 0U && g_windows_summon_probe_last_return_rva == 5418932U) {
        ExitProcess(0);
    }
    return result;
}

static int install_simple_detour_sized_recorded(void* target, void* replacement, SIZE_T minimum_patch_size, void** out_trampoline, size_t* out_patch_size, const char** out_code, const char** out_message, unsigned char* out_original)
{
    const unsigned char* target_bytes = (const unsigned char*)target;
    const size_t trampoline_capacity = BML_DETOUR_MAX_RELOCATED_BYTES;
    unsigned char original[BML_DETOUR_MAX_COPY_BYTES];
    unsigned char* trampoline;
    size_t trampoline_length = 0U;
    size_t patch_size = 0U;
    const char* decode_code = NULL;
    const char* decode_message = NULL;
    DWORD old_protect = 0;

    if (out_patch_size) *out_patch_size = 0U;
    if (out_code) *out_code = NULL;
    if (out_message) *out_message = NULL;
    if (target == NULL || replacement == NULL || out_trampoline == NULL) {
        if (out_code) *out_code = "BML_WINDOWS_DETOUR_INVALID_ARGS";
        if (out_message) *out_message = "Detour installer received a null target, replacement, or trampoline output pointer.";
        return 0;
    }
    if (bml_measure_supported_patch_window(target_bytes, (size_t)minimum_patch_size, &patch_size, &decode_code, &decode_message) != 0) {
        if (out_patch_size) *out_patch_size = patch_size;
        if (out_code) *out_code = decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
        if (out_message) *out_message = decode_message != NULL ? decode_message : "Detour target prologue is not safe for the conservative decoder.";
        return 0;
    }
    if (out_patch_size) *out_patch_size = patch_size;
    if (out_original) {
        memcpy(out_original, target, patch_size);
    }
    memcpy(original, target, patch_size);
    trampoline = (unsigned char*)bml_virtualalloc_trampoline_near_target(target, trampoline_capacity);
    if (!trampoline) {
        trampoline = (unsigned char*)VirtualAlloc(NULL, trampoline_capacity, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!trampoline) {
        if (out_code) *out_code = "BML_WINDOWS_DETOUR_TRAMPOLINE_ALLOC_FAILED";
        if (out_message) *out_message = "Detour installer could not allocate executable trampoline memory.";
        return 0;
    }
    if (bml_relocate_patch_window(target_bytes, patch_size, trampoline, trampoline_capacity, &trampoline_length) != 0) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        if (out_code) *out_code = "BML_WINDOWS_DETOUR_RELOCATE_FAILED";
        if (out_message) *out_message = "Detour installer could not relocate the decoded patch window into the trampoline.";
        return 0;
    }
    FlushInstructionCache(GetCurrentProcess(), trampoline, trampoline_length);
    if (!VirtualProtect(target, patch_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        if (out_code) *out_code = "BML_WINDOWS_DETOUR_PROTECT_FAILED";
        if (out_message) *out_message = "Detour installer could not make the target patch window writable.";
        return 0;
    }
    write_abs_jump((unsigned char*)target, replacement);
    if (patch_size > BML_DETOUR_PATCH_BYTES) {
        memset((unsigned char*)target + BML_DETOUR_PATCH_BYTES, 0x90, patch_size - BML_DETOUR_PATCH_BYTES);
    }
    FlushInstructionCache(GetCurrentProcess(), target, patch_size);
    {
        DWORD ignored = 0;
        if (!VirtualProtect(target, patch_size, old_protect, &ignored)) {
            memcpy(target, original, patch_size);
            FlushInstructionCache(GetCurrentProcess(), target, patch_size);
            VirtualFree(trampoline, 0, MEM_RELEASE);
            if (out_code) *out_code = "BML_WINDOWS_DETOUR_REPROTECT_FAILED";
            if (out_message) *out_message = "Detour installer could not restore the target page protection after patching.";
            return 0;
        }
    }
    if (out_code) *out_code = "BML_WINDOWS_DETOUR_INSTALLED";
    if (out_message) *out_message = "Detour installer patched the target and built a trampoline successfully.";
    *out_trampoline = trampoline;
    return 1;
}

static int install_simple_detour_sized(void* target, void* replacement, SIZE_T minimum_patch_size, void** out_trampoline)
{
    return install_simple_detour_sized_recorded(target, replacement, minimum_patch_size, out_trampoline, NULL, NULL, NULL, NULL);
}

static int install_simple_detour(void* target, void* replacement, void** out_trampoline)
{
    return install_simple_detour_sized(target, replacement, BML_DETOUR_PATCH_BYTES, out_trampoline);
}

static int run_windows_detour_selftest(const wchar_t* report_dir)
{
    enum { TARGET_SIZE = 64 };
    unsigned char* target = (unsigned char*)VirtualAlloc(NULL, TARGET_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    unsigned char* call_target = (unsigned char*)VirtualAlloc(NULL, TARGET_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    unsigned char* rip_target = (unsigned char*)VirtualAlloc(NULL, TARGET_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    void* trampoline = NULL;
    void* call_trampoline = NULL;
    void* rip_trampoline = NULL;
    int before = -1;
    int after = -1;
    int through_trampoline = -1;
    int call_before = -1;
    int call_after = -1;
    int call_through_trampoline = -1;
    int rip_before = -1;
    int rip_after = -1;
    int rip_through_trampoline = -1;
    int installed = 0;
    int call_installed = 0;
    int rip_installed = 0;
    wchar_t path[MAX_PATH * 4];
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;

    if (!target || !call_target || !rip_target) {
        if (target) {
            VirtualFree(target, 0, MEM_RELEASE);
        }
        if (call_target) {
            VirtualFree(call_target, 0, MEM_RELEASE);
        }
        if (rip_target) {
            VirtualFree(rip_target, 0, MEM_RELEASE);
        }
        return 0;
    }
    for (unsigned int index = 0U; index < TARGET_SIZE; ++index) {
        target[index] = 0x90U;
    }
    target[14] = 0x90U;
    target[15] = 0x90U;
    target[16] = 0xb8U;
    target[17] = 0x07U;
    target[18] = 0x00U;
    target[19] = 0x00U;
    target[20] = 0x00U;
    target[21] = 0xc3U;
    FlushInstructionCache(GetCurrentProcess(), target, TARGET_SIZE);

    before = ((int (*)(void))target)();
    installed = install_simple_detour(target, (void*)windows_detour_selftest_replacement, &trampoline);
    after = ((int (*)(void))target)();
    if (trampoline) {
        through_trampoline = ((int (*)(void))trampoline)();
    }

    for (unsigned int index = 0U; index < TARGET_SIZE; ++index) {
        call_target[index] = 0x90U;
    }
    call_target[0] = 0xb8U;
    call_target[1] = 0x01U;
    call_target[2] = 0x00U;
    call_target[3] = 0x00U;
    call_target[4] = 0x00U;
    call_target[5] = 0xe8U;
    {
        int32_t disp = 5;
        memcpy(call_target + 6U, &disp, sizeof(disp));
    }
    call_target[10] = 0x83U;
    call_target[11] = 0xc0U;
    call_target[12] = 0x03U;
    call_target[13] = 0x90U;
    call_target[14] = 0xc3U;
    call_target[15] = 0xb8U;
    call_target[16] = 0x04U;
    call_target[17] = 0x00U;
    call_target[18] = 0x00U;
    call_target[19] = 0x00U;
    call_target[20] = 0xc3U;
    FlushInstructionCache(GetCurrentProcess(), call_target, TARGET_SIZE);
    call_before = ((int (*)(void))call_target)();
    call_installed = install_simple_detour(call_target, (void*)windows_detour_selftest_replacement, &call_trampoline);
    call_after = ((int (*)(void))call_target)();
    if (call_trampoline) {
        call_through_trampoline = ((int (*)(void))call_trampoline)();
    }
    for (unsigned int index = 0U; index < TARGET_SIZE; ++index) {
        rip_target[index] = 0x90U;
    }
    rip_target[8] = 0x8bU;
    rip_target[9] = 0x05U;
    {
        int32_t disp = 4;
        memcpy(rip_target + 10U, &disp, sizeof(disp));
    }
    rip_target[14] = 0x83U;
    rip_target[15] = 0xc0U;
    rip_target[16] = 0x03U;
    rip_target[17] = 0xc3U;
    rip_target[18] = 0x04U;
    rip_target[19] = 0x00U;
    rip_target[20] = 0x00U;
    rip_target[21] = 0x00U;
    FlushInstructionCache(GetCurrentProcess(), rip_target, TARGET_SIZE);
    rip_before = ((int (*)(void))rip_target)();
    rip_installed = install_simple_detour(rip_target, (void*)windows_detour_selftest_replacement, &rip_trampoline);
    rip_after = ((int (*)(void))rip_target)();
    if (rip_trampoline) {
        rip_through_trampoline = ((int (*)(void))rip_trampoline)();
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-detour-self-test-report.json");

    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, installed && before == 7 && after == 11 && through_trampoline == 7 && call_installed && call_before == 7 && call_after == 11 && call_through_trampoline == 7 && rip_installed && rip_before == 7 && rip_after == 11 && rip_through_trampoline == 7 ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"synthetic-windows-x64-absolute-jump\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"detour-substrate-self-test-only\", \n");
    append_ascii(&cursor, end, "  \"before\": ");
    append_uint(&cursor, end, (unsigned int)(before < 0 ? 0 : before));
    append_ascii(&cursor, end, ",\n  \"after\": ");
    append_uint(&cursor, end, (unsigned int)(after < 0 ? 0 : after));
    append_ascii(&cursor, end, ",\n  \"trampoline\": ");
    append_uint(&cursor, end, (unsigned int)(through_trampoline < 0 ? 0 : through_trampoline));
    append_ascii(&cursor, end, ",\n  \"callRelocated\": { \"before\": ");
    append_uint(&cursor, end, (unsigned int)(call_before < 0 ? 0 : call_before));
    append_ascii(&cursor, end, ", \"after\": ");
    append_uint(&cursor, end, (unsigned int)(call_after < 0 ? 0 : call_after));
    append_ascii(&cursor, end, ", \"trampoline\": ");
    append_uint(&cursor, end, (unsigned int)(call_through_trampoline < 0 ? 0 : call_through_trampoline));
    append_ascii(&cursor, end, ", \"installed\": ");
    append_ascii(&cursor, end, call_installed ? "true" : "false");
    append_ascii(&cursor, end, " }");
    append_ascii(&cursor, end, ",\n  \"ripRelocated\": { \"before\": ");
    append_uint(&cursor, end, (unsigned int)(rip_before < 0 ? 0 : rip_before));
    append_ascii(&cursor, end, ", \"after\": ");
    append_uint(&cursor, end, (unsigned int)(rip_after < 0 ? 0 : rip_after));
    append_ascii(&cursor, end, ", \"trampoline\": ");
    append_uint(&cursor, end, (unsigned int)(rip_through_trampoline < 0 ? 0 : rip_through_trampoline));
    append_ascii(&cursor, end, ", \"installed\": ");
    append_ascii(&cursor, end, rip_installed ? "true" : "false");
    append_ascii(&cursor, end, " }");
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';

    write_text_file(path, json, (DWORD)(cursor - json));
    if (trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    if (call_trampoline) {
        VirtualFree(call_trampoline, 0, MEM_RELEASE);
    }
    if (rip_trampoline) {
        VirtualFree(rip_trampoline, 0, MEM_RELEASE);
    }
    VirtualFree(target, 0, MEM_RELEASE);
    VirtualFree(call_target, 0, MEM_RELEASE);
    VirtualFree(rip_target, 0, MEM_RELEASE);
    return installed && before == 7 && after == 11 && through_trampoline == 7 && call_installed && call_before == 7 && call_after == 11 && call_through_trampoline == 7 && rip_installed && rip_before == 7 && rip_after == 11 && rip_through_trampoline == 7;
}

static int run_windows_fake_stash_selftest(const wchar_t* report_dir)
{
    wchar_t provider_path[MAX_PATH * 4];
    wchar_t path[MAX_PATH * 4];
    HMODULE provider = NULL;
    void* target = NULL;
    void* trampoline = NULL;
    uintptr_t before = 0;
    uintptr_t after = 0;
    uintptr_t through_trampoline = 0;
    int installed = 0;
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    int fake_player = 2;
    void* fake_item = (void*)(uintptr_t)0x100000003ULL;
    bool fake_update = true;
    void* fake_parent = (void*)(uintptr_t)0x200000005ULL;

    get_env_wide(L"BML_WINDOWS_FAKE_PROVIDER_DLL", provider_path, (DWORD)(sizeof(provider_path) / sizeof(provider_path[0])));
    if (provider_path[0]) {
        provider = LoadLibraryW(provider_path);
    }
    if (provider) {
        target = (void*)GetProcAddress(provider, "BmlFakeAddItemToVoidChestServer");
    }
    if (target) {
        before = (uintptr_t)((void* (*)(int, void*, bool, void*))target)(fake_player, fake_item, fake_update, fake_parent);
        installed = install_simple_detour(target, (void*)windows_fake_stash_replacement, &trampoline);
        g_windows_fake_stash_trampoline = trampoline;
        after = (uintptr_t)((void* (*)(int, void*, bool, void*))target)(fake_player, fake_item, fake_update, fake_parent);
        if (trampoline) {
            through_trampoline = (uintptr_t)((void* (*)(int, void*, bool, void*))trampoline)(fake_player, fake_item, fake_update, fake_parent);
        }
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-fake-stash-detour-report.json");

    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, provider && target && installed && before == (uintptr_t)0x30000001cULL && after == (uintptr_t)0x300000029ULL && through_trampoline == (uintptr_t)0x30000001cULL ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"BmlFakeAddItemToVoidChestServer\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"fake-provider-stash-target-detour-only\", \n");
    append_ascii(&cursor, end, "  \"signature\": \"static addItemToVoidChestServer(int, Item*, bool, Item*)-shaped\", \n");
    append_ascii(&cursor, end, "  \"providerLoaded\": ");
    append_ascii(&cursor, end, provider ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCallsOriginal\": ");
    append_ascii(&cursor, end, after == (uintptr_t)0x300000029ULL ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"targetResolved\": ");
    append_ascii(&cursor, end, target ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"before\": ");
    append_uintptr(&cursor, end, before);
    append_ascii(&cursor, end, ",\n  \"after\": ");
    append_uintptr(&cursor, end, after);
    append_ascii(&cursor, end, ",\n  \"trampoline\": ");
    append_uintptr(&cursor, end, through_trampoline);
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';

    write_text_file(path, json, (DWORD)(cursor - json));
    g_windows_fake_stash_trampoline = NULL;
    if (trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    if (provider) {
        FreeLibrary(provider);
    }
    return provider && target && installed && before == (uintptr_t)0x30000001cULL && after == (uintptr_t)0x300000029ULL && through_trampoline == (uintptr_t)0x30000001cULL;
}

static int run_windows_get_item_passthrough_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48
    };
    wchar_t path[MAX_PATH * 4];
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    DWORD rva = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int rva_ok = 0;
    int prologue_ok = 0;
    int installed = 0;
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_get_item_passthrough_calls = 0U;
    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    if (identity_ok) {
        rva_ok = extract_resolved_symbol_rva(hook_manifest, "entity_get_item_from_chest", &rva);
    }
    if (rva_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_get_item_passthrough_replacement, 15U, &trampoline);
        if (installed) {
            g_windows_get_item_passthrough_trampoline = trampoline;
        }
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-get-item-passthrough-install-report.json");
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, identity_ok && rva_ok && prologue_ok && installed ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"entity_get_item_from_chest\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-install-only\", \n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, identity_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, prologue_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_get_item_passthrough_calls);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    {
        int wrote_report = write_text_file(path, json, (DWORD)(cursor - json));
        if (!installed && trampoline) {
            VirtualFree(trampoline, 0, MEM_RELEASE);
        }
        return wrote_report && identity_ok && rva_ok && prologue_ok && installed;
    }
}
static int run_windows_new_item_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire, int accept_any_fire)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7c,
        0x24, 0x20, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57
    };
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_new_item_exit_on_fire = exit_on_fire;
    g_windows_new_item_accept_any_fire = accept_any_fire;
    g_windows_new_item_trampoline = NULL;
    memset(&g_windows_new_item_last_sample, 0, sizeof(g_windows_new_item_last_sample));
    lstrcpynW(g_windows_new_item_report_path, report_dir, (int)(sizeof(g_windows_new_item_report_path) / sizeof(g_windows_new_item_report_path[0])));
    lstrcatW(g_windows_new_item_report_path, L"\\windows-new-item-probe-report.json");

    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_new_item_probe_replacement, 16U, &trampoline);
        if (installed) {
            g_windows_new_item_trampoline = trampoline;
        }
    }
    if (!write_windows_new_item_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}

static int run_windows_set_sprite_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x85, 0xc9,
        0x0f, 0x84, 0x14, 0x01, 0x00, 0x00,
        0x48, 0x89, 0x5c, 0x24, 0x10
    };
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_set_sprite_calls = 0U;
    g_windows_set_sprite_exit_on_fire = exit_on_fire;
    g_windows_set_sprite_trampoline = NULL;
    g_windows_set_sprite_code_view_match = 0;
    g_windows_set_sprite_prologue_match = 0;
    g_windows_set_sprite_rva = rva;
    memset(&g_windows_set_sprite_last_sample, 0, sizeof(g_windows_set_sprite_last_sample));
    if (report_dir && report_dir[0]) {
        lstrcpynW(g_windows_set_sprite_report_path, report_dir, (int)(sizeof(g_windows_set_sprite_report_path) / sizeof(g_windows_set_sprite_report_path[0])));
        lstrcatW(g_windows_set_sprite_report_path, L"\\windows-set-sprite-probe-report.json");
    } else {
        memset(g_windows_set_sprite_report_path, 0, sizeof(g_windows_set_sprite_report_path));
    }

    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    g_windows_set_sprite_code_view_match = identity_ok;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
        g_windows_set_sprite_prologue_match = prologue_ok;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_set_sprite_probe_replacement, 14U, &trampoline);
        if (installed) {
            g_windows_set_sprite_trampoline = trampoline;
        }
    }
    if (g_windows_set_sprite_report_path[0] && !write_windows_set_sprite_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}

typedef struct BmlWindowsDetourInstall {
    unsigned char* target;
    void* trampoline;
    size_t patch_size;
    int identity_ok;
    int prologue_ok;
    int original_valid;
    char install_code[64];
    char install_message[192];
    unsigned char original[BML_DETOUR_MAX_COPY_BYTES];
} BmlWindowsDetourInstall;
static void rollback_windows_detour_install(const BmlWindowsDetourInstall* install)
{
    DWORD old_protect = 0;
    DWORD ignored = 0;
    if (!install || !install->target || install->patch_size == 0U || !install->original_valid) {
        return;
    }
    if (VirtualProtect(install->target, install->patch_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        memcpy(install->target, install->original, install->patch_size);
        FlushInstructionCache(GetCurrentProcess(), install->target, install->patch_size);
        VirtualProtect(install->target, install->patch_size, old_protect, &ignored);
    }
    if (install->trampoline) {
        VirtualFree(install->trampoline, 0, MEM_RELEASE);
    }
}

static int run_windows_checked_detour_install_recorded(const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, const unsigned char* expected_prologue, SIZE_T expected_prologue_size, SIZE_T minimum_patch_size, void* replacement, BmlWindowsDetourInstall* out_install)
{
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    size_t patch_size = 0U;
    const char* install_code = NULL;
    const char* install_message = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;
    void* trampoline = NULL;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    if (out_install) {
        memset(out_install, 0, sizeof(*out_install));
    }
    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = expected_prologue != NULL && expected_prologue_size > 0U && memcmp(target, expected_prologue, expected_prologue_size) == 0;
    }
    if (out_install) {
        out_install->identity_ok = identity_ok;
        out_install->prologue_ok = prologue_ok;
        if (!identity_ok) {
            lstrcpynA(out_install->install_code, "BML_WINDOWS_TARGET_IDENTITY_MISMATCH", (int)(sizeof(out_install->install_code) / sizeof(out_install->install_code[0])));
            lstrcpynA(out_install->install_message, "Loaded executable RSDS guid/age does not match the hook manifest.", (int)(sizeof(out_install->install_message) / sizeof(out_install->install_message[0])));
        } else if (!prologue_ok) {
            lstrcpynA(out_install->install_code, "BML_WINDOWS_PROLOGUE_MISMATCH", (int)(sizeof(out_install->install_code) / sizeof(out_install->install_code[0])));
            lstrcpynA(out_install->install_message, "Target bytes do not match the expected prologue for this probe.", (int)(sizeof(out_install->install_message) / sizeof(out_install->install_message[0])));
        }
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized_recorded(target, replacement, (size_t)minimum_patch_size, &trampoline, &patch_size, &install_code, &install_message, out_install ? out_install->original : NULL);
        if (out_install) {
            if (installed) {
                out_install->target = target;
                out_install->patch_size = patch_size;
                out_install->original_valid = 1;
                out_install->trampoline = trampoline;
            }
            if (install_code) {
                lstrcpynA(out_install->install_code, install_code, (int)(sizeof(out_install->install_code) / sizeof(out_install->install_code[0])));
            }
            if (install_message) {
                lstrcpynA(out_install->install_message, install_message, (int)(sizeof(out_install->install_message) / sizeof(out_install->install_message[0])));
            }
        }
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}
typedef struct BmlWindowsStashCoreDetourInstall {
    const char* symbol_id;
    DWORD rva;
    BmlWindowsDetourInstall install;
    int resolved;
    int installed;
} BmlWindowsStashCoreDetourInstall;

static void reset_windows_stash_core_behavior_state(void)
{
    g_windows_stash_get_inventory_original = NULL;
    g_windows_stash_add_item_to_chest_original = NULL;
    g_windows_stash_get_item_from_chest_original = NULL;
    g_windows_stash_add_item_to_void_original = NULL;
    g_windows_stash_remove_item_from_void_original = NULL;
    g_windows_stash_close_chest_original = NULL;
    g_windows_stash_close_chest_server_original = NULL;
    g_windows_stash_new_item = NULL;
    g_windows_stash_list_free_all = NULL;
    g_windows_stash_stats_symbol = NULL;
    reset_windows_stash_playable_state();
    g_windows_stash_core_behavior_active = false;
    g_windows_stash_core_behavior_loaded = false;
    g_windows_stash_core_behavior_dirty = false;
    g_windows_stash_core_behavior_loads = 0;
    g_windows_stash_core_behavior_saves = 0;
    g_windows_stash_core_behavior_dirty_marks = 0;
    g_windows_stash_core_behavior_inventory = NULL;
}

static int write_windows_stash_core_behavior_report(const wchar_t* report_path, const char* status, const char* error_code, const char* error_message, const BmlWindowsStashCoreDetourInstall* targets, size_t target_count)
{
    char json[8192];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    size_t index;
    append_ascii(&cursor, end, "{\n  \"status\": \"");
    append_ascii(&cursor, end, status ? status : "failed");
    append_ascii(&cursor, end, "\",\n  \"coreBehavior\": {\n    \"active\": ");
    append_ascii(&cursor, end, g_windows_stash_core_behavior_active ? "true" : "false");
    append_ascii(&cursor, end, ",\n    \"loaded\": ");
    append_ascii(&cursor, end, g_windows_stash_core_behavior_loaded ? "true" : "false");
    append_ascii(&cursor, end, ",\n    \"dirty\": ");
    append_ascii(&cursor, end, g_windows_stash_core_behavior_dirty ? "true" : "false");
    append_ascii(&cursor, end, ",\n    \"loadCount\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_stash_core_behavior_loads);
    append_ascii(&cursor, end, ",\n    \"saveCount\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_stash_core_behavior_saves);
    append_ascii(&cursor, end, ",\n    \"dirtyMarks\": ");
    append_uint(&cursor, end, (unsigned int)g_windows_stash_core_behavior_dirty_marks);
    append_ascii(&cursor, end, ",\n    \"boundInventoryCount\": ");
    append_uint(&cursor, end, (unsigned int)windows_stash_inventory_count(g_windows_stash_core_behavior_inventory));
    append_ascii(&cursor, end, ",\n    \"savedRows\": ");
    append_uint(&cursor, end, (unsigned int)windows_count_stash_inventory_file_rows());
    append_ascii(&cursor, end, "\n  },\n  \"targets\": [");
    for (index = 0U; index < target_count; ++index) {
        const BmlWindowsStashCoreDetourInstall* target = &targets[index];
        append_ascii(&cursor, end, index == 0U ? "\n    {" : ",\n    {");
        append_ascii(&cursor, end, "\"symbolId\": \"");
        append_ascii(&cursor, end, target->symbol_id);
        append_ascii(&cursor, end, "\", \"rva\": ");
        append_uint(&cursor, end, target->rva);
        append_ascii(&cursor, end, ", \"resolved\": ");
        append_ascii(&cursor, end, target->resolved ? "true" : "false");
        append_ascii(&cursor, end, ", \"installed\": ");
        append_ascii(&cursor, end, target->installed ? "true" : "false");
        append_ascii(&cursor, end, ", \"codeViewMatch\": ");
        append_ascii(&cursor, end, target->install.identity_ok ? "true" : "false");
        append_ascii(&cursor, end, ", \"prologueMatch\": ");
        append_ascii(&cursor, end, target->install.prologue_ok ? "true" : "false");
        append_ascii(&cursor, end, ", \"patchWindowBytes\": ");
        append_uint(&cursor, end, (unsigned int)target->install.patch_size);
        append_ascii(&cursor, end, ", \"installCode\": \"");
        append_ascii(&cursor, end, target->install.install_code[0] ? target->install.install_code : "unknown");
        append_ascii(&cursor, end, "\", \"installMessage\": \"");
        append_ascii(&cursor, end, target->install.install_message[0] ? target->install.install_message : "No install detail recorded.");
        append_ascii(&cursor, end, "\"}");
    }
    append_ascii(&cursor, end, "\n  ],\n  \"error\": ");
    if (error_code && *error_code) {
        append_ascii(&cursor, end, "{ \"code\": \"");
        append_ascii(&cursor, end, error_code);
        append_ascii(&cursor, end, "\", \"message\": \"");
        append_ascii(&cursor, end, error_message && *error_message ? error_message : "No error message recorded.");
        append_ascii(&cursor, end, "\" }");
    } else {
        append_ascii(&cursor, end, "null");
    }
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(report_path, json, (DWORD)(cursor - json));
}

static int run_windows_stash_core_behavior_install(const wchar_t* profile_dir, const wchar_t* hook_manifest, const wchar_t* target_executable)
{
    static const unsigned char get_inventory_prologue[] = { 0x83, 0x3d, 0xdd, 0xc4, 0xd6, 0x00, 0x02, 0x74, 0x3b, 0x48, 0x8d, 0x05, 0xa0, 0xaf, 0xff, 0xff };
    static const unsigned char add_item_to_chest_prologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x57, 0x48, 0x83, 0xec, 0x20 };
    static const unsigned char get_item_from_chest_prologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48 };
    static const unsigned char add_item_void_prologue[] = { 0x48, 0x8b, 0xc4, 0x55, 0x57, 0x41, 0x54, 0x41, 0x57, 0x48, 0x83, 0xec, 0x48, 0x48, 0x63, 0xe9 };
    static const unsigned char remove_item_void_prologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57 };
    static const unsigned char close_chest_prologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7c, 0x24, 0x18, 0x41 };
    static const unsigned char close_chest_server_prologue[] = { 0x48, 0x8b, 0x81, 0x98, 0x05, 0x00, 0x00, 0x83, 0x38, 0x00, 0x74, 0x3d, 0x45, 0x33, 0xc9, 0x4c };
    BmlWindowsStashCoreDetourInstall targets[7];
    char error_code[256];
    char error_message[256];
    size_t index;
    int result = 0;

    memset(targets, 0, sizeof(targets));
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    reset_windows_stash_core_behavior_state();
    targets[0].symbol_id = "entity_get_chest_inventory_list";
    targets[1].symbol_id = "entity_add_item_to_chest";
    targets[2].symbol_id = "entity_get_item_from_chest";
    targets[3].symbol_id = "entity_add_item_to_void_chest_server";
    targets[4].symbol_id = "entity_remove_item_from_void_chest_server";
    targets[5].symbol_id = "entity_close_chest";
    targets[6].symbol_id = "entity_close_chest_server";

    if (windows_configure_stash_core_behavior(profile_dir, hook_manifest, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        result = -1;
    }

    if (result == 0) {
        static const struct {
            const char* symbol_id;
            const unsigned char* prologue;
            size_t prologue_size;
            size_t patch_size;
            void* replacement;
        } specs[7] = {
            { "entity_get_chest_inventory_list", get_inventory_prologue, sizeof(get_inventory_prologue), 16U, (void*)windows_stash_get_chest_inventory_list_replacement },
            { "entity_add_item_to_chest", add_item_to_chest_prologue, sizeof(add_item_to_chest_prologue), 16U, (void*)windows_stash_add_item_to_chest_replacement },
            { "entity_get_item_from_chest", get_item_from_chest_prologue, sizeof(get_item_from_chest_prologue), 15U, (void*)windows_stash_get_item_from_chest_replacement },
            { "entity_add_item_to_void_chest_server", add_item_void_prologue, sizeof(add_item_void_prologue), 16U, (void*)windows_stash_add_item_to_void_chest_server_replacement },
            { "entity_remove_item_from_void_chest_server", remove_item_void_prologue, sizeof(remove_item_void_prologue), 15U, (void*)windows_stash_remove_item_from_void_chest_server_replacement },
            { "entity_close_chest", close_chest_prologue, sizeof(close_chest_prologue), 15U, (void*)windows_stash_close_chest_replacement },
            { "entity_close_chest_server", close_chest_server_prologue, sizeof(close_chest_server_prologue), 15U, (void*)windows_stash_close_chest_server_replacement }
        };
        for (index = 0U; index < 7U; ++index) {
            targets[index].resolved = extract_resolved_symbol_rva(hook_manifest, specs[index].symbol_id, &targets[index].rva);
            if (!targets[index].resolved || targets[index].rva == 0U) {
                copy_ascii_string(error_code, sizeof(error_code), "BML_WINDOWS_STASH_CORE_SYMBOL_UNRESOLVED");
                copy_ascii_string(error_message, sizeof(error_message), "Windows Stash core behavior requires all resolved chest/core detour targets.");
                result = -1;
                break;
            }
            targets[index].installed = run_windows_checked_detour_install_recorded(hook_manifest, target_executable, targets[index].rva, specs[index].prologue, specs[index].prologue_size, specs[index].patch_size, specs[index].replacement, &targets[index].install);
            if (!targets[index].installed) {
                if (targets[index].install.install_code[0]) {
                    copy_ascii_string(error_code, sizeof(error_code), targets[index].install.install_code);
                } else {
                    copy_ascii_string(error_code, sizeof(error_code), "BML_WINDOWS_STASH_CORE_INSTALL_FAILED");
                }
                if (targets[index].install.install_message[0]) {
                    copy_ascii_string(error_message, sizeof(error_message), targets[index].install.install_message);
                } else {
                    copy_ascii_string(error_message, sizeof(error_message), "Windows Stash core behavior could not install one of the resolved chest/core detours.");
                }
                result = -1;
                break;
            }
        }
    }

    if (result == 0) {
        g_windows_stash_get_inventory_original = (BmlWindowsStashGetChestInventoryListFunction)targets[0].install.trampoline;
        g_windows_stash_add_item_to_chest_original = (BmlWindowsStashAddItemToChestFunction)targets[1].install.trampoline;
        g_windows_stash_get_item_from_chest_original = (BmlWindowsStashGetItemFromChestFunction)targets[2].install.trampoline;
        g_windows_stash_add_item_to_void_original = (BmlWindowsStashAddItemToVoidChestServerFunction)targets[3].install.trampoline;
        g_windows_stash_remove_item_from_void_original = (BmlWindowsStashRemoveItemFromVoidChestServerFunction)targets[4].install.trampoline;
        g_windows_stash_close_chest_original = (BmlWindowsStashCloseChestFunction)targets[5].install.trampoline;
        g_windows_stash_close_chest_server_original = (BmlWindowsStashCloseChestFunction)targets[6].install.trampoline;
    } else {
        for (index = 0U; index < 7U; ++index) {
            if (targets[index].installed) {
                rollback_windows_detour_install(&targets[index].install);
            }
        }
        reset_windows_stash_core_behavior_state();
    }

    if (!write_windows_stash_core_behavior_report(g_windows_stash_core_behavior_report_path, result == 0 ? "installed" : "failed", error_code, error_message, targets, 7U)) {
        return 0;
    }
    return result == 0;
}
static int windows_configure_stash_playable_behavior(const wchar_t* profile_dir, const wchar_t* hook_manifest, char* error_code, size_t error_code_size, char* error_message, size_t error_message_size)
{
    void* shoparea_symbol = NULL;
    void* list_add_node_first_symbol = NULL;
    void* multiplayer_symbol = NULL;
    void* clientnum_symbol = NULL;
    if (join_path_wide(g_windows_stash_playable_behavior_report_path, (DWORD)(sizeof(g_windows_stash_playable_behavior_report_path) / sizeof(g_windows_stash_playable_behavior_report_path[0])), profile_dir, BML_STASH_PLAYABLE_BEHAVIOR_REPORT_RELATIVE_PATH) != 0 ||
        join_path_wide(g_windows_stash_state_dir_path, (DWORD)(sizeof(g_windows_stash_state_dir_path) / sizeof(g_windows_stash_state_dir_path[0])), profile_dir, BML_STASH_STATE_DIR_RELATIVE_PATH) != 0 ||
        join_path_wide(g_windows_stash_diagnostics_path, (DWORD)(sizeof(g_windows_stash_diagnostics_path) / sizeof(g_windows_stash_diagnostics_path[0])), profile_dir, BML_STASH_DIAGNOSTICS_RELATIVE_PATH) != 0) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_PLAYABLE_BEHAVIOR_PATH_TOO_LONG");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash playable behavior report/state path exceeded MAX_PATH.");
        return -1;
    }
    if (!extract_resolved_symbol_address(hook_manifest, "shoparea", &shoparea_symbol) ||
        !extract_resolved_symbol_address(hook_manifest, "list_add_node_first", &list_add_node_first_symbol) ||
        !extract_resolved_symbol_address(hook_manifest, "multiplayer", &multiplayer_symbol) ||
        !extract_resolved_symbol_address(hook_manifest, "clientnum", &clientnum_symbol)) {
        copy_ascii_string(error_code, (DWORD)error_code_size, "BML_STASH_PLAYABLE_BEHAVIOR_SYMBOL_MISSING");
        copy_ascii_string(error_message, (DWORD)error_message_size, "Experimental Windows Stash playable behavior requires resolved shoparea, list_add_node_first, multiplayer, and clientnum symbols.");
        return -1;
    }
    memcpy(&g_windows_stash_list_add_node_first, &list_add_node_first_symbol, sizeof(g_windows_stash_list_add_node_first));
    g_windows_stash_empty_deconstructor = windows_empty_deconstructor;
    g_windows_resolved_shoparea_symbol = shoparea_symbol;
    g_windows_resolved_multiplayer_symbol = multiplayer_symbol;
    g_windows_resolved_clientnum_symbol = clientnum_symbol;
    return 0;
}

static int write_windows_stash_playable_behavior_report(const wchar_t* report_path, const char* status, const char* error_code, const char* error_message, const BmlWindowsStashCoreDetourInstall* targets, size_t target_count)
{
    char json[8192];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    size_t index;
    append_ascii(&cursor, end, "{\n  \"status\": \"");
    append_ascii(&cursor, end, status ? status : "failed");
    append_ascii(&cursor, end, "\",\n  \"playableBehavior\": {\n    \"active\": ");
    append_ascii(&cursor, end, g_windows_stash_playable_active ? "true" : "false");
    append_ascii(&cursor, end, ",\n    \"shopareaSymbol\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_resolved_shoparea_symbol);
    append_ascii(&cursor, end, ",\n    \"expectedEntityList\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_stash_playable_expected_entity_list);
    append_ascii(&cursor, end, ",\n    \"observedEntityList\": ");
    append_uintptr(&cursor, end, (uintptr_t)g_windows_stash_playable_observed_entity_list);
    append_ascii(&cursor, end, ",\n    \"hasListAddNodeFirst\": ");
    append_ascii(&cursor, end, g_windows_stash_list_add_node_first ? "true" : "false");
    append_ascii(&cursor, end, ",\n    \"hasEmptyDeconstructor\": ");
    append_ascii(&cursor, end, g_windows_stash_empty_deconstructor ? "true" : "false");
    append_ascii(&cursor, end, "\n  },\n  \"targets\": [");
    for (index = 0U; index < target_count; ++index) {
        const BmlWindowsStashCoreDetourInstall* target = &targets[index];
        append_ascii(&cursor, end, index == 0U ? "\n    {" : ",\n    {");
        append_ascii(&cursor, end, "\"symbolId\": \"");
        append_ascii(&cursor, end, target->symbol_id);
        append_ascii(&cursor, end, "\", \"rva\": ");
        append_uint(&cursor, end, target->rva);
        append_ascii(&cursor, end, ", \"resolved\": ");
        append_ascii(&cursor, end, target->resolved ? "true" : "false");
        append_ascii(&cursor, end, ", \"installed\": ");
        append_ascii(&cursor, end, target->installed ? "true" : "false");
        append_ascii(&cursor, end, ", \"codeViewMatch\": ");
        append_ascii(&cursor, end, target->install.identity_ok ? "true" : "false");
        append_ascii(&cursor, end, ", \"prologueMatch\": ");
        append_ascii(&cursor, end, target->install.prologue_ok ? "true" : "false");
        append_ascii(&cursor, end, ", \"patchWindowBytes\": ");
        append_uint(&cursor, end, (unsigned int)target->install.patch_size);
        append_ascii(&cursor, end, ", \"installCode\": \"");
        append_ascii(&cursor, end, target->install.install_code[0] ? target->install.install_code : "unknown");
        append_ascii(&cursor, end, "\", \"installMessage\": \"");
        append_ascii(&cursor, end, target->install.install_message[0] ? target->install.install_message : "No install detail recorded.");
        append_ascii(&cursor, end, "\"}");
    }
    append_ascii(&cursor, end, "\n  ],\n  \"error\": ");
    if (error_code && *error_code) {
        append_ascii(&cursor, end, "{ \"code\": \"");
        append_ascii(&cursor, end, error_code);
        append_ascii(&cursor, end, "\", \"message\": \"");
        append_ascii(&cursor, end, error_message && *error_message ? error_message : "No error message recorded.");
        append_ascii(&cursor, end, "\" }");
    } else {
        append_ascii(&cursor, end, "null");
    }
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    return write_text_file(report_path, json, (DWORD)(cursor - json));
}

static int run_windows_stash_playable_behavior_install(const wchar_t* profile_dir, const wchar_t* hook_manifest, const wchar_t* target_executable)
{
    static const unsigned char generate_dungeon_prologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x20, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57 };
    static const unsigned char assign_actions_prologue[] = { 0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55 };
    static const unsigned char summon_no_smoke_prologue[] = { 0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20 };
    static const unsigned char new_entity_prologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7c, 0x24, 0x18 };
    static const unsigned char set_sprite_prologue[] = { 0x48, 0x85, 0xc9, 0x0f, 0x84, 0x14, 0x01, 0x00, 0x00, 0x48, 0x89, 0x5c, 0x24, 0x10 };
    BmlWindowsStashCoreDetourInstall targets[5];
    char error_code[256];
    char error_message[256];
    size_t index;
    int result = 0;
    static const struct {
        const char* symbol_id;
        const unsigned char* prologue;
        size_t prologue_size;
        size_t patch_size;
        void* replacement;
    } specs[5] = {
        { "generate_dungeon", generate_dungeon_prologue, sizeof(generate_dungeon_prologue), 16U, (void*)windows_stash_generate_dungeon_replacement },
        { "assign_actions", assign_actions_prologue, sizeof(assign_actions_prologue), 16U, (void*)windows_stash_assign_actions_replacement },
        { "summon_monster_no_smoke", summon_no_smoke_prologue, sizeof(summon_no_smoke_prologue), 15U, (void*)windows_stash_summon_monster_no_smoke_replacement },
        { "new_entity", new_entity_prologue, sizeof(new_entity_prologue), 15U, (void*)windows_stash_new_entity_replacement },
        { "set_sprite_attributes", set_sprite_prologue, sizeof(set_sprite_prologue), 14U, (void*)windows_stash_set_sprite_attributes_replacement }
    };

    memset(targets, 0, sizeof(targets));
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    reset_windows_stash_playable_state();
    for (index = 0U; index < 5U; ++index) {
        targets[index].symbol_id = specs[index].symbol_id;
    }
    if (windows_configure_stash_playable_behavior(profile_dir, hook_manifest, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        result = -1;
    }
    if (result == 0) {
        for (index = 0U; index < 5U; ++index) {
            targets[index].resolved = extract_resolved_symbol_rva(hook_manifest, specs[index].symbol_id, &targets[index].rva);
            if (!targets[index].resolved || targets[index].rva == 0U) {
                copy_ascii_string(error_code, sizeof(error_code), "BML_WINDOWS_STASH_PLAYABLE_SYMBOL_UNRESOLVED");
                copy_ascii_string(error_message, sizeof(error_message), "Windows Stash playable behavior requires resolved generate_dungeon, assign_actions, summon_monster_no_smoke, new_entity, and set_sprite_attributes symbols.");
                result = -1;
                break;
            }
            targets[index].installed = run_windows_checked_detour_install_recorded(hook_manifest, target_executable, targets[index].rva, specs[index].prologue, specs[index].prologue_size, specs[index].patch_size, specs[index].replacement, &targets[index].install);
            if (!targets[index].installed) {
                if (targets[index].install.install_code[0]) {
                    copy_ascii_string(error_code, sizeof(error_code), targets[index].install.install_code);
                } else {
                    copy_ascii_string(error_code, sizeof(error_code), "BML_WINDOWS_STASH_PLAYABLE_INSTALL_FAILED");
                }
                if (targets[index].install.install_message[0]) {
                    copy_ascii_string(error_message, sizeof(error_message), targets[index].install.install_message);
                } else {
                    copy_ascii_string(error_message, sizeof(error_message), "Windows Stash playable behavior could not install one of the resolved placement detours.");
                }
                result = -1;
                break;
            }
        }
    }
    if (result == 0) {
        g_windows_stash_generate_dungeon_original = (BmlWindowsStashGenerateDungeonFunction)targets[0].install.trampoline;
        g_windows_stash_assign_actions_original = (BmlWindowsStashAssignActionsFunction)targets[1].install.trampoline;
        g_windows_stash_summon_no_smoke_original = (BmlWindowsStashSummonNoSmokeFunction)targets[2].install.trampoline;
        g_windows_stash_new_entity_original = (BmlWindowsStashNewEntityFunction)targets[3].install.trampoline;
        g_windows_stash_set_sprite_attributes_original = (BmlWindowsStashSetSpriteAttributesFunction)targets[4].install.trampoline;
        g_windows_stash_playable_active = 1;
    } else {
        for (index = 0U; index < 5U; ++index) {
            if (targets[index].installed) {
                rollback_windows_detour_install(&targets[index].install);
            }
        }
        reset_windows_stash_playable_state();
    }
    if (!write_windows_stash_playable_behavior_report(g_windows_stash_playable_behavior_report_path, result == 0 ? "installed" : "failed", error_code, error_message, targets, 5U)) {
        return 0;
    }
    return result == 0;
}
static int run_windows_add_item_void_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x8b, 0xc4, 0x55, 0x57, 0x41, 0x54, 0x41,
        0x57, 0x48, 0x83, 0xec, 0x48, 0x48, 0x63, 0xe9
    };
    wchar_t path[MAX_PATH * 4];
    BmlWindowsDetourInstall install;
    int installed = 0;
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;

    memset(&install, 0, sizeof(install));
    g_windows_add_item_void_probe_calls = 0U;
    installed = run_windows_checked_detour_install_recorded(
        hook_manifest,
        target_executable,
        rva,
        expected_prologue,
        sizeof(expected_prologue),
        14U,
        (void*)windows_add_item_void_probe_replacement,
        &install
    );
    if (installed) {
        g_windows_add_item_void_probe_trampoline = install.trampoline;
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-add-item-void-chest-probe-report.json");
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, install.identity_ok && install.prologue_ok && installed ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"entity_add_item_to_void_chest_server\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-install-only\", \n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, install.identity_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, install.prologue_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"patchWindowBytes\": ");
    append_uint(&cursor, end, (unsigned int)install.patch_size);
    append_ascii(&cursor, end, ",\n  \"installCode\": \"");
    append_ascii(&cursor, end, install.install_code[0] ? install.install_code : "unknown");
    append_ascii(&cursor, end, "\",\n  \"installMessage\": \"");
    append_ascii(&cursor, end, install.install_message[0] ? install.install_message : "No install detail recorded.");
    append_ascii(&cursor, end, "\",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_add_item_void_probe_calls);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    {
        int wrote_report = write_text_file(path, json, (DWORD)(cursor - json));
        if (!installed && install.trampoline) {
            VirtualFree(install.trampoline, 0, MEM_RELEASE);
        }
        return wrote_report && install.identity_ok && install.prologue_ok && installed;
    }
}
static int run_windows_get_chest_list_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva)
{
    static const unsigned char expected_prologue[] = {
        0x83, 0x3d, 0xdd, 0xc4, 0xd6, 0x00, 0x02, 0x74,
        0x3b, 0x48, 0x8d, 0x05, 0xa0, 0xaf, 0xff, 0xff
    };
    wchar_t path[MAX_PATH * 4];
    BmlWindowsDetourInstall install;
    int installed = 0;
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;

    memset(&install, 0, sizeof(install));
    g_windows_get_chest_list_probe_calls = 0U;
    installed = run_windows_checked_detour_install_recorded(
        hook_manifest,
        target_executable,
        rva,
        expected_prologue,
        sizeof(expected_prologue),
        16U,
        (void*)windows_get_chest_list_probe_replacement,
        &install
    );
    if (installed) {
        g_windows_get_chest_list_probe_trampoline = install.trampoline;
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-get-chest-list-probe-report.json");
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, install.identity_ok && install.prologue_ok && installed ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"entity_get_chest_inventory_list\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-install-only\", \n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, install.identity_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, install.prologue_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"patchWindowBytes\": ");
    append_uint(&cursor, end, (unsigned int)install.patch_size);
    append_ascii(&cursor, end, ",\n  \"installCode\": \"");
    append_ascii(&cursor, end, install.install_code[0] ? install.install_code : "unknown");
    append_ascii(&cursor, end, "\",\n  \"installMessage\": \"");
    append_ascii(&cursor, end, install.install_message[0] ? install.install_message : "No install detail recorded.");
    append_ascii(&cursor, end, "\",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_get_chest_list_probe_calls);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    {
        int wrote_report = write_text_file(path, json, (DWORD)(cursor - json));
        if (!installed && install.trampoline) {
            VirtualFree(install.trampoline, 0, MEM_RELEASE);
        }
        return wrote_report && install.identity_ok && install.prologue_ok && installed;
    }
}
static int run_windows_remove_item_void_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c,
        0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57
    };
    wchar_t path[MAX_PATH * 4];
    BmlWindowsDetourInstall install;
    int installed = 0;
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;

    memset(&install, 0, sizeof(install));
    g_windows_remove_item_void_probe_calls = 0U;
    installed = run_windows_checked_detour_install_recorded(
        hook_manifest,
        target_executable,
        rva,
        expected_prologue,
        sizeof(expected_prologue),
        16U,
        (void*)windows_remove_item_void_probe_replacement,
        &install
    );
    if (installed) {
        g_windows_remove_item_void_probe_trampoline = install.trampoline;
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-remove-item-void-chest-probe-report.json");
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, install.identity_ok && install.prologue_ok && installed ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"entity_remove_item_from_void_chest_server\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-install-only\", \n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, install.identity_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, install.prologue_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"patchWindowBytes\": ");
    append_uint(&cursor, end, (unsigned int)install.patch_size);
    append_ascii(&cursor, end, ",\n  \"installCode\": \"");
    append_ascii(&cursor, end, install.install_code[0] ? install.install_code : "unknown");
    append_ascii(&cursor, end, "\",\n  \"installMessage\": \"");
    append_ascii(&cursor, end, install.install_message[0] ? install.install_message : "No install detail recorded.");
    append_ascii(&cursor, end, "\",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_remove_item_void_probe_calls);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    {
        int wrote_report = write_text_file(path, json, (DWORD)(cursor - json));
        if (!installed && install.trampoline) {
            VirtualFree(install.trampoline, 0, MEM_RELEASE);
        }
        return wrote_report && install.identity_ok && install.prologue_ok && installed;
    }
}
static int run_windows_close_chest_server_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x8b, 0x81, 0x98, 0x05, 0x00, 0x00, 0x83,
        0x38, 0x00, 0x74, 0x3d, 0x45, 0x33, 0xc9, 0x4c
    };
    wchar_t path[MAX_PATH * 4];
    BmlWindowsDetourInstall install;
    int installed = 0;
    char json[1024];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;

    memset(&install, 0, sizeof(install));
    g_windows_close_chest_server_probe_calls = 0U;
    installed = run_windows_checked_detour_install_recorded(
        hook_manifest,
        target_executable,
        rva,
        expected_prologue,
        sizeof(expected_prologue),
        15U,
        (void*)windows_close_chest_server_probe_replacement,
        &install
    );
    if (installed) {
        g_windows_close_chest_server_probe_trampoline = install.trampoline;
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\windows-close-chest-server-probe-report.json");
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, install.identity_ok && install.prologue_ok && installed ? "installed" : "failed");
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"target\": \"entity_close_chest_server\", \n");
    append_ascii(&cursor, end, "  \"claimBoundary\": \"real-barony-target-install-only\", \n");
    append_ascii(&cursor, end, "  \"codeViewMatch\": ");
    append_ascii(&cursor, end, install.identity_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"rva\": ");
    append_uint(&cursor, end, rva);
    append_ascii(&cursor, end, ",\n  \"prologueMatch\": ");
    append_ascii(&cursor, end, install.prologue_ok ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"patchWindowBytes\": ");
    append_uint(&cursor, end, (unsigned int)install.patch_size);
    append_ascii(&cursor, end, ",\n  \"installCode\": \"");
    append_ascii(&cursor, end, install.install_code[0] ? install.install_code : "unknown");
    append_ascii(&cursor, end, "\",\n  \"installMessage\": \"");
    append_ascii(&cursor, end, install.install_message[0] ? install.install_message : "No install detail recorded.");
    append_ascii(&cursor, end, "\",\n  \"installed\": ");
    append_ascii(&cursor, end, installed ? "true" : "false");
    append_ascii(&cursor, end, ",\n  \"replacementCalls\": ");
    append_uint(&cursor, end, g_windows_close_chest_server_probe_calls);
    append_ascii(&cursor, end, "\n}\n");
    *cursor = '\0';
    {
        int wrote_report = write_text_file(path, json, (DWORD)(cursor - json));
        if (!installed && install.trampoline) {
            VirtualFree(install.trampoline, 0, MEM_RELEASE);
        }
        return wrote_report && install.identity_ok && install.prologue_ok && installed;
    }
}

static int run_windows_checked_detour_install(const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, const unsigned char* expected_prologue, SIZE_T expected_prologue_size, SIZE_T minimum_patch_size, void* replacement, void** out_trampoline)
{
    BmlWindowsDetourInstall install;
    int ok = run_windows_checked_detour_install_recorded(hook_manifest, target_executable, rva, expected_prologue, expected_prologue_size, minimum_patch_size, replacement, &install);
    if (out_trampoline) {
        *out_trampoline = install.trampoline;
    } else if (install.trampoline) {
        VirtualFree(install.trampoline, 0, MEM_RELEASE);
    }
    return ok;
}

static int run_windows_assign_actions_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire)
{
    static const unsigned char default_expected_prologue[] = {
        0x48, 0x85, 0xc9, 0x0f, 0x84, 0x83, 0x4c, 0x01,
        0x00, 0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10
    };
    static const unsigned char map_level_expected_prologue[] = {
        0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10,
        0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55
    };
    const unsigned char* expected_prologue = default_expected_prologue;
    SIZE_T expected_prologue_size = sizeof(default_expected_prologue);
    SIZE_T minimum_patch_size = 16U;
    BmlWindowsDetourInstall install;
    int installed = 0;
    if (rva == 5264752U) {
        expected_prologue = map_level_expected_prologue;
        expected_prologue_size = sizeof(map_level_expected_prologue);
    }
    memset(&install, 0, sizeof(install));
    g_windows_assign_actions_calls = 0U;
    g_windows_assign_actions_exit_on_fire = exit_on_fire;
    g_windows_assign_actions_trampoline = NULL;
    g_windows_assign_actions_code_view_match = 0;
    g_windows_assign_actions_prologue_match = 0;
    g_windows_assign_actions_rva = rva;
    g_windows_assign_actions_last_return_rva = 0U;
    memset(g_windows_assign_actions_return_rvas, 0, sizeof(g_windows_assign_actions_return_rvas));
    memset(g_windows_assign_actions_return_counts, 0, sizeof(g_windows_assign_actions_return_counts));
    g_windows_assign_actions_return_sample_count = 0U;
    lstrcpynW(g_windows_assign_actions_report_path, report_dir, (int)(sizeof(g_windows_assign_actions_report_path) / sizeof(g_windows_assign_actions_report_path[0])));
    lstrcatW(g_windows_assign_actions_report_path, L"\\windows-assign-actions-probe-report.json");

    installed = run_windows_checked_detour_install_recorded(hook_manifest, target_executable, rva, expected_prologue, expected_prologue_size, minimum_patch_size, (void*)windows_assign_actions_replacement, &install);
    g_windows_assign_actions_code_view_match = install.identity_ok;
    g_windows_assign_actions_prologue_match = install.prologue_ok;
    if (installed) {
        g_windows_assign_actions_trampoline = install.trampoline;
    }
    if (!write_windows_assign_actions_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && install.trampoline) {
        VirtualFree(install.trampoline, 0, MEM_RELEASE);
    }
    return installed;
}

static int run_windows_new_entity_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x89, 0x5c, 0x24, 0x08,
        0x48, 0x89, 0x74, 0x24, 0x10,
        0x48, 0x89, 0x7c, 0x24, 0x18
    };
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_new_entity_calls = 0U;
    g_windows_new_entity_sprite_188_calls = 0U;
    g_windows_new_entity_sprite_1484_calls = 0U;
    g_windows_new_entity_sprite_1790_calls = 0U;
    g_windows_new_entity_sprite_1791_calls = 0U;
    g_windows_new_entity_exit_on_fire = exit_on_fire;
    g_windows_new_entity_trampoline = NULL;
    memset(&g_windows_new_entity_last_sample, 0, sizeof(g_windows_new_entity_last_sample));
    lstrcpynW(g_windows_new_entity_report_path, report_dir, (int)(sizeof(g_windows_new_entity_report_path) / sizeof(g_windows_new_entity_report_path[0])));
    lstrcatW(g_windows_new_entity_report_path, L"\\windows-new-entity-probe-report.json");

    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_new_entity_probe_replacement, 15U, &trampoline);
        if (installed) {
            g_windows_new_entity_trampoline = trampoline;
        }
    }
    if (!write_windows_new_entity_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}

static int run_windows_do_new_game_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x8b, 0xc4,
        0x48, 0x89, 0x58, 0x08,
        0x48, 0x89, 0x70, 0x10,
        0x48, 0x89, 0x78, 0x18
    };
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_do_new_game_calls = 0U;
    g_windows_do_new_game_exit_on_fire = exit_on_fire;
    g_windows_do_new_game_last_make_highscore = -1;
    g_windows_do_new_game_trampoline = NULL;
    g_windows_do_new_game_code_view_match = 0;
    g_windows_do_new_game_prologue_match = 0;
    g_windows_do_new_game_rva = rva;
    lstrcpynW(g_windows_do_new_game_report_path, report_dir, (int)(sizeof(g_windows_do_new_game_report_path) / sizeof(g_windows_do_new_game_report_path[0])));
    lstrcatW(g_windows_do_new_game_report_path, L"\\windows-do-new-game-probe-report.json");

    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    g_windows_do_new_game_code_view_match = identity_ok;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
        g_windows_do_new_game_prologue_match = prologue_ok;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_do_new_game_probe_replacement, 15U, &trampoline);
        if (installed) {
            g_windows_do_new_game_trampoline = trampoline;
        }
    }
    if (!write_windows_do_new_game_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}
static int run_windows_init_class_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x89, 0x5c, 0x24, 0x18,
        0x55,
        0x56,
        0x57,
        0x41, 0x54,
        0x41, 0x55,
        0x41, 0x56,
        0x41, 0x57
    };
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_init_class_calls = 0U;
    g_windows_init_class_quickstart_calls = 0U;
    g_windows_init_class_exit_on_fire = exit_on_fire;
    g_windows_init_class_last_player = -1;
    g_windows_init_class_last_return_rva = 0U;
    g_windows_init_class_trampoline = NULL;
    g_windows_init_class_code_view_match = 0;
    g_windows_init_class_prologue_match = 0;
    g_windows_init_class_rva = rva;
    lstrcpynW(g_windows_init_class_report_path, report_dir, (int)(sizeof(g_windows_init_class_report_path) / sizeof(g_windows_init_class_report_path[0])));
    lstrcatW(g_windows_init_class_report_path, L"\\windows-init-class-probe-report.json");

    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    g_windows_init_class_code_view_match = identity_ok;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
        g_windows_init_class_prologue_match = prologue_ok;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_init_class_probe_replacement, 16U, &trampoline);
        if (installed) {
            g_windows_init_class_trampoline = trampoline;
        }
    }
    if (!write_windows_init_class_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}
static int run_windows_summon_probe_install(const wchar_t* report_dir, const wchar_t* hook_manifest, const wchar_t* target_executable, DWORD rva, int exit_on_fire)
{
    static const unsigned char expected_prologue[] = {
        0x48, 0x8b, 0xc4,
        0x48, 0x89, 0x58, 0x10,
        0x48, 0x89, 0x70, 0x18,
        0x48, 0x89, 0x78, 0x20
    };
    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    unsigned char* target = NULL;
    void* trampoline = NULL;
    int identity_ok = 0;
    int prologue_ok = 0;
    int installed = 0;

    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    g_windows_summon_probe_calls = 0U;
    g_windows_summon_shopkeeper_calls = 0U;
    g_windows_summon_probe_exit_on_fire = exit_on_fire;
    g_windows_summon_probe_last_creature = -1;
    g_windows_summon_probe_last_force_location = -1;
    g_windows_summon_probe_last_return_rva = 0U;
    g_windows_summon_probe_trampoline = NULL;
    g_windows_summon_probe_code_view_match = 0;
    g_windows_summon_probe_prologue_match = 0;
    g_windows_summon_probe_rva = rva;
    lstrcpynW(g_windows_summon_probe_report_path, report_dir, (int)(sizeof(g_windows_summon_probe_report_path) / sizeof(g_windows_summon_probe_report_path[0])));
    lstrcatW(g_windows_summon_probe_report_path, L"\\windows-summon-monster-probe-report.json");

    identity_ok =
        extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
        extract_executable_codeview_identity(target_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
        lstrcmpiA(expected_guid, actual_guid) == 0 &&
        expected_age == actual_age;
    g_windows_summon_probe_code_view_match = identity_ok;
    if (identity_ok) {
        target = (unsigned char*)GetModuleHandleW(NULL) + rva;
        prologue_ok = memcmp(target, expected_prologue, sizeof(expected_prologue)) == 0;
        g_windows_summon_probe_prologue_match = prologue_ok;
    }
    if (prologue_ok) {
        installed = install_simple_detour_sized(target, (void*)windows_summon_probe_replacement, 15U, &trampoline);
        if (installed) {
            g_windows_summon_probe_trampoline = trampoline;
        }
    }
    if (!write_windows_summon_probe_report(installed ? "installed_not_fired" : "failed")) {
        return 0;
    }
    if (!installed && trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
    }
    return identity_ok && prologue_ok && installed;
}

__declspec(dllexport) int bml_hook_init(void)
{
    wchar_t profile[MAX_PATH * 4];
    wchar_t manifest[MAX_PATH * 4];
    wchar_t hook_manifest[MAX_PATH * 4];
    wchar_t hook_library[MAX_PATH * 4];
    wchar_t target_executable[MAX_PATH * 4];
    wchar_t loaded_executable[MAX_PATH * 4];
    wchar_t report_dir[MAX_PATH * 4];
    wchar_t bml_dir[MAX_PATH * 4];
    wchar_t path[MAX_PATH * 4];
    char* runtime_manifest_json = NULL;
    BmlWindowsRuntimeReportInfo report_info;
    BmlWindowsRuntimeKind runtime_kind = BML_WINDOWS_RUNTIME_UNKNOWN;
    get_env_wide(L"BML_PROFILE_DIR", profile, (DWORD)(sizeof(profile) / sizeof(profile[0])));
    get_env_wide(L"BML_RUNTIME_MANIFEST", manifest, (DWORD)(sizeof(manifest) / sizeof(manifest[0])));
    get_env_wide(L"BML_HOOK_MANIFEST", hook_manifest, (DWORD)(sizeof(hook_manifest) / sizeof(hook_manifest[0])));
    get_env_wide(L"BML_HOOK_LIBRARY", hook_library, (DWORD)(sizeof(hook_library) / sizeof(hook_library[0])));
    get_env_wide(L"BML_TARGET_EXECUTABLE", target_executable, (DWORD)(sizeof(target_executable) / sizeof(target_executable[0])));
    loaded_executable[0] = L'\0';
    GetModuleFileNameW(NULL, loaded_executable, (DWORD)(sizeof(loaded_executable) / sizeof(loaded_executable[0])));
    int manifest_ok = file_exists(manifest);
    int hook_manifest_ok = file_exists(hook_manifest);
    int hook_library_ok = file_exists(hook_library);
    int launch_contract_ok = manifest_ok && hook_manifest_ok && hook_library_ok;
    int runtime_manifest_loaded = 0;
    char smoke_mod_id[128];
    char smoke_mod_version[32];
    int smoke_mod_ok = 0;
    int stash_mod_ok = 0;
    bml_windows_runtime_report_info_init(&report_info);
    if (manifest_ok) {
        runtime_manifest_json = read_small_ascii_file(manifest, 1024U * 1024U);
        if (runtime_manifest_json) {
            runtime_manifest_loaded = 1;
            bml_populate_windows_report_from_runtime_manifest(&report_info, runtime_manifest_json);
            runtime_kind = bml_windows_runtime_kind_from_id(report_info.runtime_id);
        }
    }
    smoke_mod_id[0] = '\0';
    smoke_mod_version[0] = '\0';
    smoke_mod_ok = launch_contract_ok && runtime_manifest_loaded && runtime_kind == BML_WINDOWS_RUNTIME_NOOP && extract_smoke_mod_id_from_manifest_json(
        runtime_manifest_json,
        smoke_mod_id,
        (DWORD)(sizeof(smoke_mod_id) / sizeof(smoke_mod_id[0])),
        smoke_mod_version,
        (DWORD)(sizeof(smoke_mod_version) / sizeof(smoke_mod_version[0]))
    );
    stash_mod_ok = launch_contract_ok && runtime_manifest_loaded && runtime_kind == BML_WINDOWS_RUNTIME_STASH && report_info.has_stash && report_info.stash_version[0] != '\0';
    int report_ok = launch_contract_ok && runtime_manifest_loaded && ((runtime_kind == BML_WINDOWS_RUNTIME_NOOP && smoke_mod_ok) || (runtime_kind == BML_WINDOWS_RUNTIME_STASH && stash_mod_ok));
    if (runtime_manifest_json) {
        HeapFree(GetProcessHeap(), 0, runtime_manifest_json);
        runtime_manifest_json = NULL;
    }
    int detour_selftest_requested = 0;
    int detour_selftest_ok = 1;
    int fake_stash_selftest_requested = 0;
    int fake_stash_selftest_ok = 1;
    int stash_core_behavior_requested = 0;
    int stash_core_behavior_ok = 1;
    int stash_playable_behavior_requested = 0;
    int stash_playable_behavior_ok = 1;
    int get_item_passthrough_requested = 0;
    int get_item_passthrough_attempted = 0;
    int get_item_passthrough_ok = 1;
    int add_item_void_probe_requested = 0;
    int add_item_void_probe_attempted = 0;
    int add_item_void_probe_ok = 1;
    int get_chest_list_probe_requested = 0;
    int get_chest_list_probe_attempted = 0;
    int get_chest_list_probe_ok = 1;
    int remove_item_void_probe_requested = 0;
    int remove_item_void_probe_attempted = 0;
    int remove_item_void_probe_ok = 1;
    int close_chest_server_probe_requested = 0;
    int close_chest_server_probe_attempted = 0;
    int close_chest_server_probe_ok = 1;
    int new_item_probe_requested = 0;
    int new_item_probe_attempted = 0;
    int new_item_probe_ok = 1;
    int assign_actions_probe_requested = 0;
    int assign_actions_probe_attempted = 0;
    int assign_actions_probe_ok = 1;
    int new_entity_probe_requested = 0;
    int new_entity_probe_attempted = 0;
    int new_entity_probe_ok = 1;
    int set_sprite_probe_requested = 0;
    int set_sprite_probe_attempted = 0;
    int set_sprite_probe_ok = 1;
    int do_new_game_probe_requested = 0;
    int do_new_game_probe_attempted = 0;
    int do_new_game_probe_ok = 1;
    int init_class_probe_requested = 0;
    int init_class_probe_attempted = 0;
    int init_class_probe_ok = 1;
    int summon_probe_requested = 0;
    int summon_probe_attempted = 0;
    int summon_probe_ok = 1;
    int placement_discovery_requested = 0;
    int placement_discovery_attempted = 0;
    int placement_discovery_ok = 1;
    int target_identity_requested = 0;
    int target_identity_ok = 1;

    char expected_guid[64];
    char actual_guid[64];
    DWORD expected_age = 0;
    DWORD actual_age = 0;
    expected_guid[0] = '\0';
    actual_guid[0] = '\0';
    if (!profile[0]) {
        return 2;
    }

    lstrcpynW(bml_dir, profile, (int)(sizeof(bml_dir) / sizeof(bml_dir[0])));
    lstrcatW(bml_dir, L"\\BaronyModLoader");
    ensure_dir(bml_dir);

    lstrcpynW(report_dir, bml_dir, (int)(sizeof(report_dir) / sizeof(report_dir[0])));
    lstrcatW(report_dir, L"\\reports");
    ensure_dir(report_dir);

    detour_selftest_requested = GetEnvironmentVariableW(L"BML_WINDOWS_DETOUR_SELF_TEST", NULL, 0) > 0;
    if (detour_selftest_requested) {
        detour_selftest_ok = run_windows_detour_selftest(report_dir);
        if (!detour_selftest_ok) {
            report_ok = 0;
        }
    }
    fake_stash_selftest_requested = GetEnvironmentVariableW(L"BML_WINDOWS_FAKE_STASH_SELF_TEST", NULL, 0) > 0;
    if (fake_stash_selftest_requested) {
        fake_stash_selftest_ok = run_windows_fake_stash_selftest(report_dir);
        if (!fake_stash_selftest_ok) {
            report_ok = 0;
        }
    }
    stash_core_behavior_requested = GetEnvironmentVariableW(L"BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR", NULL, 0) > 0;
    if ((runtime_kind == BML_WINDOWS_RUNTIME_STASH && stash_mod_ok) || stash_core_behavior_requested) {
        stash_core_behavior_requested = 1;
        stash_core_behavior_ok = run_windows_stash_core_behavior_install(profile, hook_manifest, loaded_executable);
        if (!stash_core_behavior_ok) {
            report_ok = 0;
        }
    }
    stash_playable_behavior_requested = GetEnvironmentVariableW(L"BML_STASH_ENABLE_EXPERIMENTAL_PLAYABLE_BEHAVIOR", NULL, 0) > 0;
    if ((runtime_kind == BML_WINDOWS_RUNTIME_STASH && stash_mod_ok && stash_core_behavior_ok) || stash_playable_behavior_requested) {
        stash_playable_behavior_requested = 1;
        stash_playable_behavior_ok = run_windows_stash_playable_behavior_install(profile, hook_manifest, loaded_executable);
        if (!stash_playable_behavior_ok) {
            report_ok = 0;
        }
    }
    placement_discovery_requested = GetEnvironmentVariableW(L"BML_STASH_PLACEMENT_DISCOVERY", NULL, 0) > 0;
    get_item_passthrough_requested = GetEnvironmentVariableW(L"BML_STASH_INSTALL_GET_ITEM_PASSTHROUGH", NULL, 0) > 0;
    add_item_void_probe_requested = GetEnvironmentVariableW(L"BML_ADD_ITEM_VOID_CHEST_PROBE_RVA", NULL, 0) > 0;
    get_chest_list_probe_requested = GetEnvironmentVariableW(L"BML_GET_CHEST_LIST_PROBE_RVA", NULL, 0) > 0;
    remove_item_void_probe_requested = GetEnvironmentVariableW(L"BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA", NULL, 0) > 0;
    close_chest_server_probe_requested = GetEnvironmentVariableW(L"BML_CLOSE_CHEST_SERVER_PROBE_RVA", NULL, 0) > 0;
    new_item_probe_requested = GetEnvironmentVariableW(L"BML_NEW_ITEM_PROBE_RVA", NULL, 0) > 0;
    assign_actions_probe_requested = GetEnvironmentVariableW(L"BML_ASSIGN_ACTIONS_PROBE_RVA", NULL, 0) > 0;
    new_entity_probe_requested = GetEnvironmentVariableW(L"BML_NEW_ENTITY_PROBE_RVA", NULL, 0) > 0;
    set_sprite_probe_requested = GetEnvironmentVariableW(L"BML_SET_SPRITE_PROBE_RVA", NULL, 0) > 0;
    do_new_game_probe_requested = GetEnvironmentVariableW(L"BML_DO_NEW_GAME_PROBE_RVA", NULL, 0) > 0;
    init_class_probe_requested = GetEnvironmentVariableW(L"BML_INIT_CLASS_PROBE_RVA", NULL, 0) > 0;
    summon_probe_requested = GetEnvironmentVariableW(L"BML_SUMMON_NO_SMOKE_PROBE_RVA", NULL, 0) > 0;
    target_identity_requested = placement_discovery_requested || get_item_passthrough_requested || add_item_void_probe_requested || get_chest_list_probe_requested || remove_item_void_probe_requested || close_chest_server_probe_requested || new_item_probe_requested || assign_actions_probe_requested || new_entity_probe_requested || set_sprite_probe_requested || do_new_game_probe_requested || init_class_probe_requested || summon_probe_requested;
    if (target_identity_requested && hook_manifest_ok) {
        target_identity_ok =
            extract_manifest_codeview_identity(hook_manifest, expected_guid, (DWORD)(sizeof(expected_guid) / sizeof(expected_guid[0])), &expected_age) &&
            extract_executable_codeview_identity(loaded_executable, actual_guid, (DWORD)(sizeof(actual_guid) / sizeof(actual_guid[0])), &actual_age) &&
            lstrcmpiA(expected_guid, actual_guid) == 0 &&
            expected_age == actual_age;
        if (!target_identity_ok) {
            report_ok = 0;
        }
    }
    if (hook_manifest_ok) {
        DWORD map_rva = 0U;
        if (extract_resolved_symbol_rva(hook_manifest, "map", &map_rva) && map_rva != 0U) {
            g_windows_resolved_map_symbol = (unsigned char*)GetModuleHandleW(NULL) + map_rva;
        } else {
            g_windows_resolved_map_symbol = NULL;
        }
    }
    if (placement_discovery_requested) {
        wchar_t placement_assign_rva_text[32];
        wchar_t placement_new_entity_rva_text[32];
        wchar_t placement_set_sprite_rva_text[32];
        DWORD placement_assign_rva = 0;
        DWORD placement_new_entity_rva = 0;
        DWORD placement_set_sprite_rva = 0;
        BmlWindowsDetourInstall placement_assign_install;
        BmlWindowsDetourInstall placement_new_entity_install;
        BmlWindowsDetourInstall placement_set_sprite_install;
        static const unsigned char placement_assign_prologue[] = {
            0x48, 0x85, 0xc9, 0x0f, 0x84, 0x83, 0x4c, 0x01,
            0x00, 0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10
        };
        static const unsigned char placement_new_entity_prologue[] = {
            0x48, 0x89, 0x5c, 0x24, 0x08,
            0x48, 0x89, 0x74, 0x24, 0x10,
            0x48, 0x89, 0x7c, 0x24, 0x18
        };
        static const unsigned char placement_set_sprite_prologue[] = {
            0x48, 0x85, 0xc9,
            0x0f, 0x84, 0x14, 0x01, 0x00, 0x00,
            0x48, 0x89, 0x5c, 0x24, 0x10
        };
        memset(&placement_assign_install, 0, sizeof(placement_assign_install));
        memset(&placement_new_entity_install, 0, sizeof(placement_new_entity_install));
        memset(&placement_set_sprite_install, 0, sizeof(placement_set_sprite_install));
        placement_discovery_attempted = 1;
        get_item_passthrough_requested = 0;
        add_item_void_probe_requested = 0;
        get_chest_list_probe_requested = 0;
        remove_item_void_probe_requested = 0;
        close_chest_server_probe_requested = 0;
        new_item_probe_requested = 0;
        assign_actions_probe_requested = 0;
        new_entity_probe_requested = 0;
        set_sprite_probe_requested = 0;
        do_new_game_probe_requested = 0;
        close_chest_server_probe_requested = 0;
        summon_probe_requested = 0;
        init_class_probe_requested = 0;
        get_env_wide(L"BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA", placement_assign_rva_text, (DWORD)(sizeof(placement_assign_rva_text) / sizeof(placement_assign_rva_text[0])));
        get_env_wide(L"BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA", placement_new_entity_rva_text, (DWORD)(sizeof(placement_new_entity_rva_text) / sizeof(placement_new_entity_rva_text[0])));
        get_env_wide(L"BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA", placement_set_sprite_rva_text, (DWORD)(sizeof(placement_set_sprite_rva_text) / sizeof(placement_set_sprite_rva_text[0])));
        for (DWORD i = 0; placement_assign_rva_text[i] >= L'0' && placement_assign_rva_text[i] <= L'9'; ++i) {
            placement_assign_rva = placement_assign_rva * 10U + (DWORD)(placement_assign_rva_text[i] - L'0');
        }
        for (DWORD i = 0; placement_new_entity_rva_text[i] >= L'0' && placement_new_entity_rva_text[i] <= L'9'; ++i) {
            placement_new_entity_rva = placement_new_entity_rva * 10U + (DWORD)(placement_new_entity_rva_text[i] - L'0');
        }
        for (DWORD i = 0; placement_set_sprite_rva_text[i] >= L'0' && placement_set_sprite_rva_text[i] <= L'9'; ++i) {
            placement_set_sprite_rva = placement_set_sprite_rva * 10U + (DWORD)(placement_set_sprite_rva_text[i] - L'0');
        }
        placement_discovery_ok = 0;
        if (report_ok && target_identity_ok && placement_assign_rva != 0U && placement_new_entity_rva != 0U && placement_set_sprite_rva != 0U) {
            placement_discovery_ok =
                run_windows_checked_detour_install_recorded(hook_manifest, loaded_executable, placement_assign_rva, placement_assign_prologue, sizeof(placement_assign_prologue), 16U, (void*)windows_assign_actions_replacement, &placement_assign_install) &&
                run_windows_checked_detour_install_recorded(hook_manifest, loaded_executable, placement_new_entity_rva, placement_new_entity_prologue, sizeof(placement_new_entity_prologue), 15U, (void*)windows_new_entity_probe_replacement, &placement_new_entity_install) &&
                run_windows_checked_detour_install_recorded(hook_manifest, loaded_executable, placement_set_sprite_rva, placement_set_sprite_prologue, sizeof(placement_set_sprite_prologue), 14U, (void*)windows_set_sprite_probe_replacement, &placement_set_sprite_install);
            if (placement_discovery_ok) {
                configure_windows_placement_discovery(report_dir);
                g_windows_assign_actions_trampoline = placement_assign_install.trampoline;
                g_windows_new_entity_trampoline = placement_new_entity_install.trampoline;
                g_windows_set_sprite_trampoline = placement_set_sprite_install.trampoline;
                activate_windows_placement_discovery();
            } else {
                rollback_windows_detour_install(&placement_set_sprite_install);
                rollback_windows_detour_install(&placement_new_entity_install);
                rollback_windows_detour_install(&placement_assign_install);
                report_ok = 0;
            }
        } else {
            report_ok = 0;
        }
    }
    if (get_item_passthrough_requested) {
        if (report_ok && target_identity_ok) {
            get_item_passthrough_attempted = 1;
            get_item_passthrough_ok = run_windows_get_item_passthrough_install(report_dir, hook_manifest, loaded_executable);
            if (!get_item_passthrough_ok) {
                report_ok = 0;
            }
        }
    }
    if (add_item_void_probe_requested) {
        wchar_t add_item_void_rva_text[32];
        DWORD add_item_void_rva = 0;
        get_env_wide(L"BML_ADD_ITEM_VOID_CHEST_PROBE_RVA", add_item_void_rva_text, (DWORD)(sizeof(add_item_void_rva_text) / sizeof(add_item_void_rva_text[0])));
        for (DWORD i = 0; add_item_void_rva_text[i] >= L'0' && add_item_void_rva_text[i] <= L'9'; ++i) {
            add_item_void_rva = add_item_void_rva * 10U + (DWORD)(add_item_void_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && add_item_void_rva != 0U) {
            add_item_void_probe_attempted = 1;
            add_item_void_probe_ok = run_windows_add_item_void_probe_install(report_dir, hook_manifest, loaded_executable, add_item_void_rva);
            if (!add_item_void_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (get_chest_list_probe_requested) {
        wchar_t get_chest_list_rva_text[32];
        DWORD get_chest_list_rva = 0;
        get_env_wide(L"BML_GET_CHEST_LIST_PROBE_RVA", get_chest_list_rva_text, (DWORD)(sizeof(get_chest_list_rva_text) / sizeof(get_chest_list_rva_text[0])));
        for (DWORD i = 0; get_chest_list_rva_text[i] >= L'0' && get_chest_list_rva_text[i] <= L'9'; ++i) {
            get_chest_list_rva = get_chest_list_rva * 10U + (DWORD)(get_chest_list_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && get_chest_list_rva != 0U) {
            get_chest_list_probe_attempted = 1;
            get_chest_list_probe_ok = run_windows_get_chest_list_probe_install(report_dir, hook_manifest, loaded_executable, get_chest_list_rva);
            if (!get_chest_list_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (remove_item_void_probe_requested) {
        wchar_t remove_item_void_rva_text[32];
        DWORD remove_item_void_rva = 0;
        get_env_wide(L"BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA", remove_item_void_rva_text, (DWORD)(sizeof(remove_item_void_rva_text) / sizeof(remove_item_void_rva_text[0])));
        for (DWORD i = 0; remove_item_void_rva_text[i] >= L'0' && remove_item_void_rva_text[i] <= L'9'; ++i) {
            remove_item_void_rva = remove_item_void_rva * 10U + (DWORD)(remove_item_void_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && remove_item_void_rva != 0U) {
            remove_item_void_probe_attempted = 1;
            remove_item_void_probe_ok = run_windows_remove_item_void_probe_install(report_dir, hook_manifest, loaded_executable, remove_item_void_rva);
            if (!remove_item_void_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (close_chest_server_probe_requested) {
        wchar_t close_chest_server_rva_text[32];
        DWORD close_chest_server_rva = 0;
        get_env_wide(L"BML_CLOSE_CHEST_SERVER_PROBE_RVA", close_chest_server_rva_text, (DWORD)(sizeof(close_chest_server_rva_text) / sizeof(close_chest_server_rva_text[0])));
        for (DWORD i = 0; close_chest_server_rva_text[i] >= L'0' && close_chest_server_rva_text[i] <= L'9'; ++i) {
            close_chest_server_rva = close_chest_server_rva * 10U + (DWORD)(close_chest_server_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && close_chest_server_rva != 0U) {
            close_chest_server_probe_attempted = 1;
            close_chest_server_probe_ok = run_windows_close_chest_server_probe_install(report_dir, hook_manifest, loaded_executable, close_chest_server_rva);
            if (!close_chest_server_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (new_item_probe_requested) {
        wchar_t new_item_rva_text[32];
        DWORD new_item_rva = 0;
        int exit_on_fire = GetEnvironmentVariableW(L"BML_NEW_ITEM_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        int accept_any_fire = GetEnvironmentVariableW(L"BML_NEW_ITEM_PROBE_ACCEPT_ANY_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_NEW_ITEM_PROBE_RVA", new_item_rva_text, (DWORD)(sizeof(new_item_rva_text) / sizeof(new_item_rva_text[0])));
        for (DWORD i = 0; new_item_rva_text[i] >= L'0' && new_item_rva_text[i] <= L'9'; ++i) {
            new_item_rva = new_item_rva * 10U + (DWORD)(new_item_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && new_item_rva != 0U) {
            new_item_probe_attempted = 1;
            new_item_probe_ok = run_windows_new_item_probe_install(report_dir, hook_manifest, loaded_executable, new_item_rva, exit_on_fire, accept_any_fire);
            if (!new_item_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (assign_actions_probe_requested) {
        wchar_t assign_rva_text[32];
        DWORD assign_rva = 0;
        int exit_on_fire = GetEnvironmentVariableW(L"BML_ASSIGN_ACTIONS_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_ASSIGN_ACTIONS_PROBE_RVA", assign_rva_text, (DWORD)(sizeof(assign_rva_text) / sizeof(assign_rva_text[0])));
        for (DWORD i = 0; assign_rva_text[i] >= L'0' && assign_rva_text[i] <= L'9'; ++i) {
            assign_rva = assign_rva * 10U + (DWORD)(assign_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && assign_rva != 0U) {
            assign_actions_probe_attempted = 1;
            assign_actions_probe_ok = run_windows_assign_actions_probe_install(report_dir, hook_manifest, loaded_executable, assign_rva, exit_on_fire);
            if (!assign_actions_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (new_entity_probe_requested) {
        wchar_t new_entity_rva_text[32];
        DWORD new_entity_rva = 0;
        int exit_on_fire = GetEnvironmentVariableW(L"BML_NEW_ENTITY_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_NEW_ENTITY_PROBE_RVA", new_entity_rva_text, (DWORD)(sizeof(new_entity_rva_text) / sizeof(new_entity_rva_text[0])));
        for (DWORD i = 0; new_entity_rva_text[i] >= L'0' && new_entity_rva_text[i] <= L'9'; ++i) {
            new_entity_rva = new_entity_rva * 10U + (DWORD)(new_entity_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && new_entity_rva != 0U) {
            new_entity_probe_attempted = 1;
            new_entity_probe_ok = run_windows_new_entity_probe_install(report_dir, hook_manifest, loaded_executable, new_entity_rva, exit_on_fire);
            if (!new_entity_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (set_sprite_probe_requested) {
        wchar_t set_sprite_rva_text[32];
        DWORD set_sprite_rva = 0;
        int exit_on_fire = GetEnvironmentVariableW(L"BML_SET_SPRITE_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_SET_SPRITE_PROBE_RVA", set_sprite_rva_text, (DWORD)(sizeof(set_sprite_rva_text) / sizeof(set_sprite_rva_text[0])));
        for (DWORD i = 0; set_sprite_rva_text[i] >= L'0' && set_sprite_rva_text[i] <= L'9'; ++i) {
            set_sprite_rva = set_sprite_rva * 10U + (DWORD)(set_sprite_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && set_sprite_rva != 0U) {
            set_sprite_probe_attempted = 1;
            set_sprite_probe_ok = run_windows_set_sprite_probe_install(report_dir, hook_manifest, loaded_executable, set_sprite_rva, exit_on_fire);
            if (!set_sprite_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (do_new_game_probe_requested) {
        wchar_t do_new_game_rva_text[32];
        DWORD do_new_game_rva = 0;
        int do_new_game_exit_on_fire = GetEnvironmentVariableW(L"BML_DO_NEW_GAME_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_DO_NEW_GAME_PROBE_RVA", do_new_game_rva_text, (DWORD)(sizeof(do_new_game_rva_text) / sizeof(do_new_game_rva_text[0])));
        for (DWORD i = 0; do_new_game_rva_text[i] >= L'0' && do_new_game_rva_text[i] <= L'9'; ++i) {
            do_new_game_rva = do_new_game_rva * 10U + (DWORD)(do_new_game_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && do_new_game_rva != 0U) {
            do_new_game_probe_attempted = 1;
            do_new_game_probe_ok = run_windows_do_new_game_probe_install(report_dir, hook_manifest, loaded_executable, do_new_game_rva, do_new_game_exit_on_fire);
            if (!do_new_game_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (init_class_probe_requested) {
        wchar_t init_class_rva_text[32];
        DWORD init_class_rva = 0;
        int init_class_exit_on_fire = GetEnvironmentVariableW(L"BML_INIT_CLASS_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_INIT_CLASS_PROBE_RVA", init_class_rva_text, (DWORD)(sizeof(init_class_rva_text) / sizeof(init_class_rva_text[0])));
        for (DWORD i = 0; init_class_rva_text[i] >= L'0' && init_class_rva_text[i] <= L'9'; ++i) {
            init_class_rva = init_class_rva * 10U + (DWORD)(init_class_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && init_class_rva != 0U) {
            init_class_probe_attempted = 1;
            init_class_probe_ok = run_windows_init_class_probe_install(report_dir, hook_manifest, loaded_executable, init_class_rva, init_class_exit_on_fire);
            if (!init_class_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (summon_probe_requested) {
        wchar_t summon_rva_text[32];
        DWORD summon_rva = 0;
        int summon_exit_on_fire = GetEnvironmentVariableW(L"BML_SUMMON_NO_SMOKE_PROBE_EXIT_ON_FIRE", NULL, 0) > 0;
        get_env_wide(L"BML_SUMMON_NO_SMOKE_PROBE_RVA", summon_rva_text, (DWORD)(sizeof(summon_rva_text) / sizeof(summon_rva_text[0])));
        for (DWORD i = 0; summon_rva_text[i] >= L'0' && summon_rva_text[i] <= L'9'; ++i) {
            summon_rva = summon_rva * 10U + (DWORD)(summon_rva_text[i] - L'0');
        }
        if (report_ok && target_identity_ok && summon_rva != 0U) {
            summon_probe_attempted = 1;
            summon_probe_ok = run_windows_summon_probe_install(report_dir, hook_manifest, loaded_executable, summon_rva, summon_exit_on_fire);
            if (!summon_probe_ok) {
                report_ok = 0;
            }
        }
    }
    if (runtime_kind == BML_WINDOWS_RUNTIME_STASH) {
        int stash_ready = stash_mod_ok && stash_core_behavior_requested && stash_core_behavior_ok && stash_playable_behavior_requested && stash_playable_behavior_ok;
        report_ok = report_ok && stash_ready;
    }

    lstrcpynW(path, report_dir, (int)(sizeof(path) / sizeof(path[0])));
    lstrcatW(path, L"\\runtime-load-report.json");

    char json[4096];
    char* cursor = json;
    char* end = json + sizeof(json) - 1;
    append_ascii(&cursor, end, "{\n");
    append_ascii(&cursor, end, "  \"contract\": { \"id\": \"bml-runtime-contract\", \"version\": \"0.1.0\" },\n");
    append_ascii(&cursor, end, "  \"runtime\": { \"id\": \"");
    append_ascii(&cursor, end, report_info.runtime_id);
    append_ascii(&cursor, end, "\", \"version\": \"");
    append_ascii(&cursor, end, report_info.runtime_version);
    append_ascii(&cursor, end, "\", \"strategy\": \"");
    append_ascii(&cursor, end, report_info.runtime_strategy);
    append_ascii(&cursor, end, "\", \"gameRevision\": \"");
    append_ascii(&cursor, end, report_info.game_revision);
    append_ascii(&cursor, end, "\" },\n");
    append_ascii(&cursor, end, "  \"profileId\": \"");
    append_ascii(&cursor, end, report_info.profile_id);
    append_ascii(&cursor, end, "\",\n");
    append_ascii(&cursor, end, "  \"status\": \"");
    append_ascii(&cursor, end, report_ok ? "loaded" : "failed");
    append_ascii(&cursor, end, "\",\n");
    if (runtime_kind == BML_WINDOWS_RUNTIME_NOOP && smoke_mod_ok && report_ok) {
        append_ascii(&cursor, end, "  \"loadedMods\": [ { \"id\": \"");
        append_ascii(&cursor, end, smoke_mod_id);
        append_ascii(&cursor, end, "\", \"version\": \"");
        append_ascii(&cursor, end, smoke_mod_version);
        append_ascii(&cursor, end, "\", \"status\": \"loaded\", \"capabilities\": [\"runtime_load_smoke\"], \"modules\": [] } ],\n");
    } else if (runtime_kind == BML_WINDOWS_RUNTIME_STASH && stash_mod_ok && stash_core_behavior_requested && stash_core_behavior_ok && stash_playable_behavior_requested && stash_playable_behavior_ok && report_ok) {
        append_ascii(&cursor, end, "  \"loadedMods\": [ { \"id\": \"jml.stash\", \"version\": \"");
        append_ascii(&cursor, end, report_info.stash_version);
        append_ascii(&cursor, end, "\", \"status\": \"loaded\", \"capabilities\": [\"persistent_storage\", \"persistent_inventory\", \"void_chest_binding\", \"placement_lobby\", \"placement_shop\", \"multiplayer_version_metadata\"], \"modules\": [\"persistentStorage\", \"persistentInventories\", \"voidChestBindings\", \"placements\", \"multiplayer\"] } ],\n");
    } else {
        append_ascii(&cursor, end, "  \"loadedMods\": [],\n");
    }
    if (report_ok) {
        if (runtime_kind == BML_WINDOWS_RUNTIME_NOOP) {
            append_ascii(&cursor, end, "  \"warnings\": [\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_NOOP_RUNTIME\", \"severity\": \"warning\", \"message\": \"Windows no-op hook loaded; Stash gameplay hooks are not installed.\", \"action\": \"warn-user\" }\n");
            append_ascii(&cursor, end, "  ],\n");
        } else {
            append_ascii(&cursor, end, "  \"warnings\": [],\n");
        }
        append_ascii(&cursor, end, "  \"errors\": [],\n");
    } else {
        int error_count = 0;
        append_ascii(&cursor, end, "  \"warnings\": [],\n");
        append_ascii(&cursor, end, "  \"errors\": [\n");
        if (!manifest_ok) {
            append_ascii(&cursor, end, "    { \"code\": \"BML_RUNTIME_MANIFEST_MISSING\", \"severity\": \"fatal\", \"message\": \"BML_RUNTIME_MANIFEST is missing or unreadable.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (!hook_manifest_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_HOOK_MANIFEST_MISSING\", \"severity\": \"fatal\", \"message\": \"BML_HOOK_MANIFEST is missing or unreadable.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (new_item_probe_attempted && !new_item_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_NEW_ITEM_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows newItem fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (!hook_library_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_HOOK_LIBRARY_MISSING\", \"severity\": \"fatal\", \"message\": \"BML_HOOK_LIBRARY is missing or unreadable.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (manifest_ok && !runtime_manifest_loaded) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_RUNTIME_MANIFEST_PARSE_FAILED\", \"severity\": \"fatal\", \"message\": \"BML_RUNTIME_MANIFEST could not be read by the native hook.\", \"action\": \"block-launch\" }");
            error_count++;
        } else if (launch_contract_ok && runtime_kind == BML_WINDOWS_RUNTIME_UNKNOWN) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_RUNTIME_ID_UNSUPPORTED\", \"severity\": \"fatal\", \"message\": \"Runtime manifest runtimeId is not supported by this Windows hook.\", \"action\": \"block-launch\" }");
            error_count++;
        } else if (launch_contract_ok && runtime_kind == BML_WINDOWS_RUNTIME_STASH && !stash_mod_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_STASH_MOD_MISSING\", \"severity\": \"fatal\", \"message\": \"Runtime manifest does not include jml.stash, so the Windows stash runtime was not accepted.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (new_entity_probe_attempted && !new_entity_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_NEW_ENTITY_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows newEntity fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (detour_selftest_requested && !detour_selftest_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_DETOUR_SELF_TEST_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows detour substrate self-test failed.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (fake_stash_selftest_requested && !fake_stash_selftest_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_FAKE_STASH_DETOUR_SELF_TEST_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows fake-provider Stash detour self-test failed.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (stash_core_behavior_requested && !stash_core_behavior_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_STASH_CORE_BEHAVIOR_FAILED\", \"severity\": \"fatal\", \"message\": \"Experimental Windows Stash core behavior install failed.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (stash_playable_behavior_requested && !stash_playable_behavior_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_STASH_PLAYABLE_BEHAVIOR_FAILED\", \"severity\": \"fatal\", \"message\": \"Experimental Windows Stash playable behavior install failed.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (placement_discovery_attempted && !placement_discovery_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_PLACEMENT_DISCOVERY_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows placement discovery is disabled until assignActions/newEntity/setSpriteAttributes use relocation-safe or branch-free detour windows.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (target_identity_requested && !target_identity_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_TARGET_IDENTITY_MISMATCH\", \"severity\": \"fatal\", \"message\": \"Loaded executable RSDS guid/age does not match the hook manifest (expected ");
            append_ascii(&cursor, end, expected_guid[0] ? expected_guid : "missing");
            append_ascii(&cursor, end, " / ");
            append_uint(&cursor, end, expected_age);
            append_ascii(&cursor, end, ", actual ");
            append_ascii(&cursor, end, actual_guid[0] ? actual_guid : "missing");
            append_ascii(&cursor, end, " / ");
            append_uint(&cursor, end, actual_age);
            append_ascii(&cursor, end, ").\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (init_class_probe_attempted && !init_class_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_INIT_CLASS_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows initClass fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (do_new_game_probe_attempted && !do_new_game_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_DO_NEW_GAME_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows doNewGame fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (get_item_passthrough_attempted && !get_item_passthrough_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_GET_ITEM_PASSTHROUGH_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows getItemFromChest passthrough install probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (add_item_void_probe_attempted && !add_item_void_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_ADD_ITEM_VOID_CHEST_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows addItemToVoidChestServer install probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (get_chest_list_probe_attempted && !get_chest_list_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_GET_CHEST_LIST_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows getChestInventoryList install probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (remove_item_void_probe_attempted && !remove_item_void_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_REMOVE_ITEM_VOID_CHEST_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows removeItemFromVoidChestServer install probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (close_chest_server_probe_attempted && !close_chest_server_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_CLOSE_CHEST_SERVER_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows closeChestServer install probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (assign_actions_probe_attempted && !assign_actions_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_ASSIGN_ACTIONS_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows assignActions fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (set_sprite_probe_attempted && !set_sprite_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_SET_SPRITE_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows setSpriteAttributes fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        if (summon_probe_attempted && !summon_probe_ok) {
            if (error_count > 0) append_ascii(&cursor, end, ",\n");
            append_ascii(&cursor, end, "    { \"code\": \"BML_WINDOWS_SUMMON_NO_SMOKE_PROBE_INSTALL_FAILED\", \"severity\": \"fatal\", \"message\": \"Windows summonMonsterNoSmoke fired-hook probe failed after RSDS identity validation.\", \"action\": \"block-launch\" }");
            error_count++;
        }
        append_ascii(&cursor, end, "\n  ],\n");
    }
    append_ascii(&cursor, end, "  \"reportedAt\": \"2026-07-03T00:00:00Z\"\n");
    append_ascii(&cursor, end, "}\n");
    *cursor = '\0';

    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 3;
    }
    DWORD written = 0;
    DWORD len = (DWORD)(cursor - json);
    BOOL ok = WriteFile(file, json, len, &written, NULL);
    CloseHandle(file);
    return ok && written == len ? (report_ok ? 0 : 5) : 4;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
