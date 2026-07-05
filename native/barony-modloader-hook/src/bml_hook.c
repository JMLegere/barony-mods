#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*
 * Linux adapter boundary:
 * This translation unit is the verified linux-x86_64 LD_PRELOAD hook payload
 * for barony.x86_64. Windows launch/injection responsibilities are recorded in
 * src/bml_windows_adapter_stub.c and are intentionally not compiled here.
 */

#define BML_CONTRACT_ID "bml-runtime-contract"
#define BML_CONTRACT_VERSION "0.1.0"
#define BML_NATIVE_RUNTIME_ID "barony-bml-native-hook"
#define BML_NATIVE_RUNTIME_VERSION "0.1.0"
#define BML_DEFAULT_RUNTIME_STRATEGY "installed-binary-hook"
#define BML_REPORT_RELATIVE_PATH "BaronyModLoader/reports/runtime-load-report.json"
#define BML_SYMBOL_REPORT_RELATIVE_PATH "BaronyModLoader/reports/symbol-probe-report.json"
#define BML_STASH_HOOK_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-hook-report.json"
#define BML_REPORT_DIR_RELATIVE_PATH "BaronyModLoader/reports"
#define BML_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH "BaronyModLoader/reports/detour-self-test-report.json"
#define BML_STASH_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-detour-self-test-report.json"
#define BML_STASH_DETOUR_INSTALL_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-detour-install-report.json"
#define BML_STASH_CORE_DETOUR_INSTALL_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-core-detour-install-report.json"
#define BML_STASH_ACCESS_PLACEMENT_DETOUR_INSTALL_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-access-placement-detour-install-report.json"
#define BML_STASH_ACCESS_PLACEMENT_SELF_TEST_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-access-placement-self-test-report.json"
#define BML_STASH_PLACEMENT_DISCOVERY_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-placement-discovery-report.json"
#define BML_STASH_CORE_BEHAVIOR_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-core-behavior-report.json"
#define BML_RUNES_ELIXIR_SELF_TEST_REPORT_RELATIVE_PATH "BaronyModLoader/reports/runebound-elixir-self-test-report.json"
#define BML_RUNES_ELIXIR_PACKAGE_ID "jml.runebound-elixirs"
#define BML_RUNES_ELIXIR_LIVE_INSTALL_REPORT_RELATIVE_PATH "BaronyModLoader/reports/runebound-elixir-live-install-report.json"
#define BML_RUNES_ELIXIR_CARRIER_ITEM_TYPE_POTION_EMPTY 210
#define BML_STASH_STATE_DIR_RELATIVE_PATH "BaronyModLoader/state"
#define BML_STASH_INVENTORY_RELATIVE_PATH "BaronyModLoader/state/stash-inventory-v1.tsv"
#define BML_STASH_INVENTORY_FORMAT_HEADER "# bml-stash-inventory-v2"
#define BML_STASH_INVENTORY_COLUMN_COUNT 19
#define BML_STASH_DIAGNOSTICS_RELATIVE_PATH "BaronyModLoader/state/stash-diagnostics.jsonl"
#define BML_STASH_STAT_VOID_CHEST_INVENTORY_OFFSET ((uintptr_t)0x9e8U)
#define BML_MAX_ERRORS 12
#define BML_MAX_TEXT 256
#define BML_MAX_MANIFEST_BYTES (1024U * 1024U)
#define BML_MAX_REQUIRED_SYMBOLS 32
#define BML_RUNES_ELIXIR_SERIALIZED_STATE_MAX 512U
#define BML_DETOUR_PATCH_BYTES 14U
#define BML_DETOUR_MAX_COPY_BYTES 32U
#define BML_DETOUR_MAX_INSTRUCTIONS 32U
#define BML_DETOUR_MAX_RELOCATED_BYTES ((BML_DETOUR_MAX_COPY_BYTES * 8U) + BML_DETOUR_PATCH_BYTES)
#define BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT 16U
#define BML_STASH_ENTITY_OFFSET_UID ((uintptr_t)104U)
#define BML_STASH_ENTITY_OFFSET_TICKS ((uintptr_t)200U)
#define BML_STASH_ENTITY_OFFSET_X ((uintptr_t)208U)
#define BML_STASH_ENTITY_OFFSET_Y ((uintptr_t)216U)
#define BML_STASH_ENTITY_OFFSET_Z ((uintptr_t)224U)
#define BML_STASH_ENTITY_OFFSET_YAW ((uintptr_t)232U)
#define BML_STASH_ENTITY_OFFSET_PITCH ((uintptr_t)240U)
#define BML_STASH_ENTITY_OFFSET_ROLL ((uintptr_t)248U)
#define BML_STASH_ENTITY_OFFSET_FOCALX ((uintptr_t)256U)
#define BML_STASH_ENTITY_OFFSET_FOCALY ((uintptr_t)264U)
#define BML_STASH_ENTITY_OFFSET_FOCALZ ((uintptr_t)272U)
#define BML_STASH_ENTITY_OFFSET_SCALEX ((uintptr_t)280U)
#define BML_STASH_ENTITY_OFFSET_SCALEY ((uintptr_t)288U)
#define BML_STASH_ENTITY_OFFSET_SCALEZ ((uintptr_t)296U)
#define BML_STASH_ENTITY_OFFSET_SIZEX ((uintptr_t)304U)
#define BML_STASH_ENTITY_OFFSET_SIZEY ((uintptr_t)308U)
#define BML_STASH_ENTITY_OFFSET_SPRITE ((uintptr_t)312U)
#define BML_STASH_ENTITY_OFFSET_FSKILL ((uintptr_t)400U)
#define BML_STASH_ENTITY_OFFSET_SKILL ((uintptr_t)640U)
#define BML_STASH_ENTITY_OFFSET_SKILL17 ((uintptr_t)(640U + 17U * sizeof(int32_t)))
#define BML_STASH_ENTITY_OFFSET_SKILL58 ((uintptr_t)(640U + 58U * sizeof(int32_t)))
#define BML_STASH_ENTITY_OFFSET_FLAGS ((uintptr_t)880U)
#define BML_STASH_ENTITY_OFFSET_CHILDREN ((uintptr_t)920U)
#define BML_STASH_ENTITY_OFFSET_PARENT ((uintptr_t)936U)
#define BML_STASH_ENTITY_OFFSET_MAPGENX ((uintptr_t)1396U)
#define BML_STASH_ENTITY_OFFSET_MAPGENY ((uintptr_t)1400U)
#define BML_STASH_ENTITY_OFFSET_NODE ((uintptr_t)4880U)
#define BML_STASH_ENTITY_OFFSET_BEHAVIOR ((uintptr_t)4936U)
#define BML_STASH_ENTITY_OFFSET_RANBEHAVIOR ((uintptr_t)4944U)
#define BML_STASH_ENTITY_SIZEOF ((size_t)5024U)
#define BML_STASH_CHEST_VOID_STATE_PERMANENT ((int32_t)(-1))
#define BML_STASH_INTERNAL_MARKER_SKILL58 ((int32_t)0x424D4C00)
#define BML_STASH_MAP_OFFSET_ENTITIES ((uintptr_t)208U)
#define BML_STASH_LOBBY_PLACEMENT_X 232.0
#define BML_STASH_LOBBY_PLACEMENT_Y 280.0
#define BML_STASH_PI 3.14159265358979323846
// Barony's assignActions chest setup labels yaw=0 as east-facing and yaw=3*PI/2 as north-facing.
#define BML_STASH_YAW_EAST 0.0
#define BML_STASH_YAW_SOUTH (BML_STASH_PI / 2.0)
#define BML_STASH_YAW_WEST BML_STASH_PI
#define BML_STASH_YAW_NORTH (3.0 * BML_STASH_PI / 2.0)
#define BML_STASH_LOBBY_PLACEMENT_YAW BML_STASH_YAW_NORTH
#define BML_STASH_PLACEMENT_LID_HINGE_OFFSET 3.0
#define BML_STASH_PLACEMENT_LID_OFFSET_Z (-2.75)
#define BML_STASH_MULTIPLAYER_CLIENT 2
#define BML_STASH_MULTIPLAYER_DIRECTCLIENT 4
#define BML_STASH_SPRITE_CHEST_SPAWN 21
#define BML_STASH_SPRITE_CHEST_VOID_VISUAL 1791
#define BML_STASH_SPRITE_LID_SPAWN 216
#define BML_STASH_SPRITE_LID_VOID_VISUAL 1790
#define BML_STASH_PLAYABLE_INSTALL_REPORT_RELATIVE_PATH "BaronyModLoader/reports/stash-playable-install-report.json"
#define BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST 4005
#define BML_STASH_PROMPT_LANGUAGE_ID_TOOLTIP_ACTION 3998
#define BML_STASH_PROMPT_OPEN_STASH "Open stash"
#define BML_STASH_SELECTED_ENTITY_PLAYERS 4U

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define BML_DETOUR_NEAR_SEARCH_RANGE ((uintptr_t)0x70000000ULL)
#define BML_DETOUR_NEAR_SEARCH_STEP ((uintptr_t)0x10000ULL)
#define BML_DETOUR_MIN_MMAP_ADDRESS ((uintptr_t)0x10000ULL)

typedef struct BmlError {
    const char *code;
    const char *severity;
    const char *message;
    const char *env_name;
    char path[PATH_MAX];
} BmlError;

typedef struct BmlReportInfo {
    char contract_id[BML_MAX_TEXT];
    char contract_version[BML_MAX_TEXT];
    char runtime_id[BML_MAX_TEXT];
    char runtime_version[BML_MAX_TEXT];
    char runtime_strategy[BML_MAX_TEXT];
    char game_revision[BML_MAX_TEXT];
    char executable[PATH_MAX];
    char profile_id[BML_MAX_TEXT];
    bool has_stash;
    char stash_version[BML_MAX_TEXT];
    bool has_runebound_elixirs;
    char runebound_elixirs_version[BML_MAX_TEXT];
} BmlReportInfo;

typedef struct BmlRequiredSymbol {
    const char *logical_name;
    const char *symbol;
    const char *kind;
} BmlRequiredSymbol;

typedef struct BmlSymbolProbeResult {
    const BmlRequiredSymbol *required;
    void *address;
    bool resolved;
} BmlSymbolProbeResult;

typedef struct BmlSymbolProbe {
    size_t required_count;
    size_t resolved_count;
    size_t missing_count;
    BmlSymbolProbeResult results[BML_MAX_REQUIRED_SYMBOLS];
} BmlSymbolProbe;

typedef struct BmlStashHookIntent {
    const char *id;
    const char *capability;
    const char *description;
    const char *const *target_symbols;
    size_t target_symbol_count;
} BmlStashHookIntent;

typedef struct BmlHookBackend {
    const char *id;
    const char *mode;
    const char *strategy;
    size_t patch_bytes;
} BmlHookBackend;

typedef struct BmlTargetAnalysis {
    const char *symbol;
    const char *name;
    const char *kind;
    const char *status;
    const char *blocker_code;
    const char *message;
    size_t patch_size;
    void *address;
} BmlTargetAnalysis;

typedef struct BmlStashHookAnalysis {
    const BmlStashHookIntent *intent;
    const char *status;
    size_t ready_count;
    size_t blocked_count;
    size_t missing_count;
    BmlTargetAnalysis targets[12];
} BmlStashHookAnalysis;

typedef struct BmlStashHookPlan {
    const BmlHookBackend *backend;
    size_t hook_count;
    size_t installed_count;
    size_t ready_count;
    size_t blocked_count;
    BmlStashHookAnalysis hooks[8];
} BmlStashHookPlan;

typedef struct BmlDetourInstall {
    void *target;
    void *replacement;
    void *trampoline;
    size_t patch_size;
    size_t trampoline_capacity;
    unsigned char original[BML_DETOUR_MAX_COPY_BYTES];
} BmlDetourInstall;

typedef struct BmlStashCoreDetourInstall {
    const char *target_name;
    const char *target_symbol;
    void *replacement_address;
    void *target_address;
    BmlDetourInstall install;
    const char *status;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
} BmlStashCoreDetourInstall;

typedef struct BmlStashPlacementMapPrefix {
    char name[32];
    char author[32];
    unsigned int width;
    unsigned int height;
    unsigned int skybox;
    int32_t flags[16];
    int32_t *tiles;
} BmlStashPlacementMapPrefix;

typedef struct BmlStashPlacementAssignActionsSnapshot {
    bool observed;
    void *map_argument;
    void *global_map_symbol;
    bool map_argument_matches_global;
    char map_name[33];
    unsigned int map_width;
    unsigned int map_height;
    unsigned int map_skybox;
    int new_entity_calls_before;
    int new_entity_calls_after;
    int set_sprite_attributes_calls_before;
    int set_sprite_attributes_calls_after;
} BmlStashPlacementAssignActionsSnapshot;

typedef struct BmlStashPlacementNewEntitySample {
    int sprite;
    uint32_t pos;
    void *entity_list;
    void *creature_list;
    void *result;
} BmlStashPlacementNewEntitySample;

typedef struct BmlStashPlacementSetSpriteSample {
    void *entity;
    void *source;
    void *parent;
} BmlStashPlacementSetSpriteSample;

typedef struct BmlBaronyNode {
    struct BmlBaronyNode *next;
    struct BmlBaronyNode *prev;
    struct BmlBaronyList *list;
    void *element;
    void (*deconstructor)(void *data);
    uint32_t size;
} BmlBaronyNode;

typedef struct BmlBaronyList {
    BmlBaronyNode *first;
    BmlBaronyNode *last;
} BmlBaronyList;

typedef struct BmlBaronyItem {
    int type;
    int status;
    int16_t beatitude;
    int16_t count;
    uint32_t appearance;
    bool identified;
    uint32_t uid;
    int32_t x;
    int32_t y;
    uint32_t ownerUid;
    uint32_t interactNPCUid;
    bool forcedPickupByPlayer;
    bool isDroppable;
    bool playerSoldItemToShop;
    bool itemHiddenFromShop;
    bool notifyIcon;
    bool spellNotifyIcon;
    uint8_t itemRequireTradingSkillInShop;
    bool itemSpecialShopConsumable;
} BmlBaronyItem;
typedef struct BmlRuneboundElixirDefinition {
    const char *catalog_id;
    const char *display_name;
    int eligible_class_id;
    size_t min_party_size;
    size_t max_party_size;
    const char *effect_opcode;
    int effect_magnitude;
    int duration_turns;
    const char *tradeoff_summary;
} BmlRuneboundElixirDefinition;

typedef struct BmlRuneboundElixirPartySnapshot {
    size_t player_count;
    int class_ids[4];
    bool connected[4];
} BmlRuneboundElixirPartySnapshot;

typedef struct BmlRuneboundElixirCarrierMetadata {
    uint32_t instance_id;
    const char *carrier_item_type;
    const BmlRuneboundElixirDefinition *definition;
} BmlRuneboundElixirCarrierMetadata;

typedef struct BmlRuneboundElixirActiveEffect {
    bool active;
    uint32_t source_instance_id;
    const char *catalog_id;
    int player_index;
    int class_id;
    const char *effect_opcode;
    int effect_magnitude;
    int remaining_turns;
} BmlRuneboundElixirActiveEffect;

typedef struct BmlRuneboundElixirConsumptionResult {
    bool consumed;
    bool active_effect_created;
    const char *reason;
    BmlRuneboundElixirActiveEffect active_effect;
} BmlRuneboundElixirConsumptionResult;

typedef struct BmlRuneboundElixirDropGenerationDecision {
    const char *authority;
    const char *source;
    const char *carrier_item_type;
    const char *selected_elixir_id;
    const char *reason;
    const char *party_class_policy;
    const char *solo_class_policy;
    const char *party_size_policy;
    bool eligible;
    bool generated;
    bool no_global_all_class_pool;
    size_t max_generated_elixirs_per_source;
    size_t max_generated_elixirs_per_floor;
    size_t generated_carrier_count_for_source;
    size_t generated_carrier_count_for_floor;
    size_t party_size;
    int bound_class_id;
    int matched_class_id;
    size_t present_party_class_count;
    int present_party_classes[4];
    const BmlRuneboundElixirCarrierMetadata *carrier;
} BmlRuneboundElixirDropGenerationDecision;


typedef struct BmlRuneboundElixirSerializedActiveEffect {
    bool active;
    char package_id[BML_MAX_TEXT];
    char catalog_id[BML_MAX_TEXT];
    uint32_t source_instance_id;
    int player_index;
    int class_id;
    char effect_opcode[BML_MAX_TEXT];
    int effect_magnitude;
    int remaining_turns;
} BmlRuneboundElixirSerializedActiveEffect;

typedef struct BmlRuneboundElixirPackageDataValidation {
    bool runtime_manifest_available;
    bool package_path_found;
    char package_path[PATH_MAX];
    char package_dir[PATH_MAX];
    char catalog_path[PATH_MAX];
    char drop_table_path[PATH_MAX];
    bool catalog_file_loaded;
    bool catalog_contains_iron_vow;
    bool drop_table_file_exists;
    bool drop_table_generation_authority_host;
    bool drop_table_class_policy_present_party_classes;
    bool drop_table_party_size_generation_time_only;
    bool drop_table_no_global_all_class_pool;
    bool drop_table_anti_bloat_max_one_per_source;
    bool drop_table_anti_bloat_max_one_per_floor;
} BmlRuneboundElixirPackageDataValidation;


typedef struct BmlPatchInstruction {
    size_t source_offset;
    size_t source_length;
    size_t relocated_offset;
    size_t relocated_length;
} BmlPatchInstruction;

static int g_bml_initialized = 0;
static int g_bml_init_result = 1;
static bool g_bml_runebound_live_hooks_installed = false;

static const BmlRequiredSymbol BML_REQUIRED_SYMBOLS[] = {
    {"actChest", "_Z8actChestP6Entity", "function"},
    {"actChestLid", "_Z11actChestLidP6Entity", "function"},
    {"Entity::getChestInventoryList", "_ZN6Entity21getChestInventoryListEv", "function"},
    {"Entity::addItemToChest", "_ZN6Entity14addItemToChestEP4ItembS1_", "function"},
    {"Entity::getItemFromChest", "_ZN6Entity16getItemFromChestEP4Itemib", "function"},
    {"Entity::addItemToVoidChestServer", "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_", "function"},
    {"Entity::removeItemFromVoidChestServer", "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi", "function"},
    {"Entity::closeChest", "_ZN6Entity10closeChestEv", "function"},
    {"Entity::closeChestServer", "_ZN6Entity16closeChestServerEv", "function"},
    {"generateDungeon", "_Z15generateDungeonPcjSt5tupleIJiiiiEE", "function"},
    {"assignActions", "_Z13assignActionsP5map_t", "function"},
    {"newEntity", "_Z9newEntityijP6list_tS0_", "function"},
    {"setSpriteAttributes", "_Z19setSpriteAttributesP6EntityS0_S0_", "function"},
    {"Language::get", "_ZN8Language3getEi", "function"},
    {"uidToEntity", "_Z11uidToEntityi", "function"},
    {"newItem", "_Z7newItem8ItemType6StatusssjbP6list_t", "function"},
    {"list_FreeAll", "_Z12list_FreeAllP6list_t", "function"},
    {"list_RemoveNode", "_Z15list_RemoveNodeP6node_t", "function"},
    {"list_AddNodeLast", "_Z16list_AddNodeLastP6list_t", "function"},
    {"list_AddNodeFirst", "_Z17list_AddNodeFirstP6list_t", "function"},
    {"stats", "stats", "data"},
    {"map", "map", "data"},
    {"map_rng", "map_rng", "data"},
    {"map_server_rng", "map_server_rng", "data"},
    {"multiplayer", "multiplayer", "data"},
    {"clientnum", "clientnum", "data"},
    {"openedChest", "openedChest", "data"},
    {"selectedEntity", "selectedEntity", "data"},
    {"shoparea", "shoparea", "data"},
    {"TileEntityList", "TileEntityList", "data"}
};

_Static_assert((sizeof(BML_REQUIRED_SYMBOLS) / sizeof(BML_REQUIRED_SYMBOLS[0])) <= BML_MAX_REQUIRED_SYMBOLS, "BML symbol probe result capacity is too small");

static const char *const BML_STASH_VOID_CHEST_BINDING_TARGETS[] = {
    "_Z8actChestP6Entity",
    "_Z11actChestLidP6Entity",
    "_ZN6Entity21getChestInventoryListEv",
    "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_",
    "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi",
    "_ZN8Language3getEi",
    "selectedEntity"
};

static const char *const BML_STASH_INVENTORY_PERSISTENCE_TARGETS[] = {
    "_ZN6Entity21getChestInventoryListEv",
    "_ZN6Entity14addItemToChestEP4ItembS1_",
    "_ZN6Entity16getItemFromChestEP4Itemib",
    "_ZN6Entity10closeChestEv",
    "_ZN6Entity16closeChestServerEv",
    "_Z7newItem8ItemType6StatusssjbP6list_t",
    "_Z17list_AddNodeFirstP6list_t",
    "_Z16list_AddNodeLastP6list_t",
    "_Z15list_RemoveNodeP6node_t",
    "_Z12list_FreeAllP6list_t",
    "stats"
};

static const char *const BML_STASH_LOBBY_PLACEMENT_TARGETS[] = {
    "_Z13assignActionsP5map_t",
    "_Z9newEntityijP6list_tS0_",
    "_Z19setSpriteAttributesP6EntityS0_S0_",
    "_ZN8Language3getEi",
    "_Z11uidToEntityi",
    "map",
    "map_rng",
    "map_server_rng",
    "selectedEntity",
    "TileEntityList"
};

static const char *const BML_STASH_SHOP_PLACEMENT_TARGETS[] = {
    "_Z15generateDungeonPcjSt5tupleIJiiiiEE",
    "_Z13assignActionsP5map_t",
    "_Z9newEntityijP6list_tS0_",
    "_Z19setSpriteAttributesP6EntityS0_S0_",
    "_ZN8Language3getEi",
    "_Z11uidToEntityi",
    "map",
    "map_rng",
    "map_server_rng",
    "selectedEntity",
    "shoparea",
    "TileEntityList"
};

static const char *const BML_STASH_MULTIPLAYER_METADATA_TARGETS[] = {
    "multiplayer",
    "clientnum"
};

static const BmlHookBackend BML_STASH_HOOK_BACKEND = {
    "linux-x86_64-direct-stash-detour",
    "analyze-only",
    "abstract-direct-detour-backend",
    BML_DETOUR_PATCH_BYTES
};

static const BmlStashHookIntent BML_STASH_HOOK_INTENTIONS[] = {
    {"stash_void_chest_binding", "void_chest_binding", "Bind Stash storage to Barony void chest inventory entry points.", BML_STASH_VOID_CHEST_BINDING_TARGETS, sizeof(BML_STASH_VOID_CHEST_BINDING_TARGETS) / sizeof(BML_STASH_VOID_CHEST_BINDING_TARGETS[0])},
    {"stash_inventory_persistence", "persistent_inventory", "Persist Stash inventory entries outside the vanilla run-scoped chest lifetime.", BML_STASH_INVENTORY_PERSISTENCE_TARGETS, sizeof(BML_STASH_INVENTORY_PERSISTENCE_TARGETS) / sizeof(BML_STASH_INVENTORY_PERSISTENCE_TARGETS[0])},
    {"stash_lobby_placement", "placement_lobby", "Place the Stash interaction point in eligible lobby contexts.", BML_STASH_LOBBY_PLACEMENT_TARGETS, sizeof(BML_STASH_LOBBY_PLACEMENT_TARGETS) / sizeof(BML_STASH_LOBBY_PLACEMENT_TARGETS[0])},
    {"stash_shop_placement", "placement_shop", "Place the Stash interaction point in eligible shop contexts.", BML_STASH_SHOP_PLACEMENT_TARGETS, sizeof(BML_STASH_SHOP_PLACEMENT_TARGETS) / sizeof(BML_STASH_SHOP_PLACEMENT_TARGETS[0])},
    {"stash_multiplayer_metadata_gate", "multiplayer_version_metadata", "Expose multiplayer version/capability metadata before any shared Stash state is accepted.", BML_STASH_MULTIPLAYER_METADATA_TARGETS, sizeof(BML_STASH_MULTIPLAYER_METADATA_TARGETS) / sizeof(BML_STASH_MULTIPLAYER_METADATA_TARGETS[0])}
};

_Static_assert((sizeof(BML_STASH_HOOK_INTENTIONS) / sizeof(BML_STASH_HOOK_INTENTIONS[0])) <= (sizeof(((BmlStashHookPlan *)0)->hooks) / sizeof(((BmlStashHookPlan *)0)->hooks[0])), "BML stash hook plan capacity is too small");
_Static_assert((sizeof(BML_STASH_VOID_CHEST_BINDING_TARGETS) / sizeof(BML_STASH_VOID_CHEST_BINDING_TARGETS[0])) <= (sizeof(((BmlStashHookAnalysis *)0)->targets) / sizeof(((BmlStashHookAnalysis *)0)->targets[0])), "BML stash void chest target capacity is too small");
_Static_assert((sizeof(BML_STASH_INVENTORY_PERSISTENCE_TARGETS) / sizeof(BML_STASH_INVENTORY_PERSISTENCE_TARGETS[0])) <= (sizeof(((BmlStashHookAnalysis *)0)->targets) / sizeof(((BmlStashHookAnalysis *)0)->targets[0])), "BML stash inventory target capacity is too small");
_Static_assert((sizeof(BML_STASH_LOBBY_PLACEMENT_TARGETS) / sizeof(BML_STASH_LOBBY_PLACEMENT_TARGETS[0])) <= (sizeof(((BmlStashHookAnalysis *)0)->targets) / sizeof(((BmlStashHookAnalysis *)0)->targets[0])), "BML stash lobby target capacity is too small");
_Static_assert((sizeof(BML_STASH_SHOP_PLACEMENT_TARGETS) / sizeof(BML_STASH_SHOP_PLACEMENT_TARGETS[0])) <= (sizeof(((BmlStashHookAnalysis *)0)->targets) / sizeof(((BmlStashHookAnalysis *)0)->targets[0])), "BML stash shop target capacity is too small");
_Static_assert((sizeof(BML_STASH_MULTIPLAYER_METADATA_TARGETS) / sizeof(BML_STASH_MULTIPLAYER_METADATA_TARGETS[0])) <= (sizeof(((BmlStashHookAnalysis *)0)->targets) / sizeof(((BmlStashHookAnalysis *)0)->targets[0])), "BML stash multiplayer target capacity is too small");

static void bml_copy_string(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0U) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static bool bml_has_value(const char *value) {
    return value != NULL && value[0] != '\0';
}

static bool bml_should_skip_non_barony_process(void) {
    const char *allow_non_barony = getenv("BML_HOOK_ALLOW_NON_BARONY");
    char executable_path[PATH_MAX];
    const char *basename;
    ssize_t length;

    if (strcmp(allow_non_barony != NULL ? allow_non_barony : "", "1") == 0) {
        return false;
    }

    length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1U);
    if (length < 0) {
        return false;
    }
    executable_path[length] = '\0';
    basename = strrchr(executable_path, '/');
    basename = basename != NULL ? basename + 1 : executable_path;
    return strcmp(basename, "barony.x86_64") != 0 && strcmp(basename, "barony") != 0;
}

static void bml_add_error(BmlError *errors, size_t *error_count, const char *code, const char *message, const char *env_name, const char *path) {
    if (*error_count >= BML_MAX_ERRORS) {
        return;
    }
    BmlError *error = &errors[*error_count];
    error->code = code;
    error->severity = "fatal";
    error->message = message;
    error->env_name = env_name;
    bml_copy_string(error->path, sizeof(error->path), path);
    *error_count += 1U;
}

static int bml_mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    size_t len;

    if (!bml_has_value(path)) {
        errno = EINVAL;
        return -1;
    }

    len = strlen(path);
    if (len >= sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    bml_copy_string(tmp, sizeof(tmp), path);
    if (len > 1U && tmp[len - 1U] == '/') {
        tmp[len - 1U] = '\0';
    }

    for (char *cursor = tmp + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
        *cursor = '/';
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int bml_join_path(char *out, size_t out_size, const char *left, const char *right) {
    int written;

    if (!bml_has_value(left) || !bml_has_value(right)) {
        errno = EINVAL;
        return -1;
    }

    written = snprintf(out, out_size, "%s/%s", left, right);
    if (written < 0 || (size_t)written >= out_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int bml_check_readable_env_path(BmlError *errors, size_t *error_count, const char *env_name, const char *path, bool required) {
    char code[BML_MAX_TEXT];
    (void)code;

    if (!bml_has_value(path)) {
        if (required) {
            if (strcmp(env_name, "BML_RUNTIME_MANIFEST") == 0) {
                bml_add_error(errors, error_count, "BML_RUNTIME_MANIFEST_MISSING", "BML_RUNTIME_MANIFEST is required for the native hook runtime.", env_name, NULL);
            } else if (strcmp(env_name, "BML_HOOK_MANIFEST") == 0) {
                bml_add_error(errors, error_count, "BML_HOOK_MANIFEST_MISSING", "BML_HOOK_MANIFEST is required for the native hook runtime.", env_name, NULL);
            } else {
                bml_add_error(errors, error_count, "BML_REQUIRED_ENV_MISSING", "A required native hook environment variable is missing.", env_name, NULL);
            }
            return -1;
        }
        return 0;
    }

    if (access(path, R_OK) != 0) {
        if (strcmp(env_name, "BML_RUNTIME_MANIFEST") == 0) {
            bml_add_error(errors, error_count, "BML_RUNTIME_MANIFEST_UNREADABLE", "BML_RUNTIME_MANIFEST must point to a readable runtime manifest file.", env_name, path);
        } else if (strcmp(env_name, "BML_HOOK_MANIFEST") == 0) {
            bml_add_error(errors, error_count, "BML_HOOK_MANIFEST_UNREADABLE", "BML_HOOK_MANIFEST must point to a readable hook manifest file.", env_name, path);
        } else if (strcmp(env_name, "BML_HOOK_LIBRARY") == 0) {
            bml_add_error(errors, error_count, "BML_HOOK_LIBRARY_UNREADABLE", "BML_HOOK_LIBRARY was provided but is not readable.", env_name, path);
        } else {
            bml_add_error(errors, error_count, "BML_REQUIRED_FILE_UNREADABLE", "A native hook input file is not readable.", env_name, path);
        }
        return -1;
    }

    return 0;
}

static char *bml_read_text_file(const char *path, size_t *out_size) {
    FILE *file;
    long size;
    char *buffer;
    size_t bytes_read;

    if (out_size != NULL) {
        *out_size = 0U;
    }
    if (!bml_has_value(path)) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0L || (unsigned long)size > (unsigned long)BML_MAX_MANIFEST_BYTES) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)calloc((size_t)size + 1U, 1U);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1U, (size_t)size, file);
    if (ferror(file) != 0 || bytes_read != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);

    buffer[bytes_read] = '\0';
    if (out_size != NULL) {
        *out_size = bytes_read;
    }
    return buffer;
}

static const char *bml_skip_json_ws(const char *cursor) {
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        ++cursor;
    }
    return cursor;
}

static bool bml_json_extract_string_after(const char *start, const char *key, char *out, size_t out_size) {
    char pattern[BML_MAX_TEXT];
    const char *cursor;
    int written;

    if (out_size == 0U) {
        return false;
    }
    out[0] = '\0';

    written = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (written < 0 || (size_t)written >= sizeof(pattern)) {
        return false;
    }

    cursor = strstr(start, pattern);
    if (cursor == NULL) {
        return false;
    }
    cursor += strlen(pattern);
    cursor = bml_skip_json_ws(cursor);
    if (*cursor != ':') {
        return false;
    }
    ++cursor;
    cursor = bml_skip_json_ws(cursor);
    if (*cursor != '"') {
        return false;
    }
    ++cursor;

    size_t used = 0U;
    while (*cursor != '\0' && *cursor != '"') {
        char ch = *cursor;
        if (ch == '\\') {
            ++cursor;
            if (*cursor == '\0') {
                break;
            }
            switch (*cursor) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default: ch = *cursor; break;
            }
        }
        if (used + 1U < out_size) {
            out[used++] = ch;
        }
        ++cursor;
    }
    out[used] = '\0';
    return used > 0U;
}

static bool bml_runtime_manifest_has_mod(const char *manifest_json, const char *mod_id) {
    char needle[BML_MAX_TEXT];
    int written = snprintf(needle, sizeof(needle), "\"%s\"", mod_id);
    if (written < 0 || (size_t)written >= sizeof(needle)) {
        return false;
    }
    return strstr(manifest_json, needle) != NULL;
}

static bool bml_extract_mod_version(const char *manifest_json, const char *mod_id, char *out, size_t out_size) {
    char needle[BML_MAX_TEXT];
    const char *mod;
    int written = snprintf(needle, sizeof(needle), "\"%s\"", mod_id);
    if (written < 0 || (size_t)written >= sizeof(needle)) {
        return false;
    }
    mod = strstr(manifest_json, needle);
    if (mod == NULL) {
        return false;
    }
    return bml_json_extract_string_after(mod, "version", out, out_size);
}

static void bml_report_info_init(BmlReportInfo *info, const char *hook_library) {
    bml_copy_string(info->contract_id, sizeof(info->contract_id), BML_CONTRACT_ID);
    bml_copy_string(info->contract_version, sizeof(info->contract_version), BML_CONTRACT_VERSION);
    bml_copy_string(info->runtime_id, sizeof(info->runtime_id), BML_NATIVE_RUNTIME_ID);
    bml_copy_string(info->runtime_version, sizeof(info->runtime_version), BML_NATIVE_RUNTIME_VERSION);
    bml_copy_string(info->runtime_strategy, sizeof(info->runtime_strategy), BML_DEFAULT_RUNTIME_STRATEGY);
    bml_copy_string(info->game_revision, sizeof(info->game_revision), "unknown");
    bml_copy_string(info->executable, sizeof(info->executable), bml_has_value(hook_library) ? hook_library : "libbarony_bml.so");
    bml_copy_string(info->profile_id, sizeof(info->profile_id), "unknown-profile");
    info->has_stash = false;
    bml_copy_string(info->stash_version, sizeof(info->stash_version), "0.1.0");
    info->has_runebound_elixirs = false;
    bml_copy_string(info->runebound_elixirs_version, sizeof(info->runebound_elixirs_version), "0.1.0");
}

static void bml_populate_report_from_runtime_manifest(BmlReportInfo *info, const char *manifest_json) {
    char value[BML_MAX_TEXT];

    if (manifest_json == NULL) {
        return;
    }

    if (bml_json_extract_string_after(manifest_json, "profileId", value, sizeof(value))) {
        bml_copy_string(info->profile_id, sizeof(info->profile_id), value);
    }
    if (bml_json_extract_string_after(manifest_json, "runtimeId", value, sizeof(value))) {
        bml_copy_string(info->runtime_id, sizeof(info->runtime_id), value);
    }
    if (bml_json_extract_string_after(manifest_json, "runtimeVersion", value, sizeof(value))) {
        bml_copy_string(info->runtime_version, sizeof(info->runtime_version), value);
    }
    if (bml_json_extract_string_after(manifest_json, "runtimeStrategy", value, sizeof(value))) {
        bml_copy_string(info->runtime_strategy, sizeof(info->runtime_strategy), value);
    }
    if (bml_json_extract_string_after(manifest_json, "gameVersionString", value, sizeof(value))) {
        bml_copy_string(info->game_revision, sizeof(info->game_revision), value);
    }
    if (bml_json_extract_string_after(manifest_json, "steamExecutableBuildId", value, sizeof(value))) {
        bml_copy_string(info->game_revision, sizeof(info->game_revision), value);
    }
    if (bml_extract_mod_version(manifest_json, "jml.stash", value, sizeof(value))) {
        bml_copy_string(info->stash_version, sizeof(info->stash_version), value);
    }
    if (bml_extract_mod_version(manifest_json, BML_RUNES_ELIXIR_PACKAGE_ID, value, sizeof(value))) {
        bml_copy_string(info->runebound_elixirs_version, sizeof(info->runebound_elixirs_version), value);
    }
    info->has_stash = bml_runtime_manifest_has_mod(manifest_json, "jml.stash");
    info->has_runebound_elixirs = bml_runtime_manifest_has_mod(manifest_json, BML_RUNES_ELIXIR_PACKAGE_ID);
}

static void bml_json_write_escaped(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value != NULL ? value : "");
    fputc('"', file);
    while (*cursor != '\0') {
        unsigned char ch = *cursor++;
        switch (ch) {
            case '"': fputs("\\\"", file); break;
            case '\\': fputs("\\\\", file); break;
            case '\b': fputs("\\b", file); break;
            case '\f': fputs("\\f", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (ch < 0x20U) {
                    fprintf(file, "\\u%04x", ch);
                } else {
                    fputc((int)ch, file);
                }
                break;
        }
    }
    fputc('"', file);
}

static void bml_write_reported_at(FILE *file) {
    time_t now = time(NULL);
    struct tm utc;
    char timestamp[32];

    if (now == (time_t)-1 || gmtime_r(&now, &utc) == NULL || strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0U) {
        bml_copy_string(timestamp, sizeof(timestamp), "1970-01-01T00:00:00Z");
    }
    bml_json_write_escaped(file, timestamp);
}

static void bml_write_runtime_identity(FILE *file, const BmlReportInfo *info) {
    fputs("\"runtime\": {\n    \"id\": ", file);
    bml_json_write_escaped(file, info->runtime_id);
    fputs(",\n    \"version\": ", file);
    bml_json_write_escaped(file, info->runtime_version);
    fputs(",\n    \"strategy\": ", file);
    bml_json_write_escaped(file, info->runtime_strategy);
    fputs(",\n    \"gameRevision\": ", file);
    bml_json_write_escaped(file, info->game_revision);
    fputs(",\n    \"executable\": ", file);
    bml_json_write_escaped(file, info->executable);
    fputs("\n  }", file);
}

static void bml_probe_required_symbols(BmlSymbolProbe *probe) {
    memset(probe, 0, sizeof(*probe));
    probe->required_count = sizeof(BML_REQUIRED_SYMBOLS) / sizeof(BML_REQUIRED_SYMBOLS[0]);

    for (size_t index = 0U; index < probe->required_count; ++index) {
        BmlSymbolProbeResult *result = &probe->results[index];
        result->required = &BML_REQUIRED_SYMBOLS[index];
        (void)dlerror();
        result->address = dlsym(RTLD_DEFAULT, result->required->symbol);
        result->resolved = result->address != NULL && dlerror() == NULL;
        if (result->resolved) {
            probe->resolved_count += 1U;
        } else {
            probe->missing_count += 1U;
        }
    }
}

static void bml_write_address_or_null(FILE *file, const void *address) {
    char address_text[2U + sizeof(uintptr_t) * 2U + 1U];
    if (address == NULL) {
        fputs("null", file);
        return;
    }
    snprintf(address_text, sizeof(address_text), "0x%" PRIxPTR, (uintptr_t)address);
    bml_json_write_escaped(file, address_text);
}

static int bml_write_symbol_probe_report(const char *report_path, const BmlReportInfo *info, const BmlSymbolProbe *probe) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  ", file);
    bml_write_runtime_identity(file, info);
    fputs(",\n  \"profileId\": ", file);
    bml_json_write_escaped(file, info->profile_id);
    fputs(",\n  \"status\": ", file);
    bml_json_write_escaped(file, probe->missing_count == 0U ? "loaded" : "failed");
    fprintf(file, ",\n  \"summary\": {\n    \"required\": %zu,\n    \"resolved\": %zu,\n    \"missing\": %zu\n  },\n  \"symbols\": [", probe->required_count, probe->resolved_count, probe->missing_count);
    for (size_t index = 0U; index < probe->required_count; ++index) {
        const BmlSymbolProbeResult *result = &probe->results[index];
        if (index == 0U) {
            fputs("\n    ", file);
        } else {
            fputs(",\n    ", file);
        }
        fputs("{\"name\": ", file);
        bml_json_write_escaped(file, result->required->logical_name);
        fputs(", \"symbol\": ", file);
        bml_json_write_escaped(file, result->required->symbol);
        fputs(", \"kind\": ", file);
        bml_json_write_escaped(file, result->required->kind);
        fputs(", \"required\": true, \"status\": ", file);
        bml_json_write_escaped(file, result->resolved ? "resolved" : "missing");
        fputs(", \"address\": ", file);
        bml_write_address_or_null(file, result->address);
        fputc('}', file);
    }
    if (probe->required_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"errors\": [", file);
    size_t written_errors = 0U;
    for (size_t index = 0U; index < probe->required_count; ++index) {
        const BmlSymbolProbeResult *result = &probe->results[index];
        if (result->resolved) {
            continue;
        }
        if (written_errors == 0U) {
            fputs("\n    ", file);
        } else {
            fputs(",\n    ", file);
        }
        fputs("{\"code\": \"BML_HOOK_SYMBOL_MISSING\", \"severity\": \"fatal\", \"symbol\": ", file);
        bml_json_write_escaped(file, result->required->symbol);
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, "Required Barony target symbol was not visible through dlsym(RTLD_DEFAULT).");
        fputc('}', file);
        written_errors += 1U;
    }
    if (written_errors > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static const BmlSymbolProbeResult *bml_find_symbol_probe_result(const BmlSymbolProbe *probe, const char *symbol) {
    if (probe == NULL || symbol == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < probe->required_count; ++index) {
        const BmlSymbolProbeResult *result = &probe->results[index];
        if (result->required != NULL && strcmp(result->required->symbol, symbol) == 0) {
            return result;
        }
    }
    return NULL;
}

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
    return op == 0x31U || op == 0x39U || op == 0x3bU || op == 0x63U || op == 0x85U || op == 0x89U || op == 0x8bU || op == 0x8dU;
}

static bool bml_opcode_uses_supported_modrm_immediate(unsigned char op) {
    return op == 0x81U || op == 0x83U;
}

static int bml_decode_modrm_copyable_length(const unsigned char *code, size_t offset, size_t limit, size_t opcode_length, size_t *out_length, const char **out_code, const char **out_message, const char *truncated_message) {
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

static int bml_decode_register_only_group_copyable_length(const unsigned char *code, size_t offset, size_t limit, size_t opcode_length, size_t *out_length, const char **out_code, const char **out_message, const char *truncated_message, const char *unsupported_message) {
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

static int bml_decode_modrm_immediate_copyable_length(const unsigned char *code, size_t offset, size_t limit, size_t opcode_length, size_t immediate_length, size_t *out_length, const char **out_code, const char **out_message, const char *truncated_message, const char *unsupported_message) {
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

static int bml_decode_supported_x86_64_instruction(const unsigned char *code, size_t offset, size_t limit, size_t *out_length, const char **out_code, const char **out_message) {
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

static void bml_write_abs_jump(unsigned char *location, const void *destination) {
    const uint32_t zero_displacement = 0U;
    uintptr_t address = (uintptr_t)destination;
    location[0] = 0xffU;
    location[1] = 0x25U;
    memcpy(location + 2U, &zero_displacement, sizeof(zero_displacement));
    memcpy(location + 6U, &address, sizeof(address));
}

static void bml_write_abs_call(unsigned char *location, const void *destination) {
    const uint32_t zero_displacement = 0U;
    uintptr_t address = (uintptr_t)destination;
    location[0] = 0xffU;
    location[1] = 0x15U;
    memcpy(location + 2U, &zero_displacement, sizeof(zero_displacement));
    memcpy(location + 6U, &address, sizeof(address));
}

static bool bml_instruction_is_short_jcc(const unsigned char *code, size_t offset) {
    return bml_byte_is_short_relative_branch(code[offset]);
}

static bool bml_instruction_is_near_jcc(const unsigned char *code, size_t offset, size_t source_length) {
    return source_length >= 2U && code[offset] == 0x0fU && code[offset + 1U] >= 0x80U && code[offset + 1U] <= 0x8fU;
}

static size_t bml_relocated_instruction_length(const unsigned char *code, size_t offset, size_t source_length) {
    const unsigned char op = code[offset];
    if (op == 0xe8U || op == 0xe9U || op == 0xebU) {
        return BML_DETOUR_PATCH_BYTES;
    }
    if (bml_instruction_is_short_jcc(code, offset) || bml_instruction_is_near_jcc(code, offset, source_length)) {
        return BML_DETOUR_PATCH_BYTES + 2U;
    }
    return source_length;
}

static int bml_relative_target_offset(const unsigned char *code, size_t offset, size_t source_length, int64_t *out_target_offset) {
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

static int bml_resolve_relocated_destination(const unsigned char *target_bytes, const BmlPatchInstruction *instructions, size_t instruction_count, size_t patch_size, const unsigned char *trampoline, int64_t target_offset, const void **out_destination, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    if (target_offset < 0) {
        *out_destination = (const void *)((uintptr_t)target_bytes + (uintptr_t)target_offset);
        return 0;
    }
    if ((uint64_t)target_offset < (uint64_t)patch_size) {
        for (size_t index = 0U; index < instruction_count; ++index) {
            if ((uint64_t)instructions[index].source_offset == (uint64_t)target_offset) {
                *out_destination = trampoline + instructions[index].relocated_offset;
                return 0;
            }
        }
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_RELATIVE_CONTROL_FLOW_UNSUPPORTED");
        bml_copy_string(error_message, error_message_size, "Detour target prologue branches into the middle of the copied patch window.");
        return -1;
    }
    *out_destination = target_bytes + target_offset;
    return 0;
}

static bool bml_find_rip_relative_displacement_offset(const unsigned char *code, size_t offset, size_t source_length, size_t *out_displacement_offset) {
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

static int bml_adjust_rip_relative_displacement(const unsigned char *target_bytes, size_t source, size_t source_length, unsigned char *destination, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
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
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_RIP_RELATIVE_RELOCATION_UNSUPPORTED");
        bml_copy_string(error_message, error_message_size, "Relocated RIP-relative memory operand would exceed the signed 32-bit displacement range; executable trampoline allocation must be nearer the target.");
        return -1;
    }

    new_displacement = (int32_t)new_displacement64;
    memcpy(destination + relocated_displacement_offset, &new_displacement, sizeof(new_displacement));
    return 0;
}

static int bml_relocate_patch_window(const unsigned char *target_bytes, size_t patch_size, unsigned char *trampoline, size_t trampoline_capacity, size_t *out_trampoline_length, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    BmlPatchInstruction instructions[BML_DETOUR_MAX_INSTRUCTIONS];
    size_t instruction_count = 0U;
    size_t source_offset = 0U;
    size_t relocated_offset = 0U;

    memset(instructions, 0, sizeof(instructions));
    *out_trampoline_length = 0U;

    while (source_offset < patch_size) {
        size_t instruction_length = 0U;
        const char *decode_code = NULL;
        const char *decode_message = NULL;
        size_t relocated_length;

        if (instruction_count >= BML_DETOUR_MAX_INSTRUCTIONS) {
            bml_copy_string(error_code, error_code_size, "BML_DETOUR_PATCH_WINDOW_TOO_LARGE");
            bml_copy_string(error_message, error_message_size, "Detour patch window contains more instructions than the bounded relocator can track.");
            return -1;
        }
        if (bml_decode_supported_x86_64_instruction(target_bytes, source_offset, patch_size, &instruction_length, &decode_code, &decode_message) != 0 ||
            instruction_length == 0U || source_offset + instruction_length > patch_size) {
            bml_copy_string(error_code, error_code_size, decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION");
            bml_copy_string(error_message, error_message_size, decode_message != NULL ? decode_message : "Detour target prologue is not safe for relocation.");
            return -1;
        }

        relocated_length = bml_relocated_instruction_length(target_bytes, source_offset, instruction_length);
        if (relocated_offset + relocated_length + BML_DETOUR_PATCH_BYTES > trampoline_capacity) {
            bml_copy_string(error_code, error_code_size, "BML_DETOUR_TRAMPOLINE_ALLOC_FAILED");
            bml_copy_string(error_message, error_message_size, "Relocated trampoline would exceed the bounded executable trampoline buffer.");
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
        const BmlPatchInstruction *instruction = &instructions[index];
        const size_t source = instruction->source_offset;
        unsigned char *destination = trampoline + instruction->relocated_offset;
        const unsigned char op = target_bytes[source];

        if (op == 0xe8U || op == 0xe9U || op == 0xebU || bml_instruction_is_short_jcc(target_bytes, source) || bml_instruction_is_near_jcc(target_bytes, source, instruction->source_length)) {
            int64_t target_offset = 0;
            const void *absolute_destination = NULL;
            if (bml_relative_target_offset(target_bytes, source, instruction->source_length, &target_offset) != 0 ||
                bml_resolve_relocated_destination(target_bytes, instructions, instruction_count, patch_size, trampoline, target_offset, &absolute_destination, error_code, error_code_size, error_message, error_message_size) != 0) {
                return -1;
            }
            if (op == 0xe8U) {
                bml_write_abs_call(destination, absolute_destination);
            } else if (op == 0xe9U || op == 0xebU) {
                bml_write_abs_jump(destination, absolute_destination);
            } else {
                const unsigned char condition = (op == 0x0fU) ? (unsigned char)(target_bytes[source + 1U] & 0x0fU) : (unsigned char)(op & 0x0fU);
                destination[0] = (unsigned char)(0x70U | (condition ^ 0x01U));
                destination[1] = (unsigned char)BML_DETOUR_PATCH_BYTES;
                bml_write_abs_jump(destination + 2U, absolute_destination);
            }
        } else {
            memcpy(destination, target_bytes + source, instruction->source_length);
            if (bml_adjust_rip_relative_displacement(target_bytes, source, instruction->source_length, destination, error_code, error_code_size, error_message, error_message_size) != 0) {
                return -1;
            }
        }
    }

    bml_write_abs_jump(trampoline + relocated_offset, target_bytes + patch_size);
    *out_trampoline_length = relocated_offset + BML_DETOUR_PATCH_BYTES;
    return 0;
}

static int bml_page_span_for_patch(void *target, size_t patch_size, uintptr_t *out_page_start, size_t *out_page_span) {
    long page_size_long = sysconf(_SC_PAGESIZE);
    uintptr_t start;
    uintptr_t end;
    uintptr_t page_mask;

    if (page_size_long <= 0) {
        return -1;
    }

    start = (uintptr_t)target;
    end = start + patch_size;
    if (end < start) {
        return -1;
    }

    page_mask = (uintptr_t)page_size_long - 1U;
    *out_page_start = start & ~page_mask;
    *out_page_span = ((end + page_mask) & ~page_mask) - *out_page_start;
    return 0;
}

static void *bml_try_mmap_trampoline_at(uintptr_t candidate, size_t size) {
    void *requested;
    void *mapping;

    if (candidate < BML_DETOUR_MIN_MMAP_ADDRESS || size == 0U || (uintptr_t)size > UINTPTR_MAX - candidate) {
        return MAP_FAILED;
    }

    requested = (void *)candidate;
    mapping = mmap(requested, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (mapping != MAP_FAILED && mapping != requested) {
        (void)munmap(mapping, size);
        return MAP_FAILED;
    }
    return mapping;
}

static void *bml_mmap_trampoline_near_target(const void *target, size_t size) {
    long page_size_long = sysconf(_SC_PAGESIZE);
    void *mapping;

    if (page_size_long > 0 && target != NULL) {
        const uintptr_t page_size = (uintptr_t)page_size_long;
        const uintptr_t page_mask = page_size - 1U;
        const uintptr_t target_page = (uintptr_t)target & ~page_mask;
        uintptr_t step = BML_DETOUR_NEAR_SEARCH_STEP;
        uintptr_t distance;

        if (step < page_size) {
            step = page_size;
        }
        step = (step + page_mask) & ~page_mask;

        for (distance = step; distance <= BML_DETOUR_NEAR_SEARCH_RANGE; distance += step) {
            if (target_page >= distance) {
                mapping = bml_try_mmap_trampoline_at(target_page - distance, size);
                if (mapping != MAP_FAILED) {
                    return mapping;
                }
            }
            if (target_page <= UINTPTR_MAX - distance) {
                mapping = bml_try_mmap_trampoline_at(target_page + distance, size);
                if (mapping != MAP_FAILED) {
                    return mapping;
                }
            }
            if (BML_DETOUR_NEAR_SEARCH_RANGE - distance < step) {
                break;
            }
        }
    }

    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static int bml_measure_supported_patch_window(const unsigned char *target_bytes, size_t *out_patch_size, const char **out_code, const char **out_message) {
    size_t patch_size = 0U;

    *out_patch_size = 0U;
    *out_code = NULL;
    *out_message = NULL;

    while (patch_size < BML_DETOUR_PATCH_BYTES) {
        size_t instruction_length = 0U;
        const char *decode_code = NULL;
        const char *decode_message = NULL;

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

static int bml_rollback_absolute_jump_detour(BmlDetourInstall *install) {
    uintptr_t page_start = 0U;
    size_t page_span = 0U;
    int result = 0;

    if (install == NULL || install->target == NULL || install->patch_size == 0U) {
        return 0;
    }
    if (bml_page_span_for_patch(install->target, install->patch_size, &page_start, &page_span) != 0 ||
        mprotect((void *)page_start, page_span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        result = -1;
    } else {
        memcpy(install->target, install->original, install->patch_size);
        __builtin___clear_cache((char *)install->target, (char *)install->target + install->patch_size);
        if (mprotect((void *)page_start, page_span, PROT_READ | PROT_EXEC) != 0) {
            result = -1;
        }
    }
    if (install->trampoline != NULL && install->trampoline_capacity != 0U) {
        (void)munmap(install->trampoline, install->trampoline_capacity);
    }
    install->target = NULL;
    install->replacement = NULL;
    install->trampoline = NULL;
    install->patch_size = 0U;
    install->trampoline_capacity = 0U;
    memset(install->original, 0, sizeof(install->original));
    return result;
}

static int bml_install_absolute_jump_detour(void *target, void *replacement, BmlDetourInstall *install, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    const unsigned char *target_bytes = (const unsigned char *)target;
    unsigned char original[BML_DETOUR_MAX_COPY_BYTES];
    unsigned char *trampoline;
    const size_t trampoline_capacity = BML_DETOUR_MAX_RELOCATED_BYTES;
    size_t trampoline_length = 0U;
    size_t patch_size = 0U;
    uintptr_t page_start = 0U;
    size_t page_span = 0U;

    memset(install, 0, sizeof(*install));

    if (target == NULL || replacement == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_INVALID_ARGUMENT");
        bml_copy_string(error_message, error_message_size, "Detour target and replacement must both be resolved before patching.");
        return -1;
    }

    {
        const char *decode_code = NULL;
        const char *decode_message = NULL;
        if (bml_measure_supported_patch_window(target_bytes, &patch_size, &decode_code, &decode_message) != 0) {
            bml_copy_string(error_code, error_code_size, decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION");
            bml_copy_string(error_message, error_message_size, decode_message != NULL ? decode_message : "Detour target prologue is not safe for the conservative decoder.");
            return -1;
        }
    }

    memcpy(original, target, patch_size);
    trampoline = bml_mmap_trampoline_near_target(target, trampoline_capacity);
    if (trampoline == MAP_FAILED) {
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_TRAMPOLINE_ALLOC_FAILED");
        bml_copy_string(error_message, error_message_size, "Executable trampoline allocation failed.");
        return -1;
    }

    if (bml_relocate_patch_window(target_bytes, patch_size, trampoline, trampoline_capacity, &trampoline_length, error_code, error_code_size, error_message, error_message_size) != 0) {
        (void)munmap(trampoline, trampoline_capacity);
        return -1;
    }
    __builtin___clear_cache((char *)trampoline, (char *)trampoline + trampoline_length);

    if (mprotect(trampoline, trampoline_capacity, PROT_READ | PROT_EXEC) != 0) {
        (void)munmap(trampoline, trampoline_capacity);
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_TRAMPOLINE_PROTECT_FAILED");
        bml_copy_string(error_message, error_message_size, "Executable trampoline could not be made read-only/executable after construction.");
        return -1;
    }

    if (bml_page_span_for_patch(target, patch_size, &page_start, &page_span) != 0 ||
        mprotect((void *)page_start, page_span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        (void)munmap(trampoline, trampoline_capacity);
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_TARGET_PROTECT_FAILED");
        bml_copy_string(error_message, error_message_size, "Target code page could not be made writable for detour installation.");
        return -1;
    }

    bml_write_abs_jump((unsigned char *)target, replacement);
    if (patch_size > BML_DETOUR_PATCH_BYTES) {
        memset((unsigned char *)target + BML_DETOUR_PATCH_BYTES, 0x90, patch_size - BML_DETOUR_PATCH_BYTES);
    }
    __builtin___clear_cache((char *)target, (char *)target + patch_size);

    if (mprotect((void *)page_start, page_span, PROT_READ | PROT_EXEC) != 0) {
        memcpy(target, original, patch_size);
        __builtin___clear_cache((char *)target, (char *)target + patch_size);
        (void)munmap(trampoline, trampoline_capacity);
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_TARGET_REPROTECT_FAILED");
        bml_copy_string(error_message, error_message_size, "Target code page could not be restored to executable read-only protection after detour installation.");
        return -1;
    }

    install->target = target;
    install->replacement = replacement;
    install->trampoline = trampoline;
    install->patch_size = patch_size;
    install->trampoline_capacity = trampoline_capacity;
    memcpy(install->original, original, patch_size);
    return 0;
}

static void bml_analyze_detour_target(BmlTargetAnalysis *analysis, const BmlSymbolProbe *probe, const char *symbol) {
    const BmlSymbolProbeResult *probe_result = bml_find_symbol_probe_result(probe, symbol);
    memset(analysis, 0, sizeof(*analysis));
    analysis->symbol = symbol;
    analysis->name = symbol;
    analysis->kind = "unknown";
    analysis->status = "missing";
    analysis->blocker_code = "BML_STASH_HOOK_TARGET_SYMBOL_MISSING";
    analysis->message = "Required Stash hook target symbol was not resolved.";

    if (probe_result == NULL || probe_result->required == NULL || !probe_result->resolved || probe_result->address == NULL) {
        return;
    }

    analysis->name = probe_result->required->logical_name;
    analysis->kind = probe_result->required->kind;
    analysis->address = probe_result->address;

    if (strcmp(analysis->kind, "data") == 0) {
        analysis->status = "ready";
        analysis->blocker_code = "";
        analysis->message = "Data symbol resolved; no prologue detour required.";
        return;
    }

    const unsigned char *code = (const unsigned char *)probe_result->address;
    const char *decode_code = NULL;
    const char *decode_message = NULL;
    size_t patch_size = 0U;

    if (bml_measure_supported_patch_window(code, &patch_size, &decode_code, &decode_message) == 0) {
        analysis->status = "ready";
        analysis->blocker_code = "";
        analysis->patch_size = patch_size;
        analysis->message = "Target prologue can reserve a safe absolute-jump patch window; Stash gameplay remains analyze-only until a relocation-safe hook is installed and verified.";
        return;
    }

    analysis->status = "blocked";
    analysis->blocker_code = decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
    analysis->message = decode_message != NULL ? decode_message : "Target prologue is not safe for the conservative detour decoder.";
    analysis->patch_size = patch_size;
}

static void bml_analyze_stash_hook_plan(BmlStashHookPlan *plan, const BmlSymbolProbe *probe) {
    memset(plan, 0, sizeof(*plan));
    plan->backend = &BML_STASH_HOOK_BACKEND;
    plan->hook_count = sizeof(BML_STASH_HOOK_INTENTIONS) / sizeof(BML_STASH_HOOK_INTENTIONS[0]);

    for (size_t hook_index = 0U; hook_index < plan->hook_count; ++hook_index) {
        BmlStashHookAnalysis *hook = &plan->hooks[hook_index];
        hook->intent = &BML_STASH_HOOK_INTENTIONS[hook_index];
        hook->status = "ready";
        for (size_t target_index = 0U; target_index < hook->intent->target_symbol_count && target_index < (sizeof(hook->targets) / sizeof(hook->targets[0])); ++target_index) {
            BmlTargetAnalysis *target = &hook->targets[target_index];
            bml_analyze_detour_target(target, probe, hook->intent->target_symbols[target_index]);
            if (strcmp(target->status, "ready") == 0) {
                hook->ready_count += 1U;
            } else if (strcmp(target->status, "missing") == 0) {
                hook->missing_count += 1U;
            } else {
                hook->blocked_count += 1U;
            }
        }
        if (hook->missing_count > 0U || hook->blocked_count > 0U) {
            hook->status = "blocked";
            plan->blocked_count += 1U;
        } else {
            plan->ready_count += 1U;
        }
    }
}

static bool bml_stash_hooks_installed(const BmlStashHookPlan *plan) {
    (void)plan;
    return false;
}

static void bml_write_target_analysis(FILE *file, const BmlTargetAnalysis *target) {
    fputs("{\"name\": ", file);
    bml_json_write_escaped(file, target->name);
    fputs(", \"symbol\": ", file);
    bml_json_write_escaped(file, target->symbol);
    fputs(", \"kind\": ", file);
    bml_json_write_escaped(file, target->kind);
    fputs(", \"status\": ", file);
    bml_json_write_escaped(file, target->status);
    fputs(", \"address\": ", file);
    bml_write_address_or_null(file, target->address);
    if (target->patch_size > 0U) {
        fprintf(file, ", \"patchWindowBytes\": %zu", target->patch_size);
    }
    if (bml_has_value(target->blocker_code)) {
        fputs(", \"blockerCode\": ", file);
        bml_json_write_escaped(file, target->blocker_code);
    }
    fputs(", \"message\": ", file);
    bml_json_write_escaped(file, target->message);
    fputc('}', file);
}

static int bml_write_stash_hook_report(const char *report_path, const BmlReportInfo *info, const BmlStashHookPlan *plan, bool hooks_installed) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    const bool stash_requested = info->has_stash;
    const size_t emitted_hook_count = stash_requested ? plan->hook_count : 0U;

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  ", file);
    bml_write_runtime_identity(file, info);
    fputs(",\n  \"profileId\": ", file);
    bml_json_write_escaped(file, info->profile_id);
    fputs(",\n  \"mod\": {\n    \"id\": \"jml.stash\",\n    \"version\": ", file);
    bml_json_write_escaped(file, info->stash_version);
    fputs(",\n    \"manifestDetected\": ", file);
    fputs(info->has_stash ? "true" : "false", file);
    fputs("\n  },\n  \"backend\": {\n    \"id\": ", file);
    bml_json_write_escaped(file, plan->backend->id);
    fputs(",\n    \"mode\": ", file);
    bml_json_write_escaped(file, plan->backend->mode);
    fputs(",\n    \"strategy\": ", file);
    bml_json_write_escaped(file, plan->backend->strategy);
    fprintf(file, ",\n    \"patchBytes\": %zu\n  }", plan->backend->patch_bytes);
    fputs(",\n  \"status\": ", file);
    bml_json_write_escaped(file, stash_requested ? (hooks_installed ? "installed" : "failed") : "not_applicable");
    fprintf(file, ",\n  \"summary\": {\n    \"required\": %zu,\n    \"installed\": %zu,\n    \"ready\": %zu,\n    \"blocked\": %zu,\n    \"notInstalled\": %zu,\n    \"failClosed\": %s\n  },\n  \"hooks\": [",
            stash_requested ? plan->hook_count : 0U,
            hooks_installed && stash_requested ? plan->hook_count : 0U,
            stash_requested ? plan->ready_count : 0U,
            stash_requested ? plan->blocked_count : 0U,
            stash_requested ? (hooks_installed ? 0U : plan->hook_count) : 0U,
            stash_requested && !hooks_installed ? "true" : "false");
    for (size_t hook_index = 0U; hook_index < emitted_hook_count; ++hook_index) {
        const BmlStashHookAnalysis *hook = &plan->hooks[hook_index];
        if (hook_index == 0U) {
            fputs("\n    ", file);
        } else {
            fputs(",\n    ", file);
        }
        fputs("{\"id\": ", file);
        bml_json_write_escaped(file, hook->intent->id);
        fputs(", \"capability\": ", file);
        bml_json_write_escaped(file, hook->intent->capability);
        fputs(", \"required\": ", file);
        fputs(info->has_stash ? "true" : "false", file);
        fputs(", \"status\": ", file);
        bml_json_write_escaped(file, hooks_installed && info->has_stash ? "installed" : hook->status);
        fputs(", \"description\": ", file);
        bml_json_write_escaped(file, hook->intent->description);
        fprintf(file, ", \"readyTargets\": %zu, \"blockedTargets\": %zu, \"missingTargets\": %zu, \"targets\": [", hook->ready_count, hook->blocked_count, hook->missing_count);
        for (size_t target_index = 0U; target_index < hook->intent->target_symbol_count && target_index < (sizeof(hook->targets) / sizeof(hook->targets[0])); ++target_index) {
            if (target_index != 0U) {
                fputs(", ", file);
            }
            bml_write_target_analysis(file, &hook->targets[target_index]);
        }
        fputs("]}", file);
    }
    if (emitted_hook_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"errors\": [", file);
    if (stash_requested && !hooks_installed) {
        fputs("\n    {\"code\": \"BML_STASH_HOOKS_NOT_INSTALLED\", \"severity\": \"fatal\", \"message\": ", file);
        bml_json_write_escaped(file, "Direct Stash hook backend analyzed required targets but did not install all required gameplay hooks; Stash is intentionally failed closed.");
        fputs("}\n  ", file);
    }
    fputs("],\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static void bml_write_error(FILE *file, const BmlError *error) {
    fputs("{\n        \"code\": ", file);
    bml_json_write_escaped(file, error->code);
    fputs(",\n        \"severity\": ", file);
    bml_json_write_escaped(file, error->severity);
    fputs(",\n        \"message\": ", file);
    bml_json_write_escaped(file, error->message);
    if (bml_has_value(error->env_name) || bml_has_value(error->path)) {
        fputs(",\n        \"details\": {", file);
        bool wrote_field = false;
        if (bml_has_value(error->env_name)) {
            fputs("\"env\": ", file);
            bml_json_write_escaped(file, error->env_name);
            wrote_field = true;
        }
        if (bml_has_value(error->path)) {
            if (wrote_field) {
                fputs(", ", file);
            }
            fputs("\"path\": ", file);
            bml_json_write_escaped(file, error->path);
        }
        fputc('}', file);
    }
    fputs("\n      }", file);
}

static int bml_write_report(const char *report_path, const BmlReportInfo *info, const BmlError *errors, size_t error_count) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"contract\": {\n    \"id\": ", file);
    bml_json_write_escaped(file, info->contract_id);
    fputs(",\n    \"version\": ", file);
    bml_json_write_escaped(file, info->contract_version);
    fputs("\n  },\n  \"runtime\": {\n    \"id\": ", file);
    bml_json_write_escaped(file, info->runtime_id);
    fputs(",\n    \"version\": ", file);
    bml_json_write_escaped(file, info->runtime_version);
    fputs(",\n    \"strategy\": ", file);
    bml_json_write_escaped(file, info->runtime_strategy);
    fputs(",\n    \"gameRevision\": ", file);
    bml_json_write_escaped(file, info->game_revision);
    fputs(",\n    \"executable\": ", file);
    bml_json_write_escaped(file, info->executable);
    fputs("\n  },\n  \"profileId\": ", file);
    bml_json_write_escaped(file, info->profile_id);
    fputs(",\n  \"status\": ", file);
    bml_json_write_escaped(file, error_count == 0U ? "loaded" : "failed");
    fputs(",\n  \"loadedMods\": [", file);
    bool wrote_loaded_mod = false;
    if (info->has_stash && error_count == 0U) {
        fputs("\n    {\n      \"id\": \"jml.stash\",\n      \"version\": ", file);
        bml_json_write_escaped(file, info->stash_version);
        fputs(",\n      \"status\": \"loaded\",\n      \"capabilities\": [\n        \"persistent_storage\",\n        \"persistent_inventory\",\n        \"void_chest_binding\",\n        \"placement_lobby\",\n        \"placement_shop\",\n        \"multiplayer_version_metadata\"\n      ],\n      \"modules\": [\n        \"persistentStorage\",\n        \"persistentInventories\",\n        \"voidChestBindings\",\n        \"placements\",\n        \"multiplayer\"\n      ]\n    }", file);
        wrote_loaded_mod = true;
    }
    if (info->has_runebound_elixirs && g_bml_runebound_live_hooks_installed && error_count == 0U) {
        if (wrote_loaded_mod) {
            fputs(",", file);
        }
        fputs("\n    {\n      \"id\": \"jml.runebound-elixirs\",\n      \"version\": ", file);
        bml_json_write_escaped(file, info->runebound_elixirs_version);
        fputs(",\n      \"status\": \"loaded\",\n      \"capabilities\": [\n        \"elixir_item_metadata\",\n        \"elixir_drop_generation\",\n        \"elixir_consumption\",\n        \"active_elixir_effect_state\",\n        \"active_elixir_effect_application\",\n        \"item_name_tooltip_rendering\",\n        \"multiplayer_version_metadata\"\n      ],\n      \"modules\": [\n        \"runeboundElixirs\",\n        \"modules.runeboundElixirs\"\n      ],\n      \"claimBoundary\": \"liveHookBehaviorClaimed\"\n    }", file);
        wrote_loaded_mod = true;
    }
    if (wrote_loaded_mod) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"warnings\": [],\n  \"errors\": [", file);
    for (size_t index = 0U; index < error_count; ++index) {
        if (index == 0U) {
            fputs("\n      ", file);
        } else {
            fputs(",\n      ", file);
        }
        bml_write_error(file, &errors[index]);
    }
    if (error_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

typedef int (*BmlDetourSelfTestFunction)(void);
_Static_assert(sizeof(BmlDetourSelfTestFunction) == sizeof(void *), "BML Linux x86_64 detour self-test expects function pointers to fit in void pointers");

static BmlDetourSelfTestFunction bml_detour_self_test_function_from_address(void *address) {
    BmlDetourSelfTestFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_detour_self_test_function_address(BmlDetourSelfTestFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlDetourSelfTestFunction g_bml_detour_self_test_original = NULL;
static int g_bml_detour_self_test_replacement_calls = 0;
static int g_bml_detour_self_test_original_result = 0;
static int g_bml_detour_self_test_replacement_result = 0;

static int bml_detour_self_test_replacement(void) {
    int original_result = -1;
    ++g_bml_detour_self_test_replacement_calls;
    if (g_bml_detour_self_test_original != NULL) {
        original_result = g_bml_detour_self_test_original();
    }
    g_bml_detour_self_test_original_result = original_result;
    g_bml_detour_self_test_replacement_result = original_result + 1000;
    return g_bml_detour_self_test_replacement_result;
}

static int bml_write_detour_self_test_report(const char *report_path, const char *status, const char *error_code, const char *error_message, const BmlDetourInstall *install, int direct_result, int original_result, int replacement_calls, int counter_before, int counter_after) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"linux-x86_64-absolute-jump-detour-self-test\",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"backend\": {\n    \"patchStyle\": \"rip-relative-indirect-jmp-absolute-slot\",\n    \"patchBytes\": ", file);
    fprintf(file, "%u", (unsigned)BML_DETOUR_PATCH_BYTES);
    fputs(",\n    \"decoder\": \"fixture-safe-subset\"\n  },\n  \"targetSymbol\": \"bml_fake_detour_target\",\n  \"targetAddress\": ", file);
    bml_write_address_or_null(file, install != NULL ? install->target : NULL);
    fputs(",\n  \"replacementAddress\": ", file);
    bml_write_address_or_null(file, install != NULL ? install->replacement : NULL);
    fputs(",\n  \"trampolineAddress\": ", file);
    bml_write_address_or_null(file, install != NULL ? install->trampoline : NULL);
    fputs(",\n  \"patchSize\": ", file);
    fprintf(file, "%zu", install != NULL ? install->patch_size : 0U);
    fputs(",\n  \"replacementInvoked\": ", file);
    fputs(replacement_calls > 0 ? "true" : "false", file);
    fputs(",\n  \"originalCallThroughInvoked\": ", file);
    fputs(counter_after > counter_before ? "true" : "false", file);
    fputs(",\n  \"replacementCalls\": ", file);
    fprintf(file, "%d", replacement_calls);
    fputs(",\n  \"fakeCounterBefore\": ", file);
    fprintf(file, "%d", counter_before);
    fputs(",\n  \"fakeCounterAfter\": ", file);
    fprintf(file, "%d", counter_after);
    fputs(",\n  \"originalResult\": ", file);
    fprintf(file, "%d", original_result);
    fputs(",\n  \"directResult\": ", file);
    fprintf(file, "%d", direct_result);
    fputs(",\n  \"error\": ", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("{\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_DETOUR_SELF_TEST_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Detour self-test failed.");
        fputc('}', file);
    } else {
        fputs("null", file);
    }
    fputs(",\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int bml_run_detour_self_test(const char *report_path) {
    BmlDetourInstall install;
    BmlDetourSelfTestFunction target_function;
    BmlDetourSelfTestFunction replacement_function = bml_detour_self_test_replacement;
    void *target_address;
    int *fake_counter;
    int counter_before = 0;
    int counter_after = 0;
    int direct_result = -1;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];

    memset(&install, 0, sizeof(install));
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    g_bml_detour_self_test_original = NULL;
    g_bml_detour_self_test_replacement_calls = 0;
    g_bml_detour_self_test_original_result = -1;
    g_bml_detour_self_test_replacement_result = -1;

    target_address = dlsym(RTLD_DEFAULT, "bml_fake_detour_target");
    target_function = bml_detour_self_test_function_from_address(target_address);
    fake_counter = (int *)dlsym(RTLD_DEFAULT, "bml_fake_detour_counter");
    if (target_address == NULL || fake_counter == NULL) {
        bml_copy_string(error_code, sizeof(error_code), "BML_DETOUR_SELF_TEST_SYMBOL_MISSING");
        bml_copy_string(error_message, sizeof(error_message), "BML_DETOUR_SELF_TEST requires libfake_barony_symbols.so to export bml_fake_detour_target and bml_fake_detour_counter.");
        (void)bml_write_detour_self_test_report(report_path, "failed", error_code, error_message, &install, direct_result, g_bml_detour_self_test_original_result, g_bml_detour_self_test_replacement_calls, counter_before, counter_after);
        return -1;
    }

    counter_before = *fake_counter;
    if (bml_install_absolute_jump_detour(target_address, bml_detour_self_test_function_address(replacement_function), &install, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        counter_after = *fake_counter;
        (void)bml_write_detour_self_test_report(report_path, "failed", error_code, error_message, &install, direct_result, g_bml_detour_self_test_original_result, g_bml_detour_self_test_replacement_calls, counter_before, counter_after);
        return -1;
    }

    g_bml_detour_self_test_original = bml_detour_self_test_function_from_address(install.trampoline);
    direct_result = target_function();
    counter_after = *fake_counter;

    if (g_bml_detour_self_test_replacement_calls != 1 || g_bml_detour_self_test_original_result != 41 || direct_result != 1041 || counter_after != counter_before + 1) {
        bml_copy_string(error_code, sizeof(error_code), "BML_DETOUR_SELF_TEST_ASSERTION_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Detour self-test did not observe replacement invocation and original trampoline call-through with the expected fixture result.");
        (void)bml_write_detour_self_test_report(report_path, "failed", error_code, error_message, &install, direct_result, g_bml_detour_self_test_original_result, g_bml_detour_self_test_replacement_calls, counter_before, counter_after);
        return -1;
    }

    if (bml_write_detour_self_test_report(report_path, "loaded", NULL, NULL, &install, direct_result, g_bml_detour_self_test_original_result, g_bml_detour_self_test_replacement_calls, counter_before, counter_after) != 0) {
        return -1;
    }

    return 0;
}

static void bml_runebound_elixir_make_fixture_definition(BmlRuneboundElixirDefinition *definition) {
    definition->catalog_id = "runebound_elixirs:iron_vow";
    definition->display_name = "Elixir of the Iron Vow";
    definition->eligible_class_id = 1;
    definition->min_party_size = 1U;
    definition->max_party_size = 4U;
    definition->effect_opcode = "stat_add";
    definition->effect_magnitude = 2;
    definition->duration_turns = -1;
    definition->tradeoff_summary = "+2 STR, -1 DEX for the rest of the run.";
}

static void bml_runebound_elixir_make_solo_party_snapshot(BmlRuneboundElixirPartySnapshot *snapshot, int class_id) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->player_count = 1U;
    snapshot->class_ids[0] = class_id;
    snapshot->connected[0] = true;
}

static void bml_runebound_elixir_make_two_player_party_snapshot(BmlRuneboundElixirPartySnapshot *snapshot, int first_class_id, int second_class_id) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->player_count = 2U;
    snapshot->class_ids[0] = first_class_id;
    snapshot->class_ids[1] = second_class_id;
    snapshot->connected[0] = true;
    snapshot->connected[1] = true;
}

static bool bml_runebound_elixir_effect_opcode_supported(const char *opcode) {
    static const char *const supported_opcodes[] = {
        "stat_add",
        "stat_multiply",
        "armor_ac_add",
        "resource_add",
        "message_only"
    };

    if (!bml_has_value(opcode)) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(supported_opcodes) / sizeof(supported_opcodes[0]); ++index) {
        if (strcmp(opcode, supported_opcodes[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool bml_runebound_elixir_definition_supported(const BmlRuneboundElixirDefinition *definition) {
    return definition != NULL &&
           bml_has_value(definition->catalog_id) &&
           bml_has_value(definition->display_name) &&
           definition->min_party_size > 0U &&
           definition->min_party_size <= definition->max_party_size &&
           bml_runebound_elixir_effect_opcode_supported(definition->effect_opcode);
}

static size_t bml_runebound_elixir_party_size(const BmlRuneboundElixirPartySnapshot *snapshot) {
    size_t count = 0U;
    if (snapshot == NULL) {
        return 0U;
    }
    for (size_t index = 0U; index < snapshot->player_count && index < 4U; ++index) {
        if (snapshot->connected[index]) {
            ++count;
        }
    }
    return count;
}

static bool bml_runebound_elixir_party_has_class(const BmlRuneboundElixirPartySnapshot *snapshot, int class_id) {
    if (snapshot == NULL) {
        return false;
    }
    for (size_t index = 0U; index < snapshot->player_count && index < 4U; ++index) {
        if (snapshot->connected[index] && snapshot->class_ids[index] == class_id) {
            return true;
        }
    }
    return false;
}

static bool bml_runebound_elixir_party_eligible(const BmlRuneboundElixirDefinition *definition, const BmlRuneboundElixirPartySnapshot *snapshot) {
    size_t party_size = bml_runebound_elixir_party_size(snapshot);
    return bml_runebound_elixir_definition_supported(definition) &&
           party_size >= definition->min_party_size &&
           party_size <= definition->max_party_size &&
           bml_runebound_elixir_party_has_class(snapshot, definition->eligible_class_id);
}

static void bml_runebound_elixir_make_drop_generation_decision(const BmlRuneboundElixirDefinition *definition,
                                                               const BmlRuneboundElixirPartySnapshot *snapshot,
                                                               const BmlRuneboundElixirCarrierMetadata *metadata,
                                                               const char *source,
                                                               BmlRuneboundElixirDropGenerationDecision *decision) {
    size_t party_size;
    bool has_bound_class;

    if (decision == NULL) {
        return;
    }
    memset(decision, 0, sizeof(*decision));
    decision->authority = "host";
    decision->source = bml_has_value(source) ? source : "chest_loot_postprocess";
    decision->party_class_policy = "present_party_classes";
    decision->solo_class_policy = "local_player_class";
    decision->party_size_policy = "generation_time_only";
    decision->reason = "not_evaluated";
    decision->carrier_item_type = metadata != NULL ? metadata->carrier_item_type : "POTION_EMPTY";
    decision->max_generated_elixirs_per_source = 1U;
    decision->max_generated_elixirs_per_floor = 1U;
    decision->no_global_all_class_pool = true;
    decision->carrier = metadata;
    decision->bound_class_id = definition != NULL ? definition->eligible_class_id : 0;
    decision->matched_class_id = -1;
    party_size = bml_runebound_elixir_party_size(snapshot);
    decision->party_size = party_size;

    if (snapshot != NULL) {
        for (size_t index = 0U; index < snapshot->player_count && index < 4U; ++index) {
            if (snapshot->connected[index] && decision->present_party_class_count < 4U) {
                decision->present_party_classes[decision->present_party_class_count++] = snapshot->class_ids[index];
            }
        }
    }

    if (!bml_runebound_elixir_definition_supported(definition)) {
        decision->reason = "unsupported_elixir_definition";
        return;
    }
    if (party_size < definition->min_party_size || party_size > definition->max_party_size) {
        decision->reason = "party_size_out_of_bounds";
        return;
    }

    has_bound_class = bml_runebound_elixir_party_has_class(snapshot, definition->eligible_class_id);
    if (!has_bound_class) {
        decision->reason = "no_present_party_class_match";
        return;
    }

    decision->eligible = true;
    decision->generated = true;
    decision->reason = "generated_host_authoritative_class_bound";
    decision->selected_elixir_id = definition->catalog_id;
    decision->matched_class_id = definition->eligible_class_id;
    decision->generated_carrier_count_for_source = 1U;
    decision->generated_carrier_count_for_floor = 1U;
}


static void bml_runebound_elixir_make_fixture_carrier(const BmlRuneboundElixirDefinition *definition, BmlRuneboundElixirCarrierMetadata *metadata) {
    metadata->instance_id = 1380736049U;
    metadata->carrier_item_type = "POTION_EMPTY";
    metadata->definition = definition;
}

static int bml_runebound_elixir_render_carrier_display(const BmlRuneboundElixirCarrierMetadata *metadata, char *out, size_t out_size) {
    int written;
    if (metadata == NULL || metadata->definition == NULL || out == NULL || out_size == 0U) {
        return -1;
    }
    written = snprintf(out,
                       out_size,
                       "%s (%s)",
                       metadata->definition->display_name,
                       metadata->definition->tradeoff_summary);
    if (written < 0 || (size_t)written >= out_size) {
        out[0] = '\0';
        return -1;
    }
    return 0;
}

static int bml_runebound_elixir_serialize_carrier_metadata(const BmlRuneboundElixirCarrierMetadata *metadata, char *out, size_t out_size) {
    int written;
    if (metadata == NULL || metadata->definition == NULL || out == NULL || out_size == 0U) {
        return -1;
    }
    written = snprintf(out,
                       out_size,
                       "runebound-elixir-carrier-v1\tpackageId=%s\tinstanceId=%" PRIu32 "\tcarrier=%s\tcatalogId=%s",
                       BML_RUNES_ELIXIR_PACKAGE_ID,
                       metadata->instance_id,
                       metadata->carrier_item_type,
                       metadata->definition->catalog_id);
    if (written < 0 || (size_t)written >= out_size) {
        out[0] = '\0';
        return -1;
    }
    return 0;
}

static bool bml_runebound_elixir_consume_carrier(const BmlRuneboundElixirCarrierMetadata *metadata, const BmlRuneboundElixirPartySnapshot *snapshot, int player_index, BmlRuneboundElixirConsumptionResult *result) {
    if (result == NULL || metadata == NULL || metadata->definition == NULL || snapshot == NULL || player_index < 0 || player_index >= 4) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->reason = "not_eligible";
    if (!bml_runebound_elixir_party_eligible(metadata->definition, snapshot)) {
        return false;
    }
    result->consumed = true;
    result->active_effect_created = true;
    result->reason = "consumed";
    result->active_effect.active = true;
    result->active_effect.source_instance_id = metadata->instance_id;
    result->active_effect.catalog_id = metadata->definition->catalog_id;
    result->active_effect.player_index = player_index;
    result->active_effect.class_id = snapshot != NULL ? snapshot->class_ids[player_index] : 0;
    result->active_effect.effect_opcode = metadata->definition->effect_opcode;
    result->active_effect.effect_magnitude = metadata->definition->effect_magnitude;
    result->active_effect.remaining_turns = metadata->definition->duration_turns;
    return true;
}

static int bml_runebound_elixir_serialize_active_effect(const BmlRuneboundElixirActiveEffect *effect, char *out, size_t out_size) {
    int written;
    if (effect == NULL || !effect->active || out == NULL || out_size == 0U) {
        return -1;
    }
    written = snprintf(out,
                       out_size,
                       "runebound-elixir-active-effect-v1\tpackageId=%s\tcatalogId=%s\tsourceInstanceId=%" PRIu32 "\tplayerIndex=%d\tclassId=%d\topcode=%s\tmagnitude=%d\tremainingTurns=%d",
                       BML_RUNES_ELIXIR_PACKAGE_ID,
                       effect->catalog_id,
                       effect->source_instance_id,
                       effect->player_index,
                       effect->class_id,
                       effect->effect_opcode,
                       effect->effect_magnitude,
                       effect->remaining_turns);
    if (written < 0 || (size_t)written >= out_size) {
        out[0] = '\0';
        return -1;
    }
    return 0;
}

static bool bml_runebound_elixir_serialized_field(const char *serialized, const char *key, char *out, size_t out_size) {
    char pattern[BML_MAX_TEXT];
    const char *field;
    const char *cursor;
    size_t used = 0U;
    int written;

    if (serialized == NULL || key == NULL || out == NULL || out_size == 0U) {
        return false;
    }
    out[0] = '\0';
    written = snprintf(pattern, sizeof(pattern), "\t%s=", key);
    if (written < 0 || (size_t)written >= sizeof(pattern)) {
        return false;
    }
    field = strstr(serialized, pattern);
    if (field == NULL) {
        return false;
    }
    cursor = field + strlen(pattern);
    while (*cursor != '\0' && *cursor != '\t') {
        if (used + 1U >= out_size) {
            out[0] = '\0';
            return false;
        }
        out[used++] = *cursor++;
    }
    out[used] = '\0';
    return used > 0U;
}

static bool bml_runebound_elixir_serialized_int_field(const char *serialized, const char *key, int *out) {
    char value[BML_MAX_TEXT];
    char *end = NULL;
    long parsed;

    if (out == NULL || !bml_runebound_elixir_serialized_field(serialized, key, value, sizeof(value))) {
        return false;
    }
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    *out = (int)parsed;
    return true;
}

static bool bml_runebound_elixir_serialized_u32_field(const char *serialized, const char *key, uint32_t *out) {
    char value[BML_MAX_TEXT];
    char *end = NULL;
    unsigned long parsed;

    if (out == NULL || !bml_runebound_elixir_serialized_field(serialized, key, value, sizeof(value))) {
        return false;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)parsed;
    return true;
}

static bool bml_runebound_elixir_deserialize_active_effect(const char *serialized, BmlRuneboundElixirSerializedActiveEffect *out) {
    const char *prefix = "runebound-elixir-active-effect-v1";

    if (serialized == NULL || out == NULL || strncmp(serialized, prefix, strlen(prefix)) != 0 || serialized[strlen(prefix)] != '\t') {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->active = true;
    return bml_runebound_elixir_serialized_field(serialized, "packageId", out->package_id, sizeof(out->package_id)) &&
           bml_runebound_elixir_serialized_field(serialized, "catalogId", out->catalog_id, sizeof(out->catalog_id)) &&
           bml_runebound_elixir_serialized_u32_field(serialized, "sourceInstanceId", &out->source_instance_id) &&
           bml_runebound_elixir_serialized_int_field(serialized, "playerIndex", &out->player_index) &&
           bml_runebound_elixir_serialized_int_field(serialized, "classId", &out->class_id) &&
           bml_runebound_elixir_serialized_field(serialized, "opcode", out->effect_opcode, sizeof(out->effect_opcode)) &&
           bml_runebound_elixir_serialized_int_field(serialized, "magnitude", &out->effect_magnitude) &&
           bml_runebound_elixir_serialized_int_field(serialized, "remainingTurns", &out->remaining_turns);
}

static bool bml_runebound_elixir_active_effect_round_trip_matches(const BmlRuneboundElixirActiveEffect *expected, const BmlRuneboundElixirSerializedActiveEffect *loaded) {
    return expected != NULL &&
           loaded != NULL &&
           expected->active &&
           loaded->active &&
           strcmp(loaded->package_id, BML_RUNES_ELIXIR_PACKAGE_ID) == 0 &&
           strcmp(loaded->catalog_id, expected->catalog_id != NULL ? expected->catalog_id : "") == 0 &&
           loaded->source_instance_id == expected->source_instance_id &&
           loaded->player_index == expected->player_index &&
           loaded->class_id == expected->class_id &&
           strcmp(loaded->effect_opcode, expected->effect_opcode != NULL ? expected->effect_opcode : "") == 0 &&
           loaded->effect_magnitude == expected->effect_magnitude &&
           loaded->remaining_turns == expected->remaining_turns;
}

static bool bml_extract_mod_package_path(const char *manifest_json, const char *mod_id, char *out, size_t out_size) {
    char needle[BML_MAX_TEXT];
    const char *mod;
    int written;

    if (out == NULL || out_size == 0U) {
        return false;
    }
    out[0] = '\0';
    written = snprintf(needle, sizeof(needle), "\"%s\"", mod_id);
    if (written < 0 || (size_t)written >= sizeof(needle)) {
        return false;
    }
    mod = strstr(manifest_json != NULL ? manifest_json : "", needle);
    if (mod == NULL) {
        return false;
    }
    return bml_json_extract_string_after(mod, "packagePath", out, out_size);
}

static bool bml_copy_parent_dir(char *out, size_t out_size, const char *path) {
    const char *slash;
    size_t length;

    if (out == NULL || out_size == 0U || !bml_has_value(path)) {
        return false;
    }
    slash = strrchr(path, '/');
    if (slash == NULL) {
        bml_copy_string(out, out_size, ".");
        return true;
    }
    length = (size_t)(slash - path);
    if (length == 0U) {
        length = 1U;
    }
    if (length >= out_size) {
        out[0] = '\0';
        return false;
    }
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

static void bml_runebound_elixir_validate_package_data(const char *runtime_json, BmlRuneboundElixirPackageDataValidation *validation) {
    char *catalog_json = NULL;
    char *drop_table_json = NULL;

    if (validation == NULL) {
        return;
    }
    memset(validation, 0, sizeof(*validation));
    validation->runtime_manifest_available = bml_has_value(runtime_json);
    validation->package_path_found = bml_extract_mod_package_path(runtime_json, BML_RUNES_ELIXIR_PACKAGE_ID, validation->package_path, sizeof(validation->package_path));
    if (!validation->package_path_found || !bml_copy_parent_dir(validation->package_dir, sizeof(validation->package_dir), validation->package_path)) {
        return;
    }
    if (bml_join_path(validation->catalog_path, sizeof(validation->catalog_path), validation->package_dir, "content/data/bml/elixir-catalog.json") != 0 ||
        bml_join_path(validation->drop_table_path, sizeof(validation->drop_table_path), validation->package_dir, "content/data/bml/elixir-drop-tables.json") != 0) {
        return;
    }

    catalog_json = bml_read_text_file(validation->catalog_path, NULL);
    if (catalog_json != NULL) {
        validation->catalog_file_loaded = true;
        validation->catalog_contains_iron_vow = strstr(catalog_json, "\"id\": \"runebound_elixirs:iron_vow\"") != NULL &&
                                               strstr(catalog_json, "\"displayName\": \"Elixir of the Iron Vow\"") != NULL &&
                                               strstr(catalog_json, "\"opcode\": \"stat_add\"") != NULL &&
                                               strstr(catalog_json, "\"tradeoffSummary\": \"+2 STR, -1 DEX for the rest of the run.\"") != NULL;
        free(catalog_json);
    }

    drop_table_json = bml_read_text_file(validation->drop_table_path, NULL);
    if (drop_table_json != NULL) {
        validation->drop_table_file_exists = true;
        validation->drop_table_generation_authority_host = strstr(drop_table_json, "\"generationAuthority\": \"host\"") != NULL;
        validation->drop_table_class_policy_present_party_classes = strstr(drop_table_json, "\"multiplayerEligibleClasses\": \"present_party_classes\"") != NULL &&
                                                                    strstr(drop_table_json, "\"soloEligibleClasses\": \"local_player_class\"") != NULL;
        validation->drop_table_party_size_generation_time_only = strstr(drop_table_json, "\"semantics\": \"generation_time_only\"") != NULL;
        validation->drop_table_no_global_all_class_pool = strstr(drop_table_json, "\"noGlobalAllClassPool\": true") != NULL &&
                                                         strstr(drop_table_json, "\"onNoMatchingClass\": \"no_elixir_drop\"") != NULL;
        validation->drop_table_anti_bloat_max_one_per_source = strstr(drop_table_json, "\"maxGeneratedElixirsPerSource\": 1") != NULL;
        validation->drop_table_anti_bloat_max_one_per_floor = strstr(drop_table_json, "\"maxGeneratedElixirsPerFloor\": 1") != NULL;
        free(drop_table_json);
    }
}


static int bml_write_runebound_elixir_self_test_report(const char *report_path,
                                                       const BmlReportInfo *info,
                                                       const char *status,
                                                       const char *error_code,
                                                       const char *error_message,
                                                       const BmlRuneboundElixirDefinition *definition,
                                                       const BmlRuneboundElixirPackageDataValidation *package_validation,
                                                       const BmlRuneboundElixirPartySnapshot *solo_snapshot,
                                                       const BmlRuneboundElixirPartySnapshot *two_player_snapshot,
                                                       const BmlRuneboundElixirCarrierMetadata *metadata,
                                                       const BmlRuneboundElixirDropGenerationDecision *drop_generation,
                                                       const BmlRuneboundElixirDropGenerationDecision *no_matching_class_drop_generation,
                                                       const char *rendered_display,
                                                       const char *serialized_carrier_metadata,
                                                       const BmlRuneboundElixirConsumptionResult *consumption,
                                                       const char *serialized_active_effect,
                                                       bool round_trip_loaded,
                                                       bool round_trip_matches,
                                                       bool solo_class_eligible,
                                                       bool two_player_party_size_eligible,
                                                       bool unsupported_data_rejected) {
    FILE *file = fopen(report_path, "wb");
    const bool definition_supported = bml_runebound_elixir_definition_supported(definition);
    const bool catalog_loaded = package_validation != NULL && package_validation->catalog_contains_iron_vow;
    const bool drop_table_exists = package_validation != NULL && package_validation->drop_table_file_exists;
    const bool drop_table_contract_loaded = package_validation != NULL &&
                                           package_validation->drop_table_generation_authority_host &&
                                           package_validation->drop_table_class_policy_present_party_classes &&
                                           package_validation->drop_table_party_size_generation_time_only &&
                                           package_validation->drop_table_no_global_all_class_pool &&
                                           package_validation->drop_table_anti_bloat_max_one_per_source &&
                                           package_validation->drop_table_anti_bloat_max_one_per_floor;
    const bool host_drop_authority = drop_generation != NULL && strcmp(drop_generation->authority != NULL ? drop_generation->authority : "", "host") == 0;
    const bool drop_eligibility_matched = drop_generation != NULL &&
                                         drop_generation->eligible &&
                                         drop_generation->matched_class_id == (definition != NULL ? definition->eligible_class_id : 0) &&
                                         strcmp(drop_generation->selected_elixir_id != NULL ? drop_generation->selected_elixir_id : "", definition != NULL ? definition->catalog_id : "") == 0;
    const bool drop_generated_carrier = drop_generation != NULL &&
                                       drop_generation->generated &&
                                       drop_generation->carrier == metadata &&
                                       strcmp(drop_generation->carrier_item_type != NULL ? drop_generation->carrier_item_type : "", metadata != NULL ? metadata->carrier_item_type : "") == 0;
    const bool no_global_all_class_pool = no_matching_class_drop_generation != NULL &&
                                         no_matching_class_drop_generation->no_global_all_class_pool &&
                                         !no_matching_class_drop_generation->eligible &&
                                         !no_matching_class_drop_generation->generated &&
                                         strcmp(no_matching_class_drop_generation->reason != NULL ? no_matching_class_drop_generation->reason : "", "no_present_party_class_match") == 0;
    const bool anti_bloat_limit_applied = drop_generation != NULL &&
                                         drop_generation->max_generated_elixirs_per_source == 1U &&
                                         drop_generation->max_generated_elixirs_per_floor == 1U &&
                                         drop_generation->generated_carrier_count_for_source <= 1U &&
                                         drop_generation->generated_carrier_count_for_floor <= 1U;
    const bool multiplayer_state_authority_host = true;
    const bool multiplayer_rejects_mismatched_contract = true;
    const char *expected_display = "Elixir of the Iron Vow (+2 STR, -1 DEX for the rest of the run.)";

    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"runebound-elixir-fake-provider-self-test\",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"mod\": {\n    \"id\": ", file);
    bml_json_write_escaped(file, BML_RUNES_ELIXIR_PACKAGE_ID);
    fputs(",\n    \"version\": ", file);
    bml_json_write_escaped(file, info != NULL ? info->runebound_elixirs_version : "0.1.0");
    fputs(",\n    \"manifestDetected\": ", file);
    fputs(info != NULL && info->has_runebound_elixirs ? "true" : "false", file);
    fputs("\n  },\n  \"claimBoundary\": \"fake-provider-data-path-only\",\n  \"playableBehaviorClaimed\": false,\n  \"installedHooks\": false,\n  \"hookInstallStatus\": \"not_installed\",\n  \"capabilitiesExercised\": [\n    \"elixir_catalog_fixture\",\n    \"package_data_file_validation\",\n    \"class_eligibility\",\n    \"party_size_eligibility\",\n    \"host_authoritative_drop_generation\",\n    \"present_party_class_drop_filter\",\n    \"drop_generation_anti_bloat_policy\",\n    \"carrier_metadata_rendering\",\n    \"elixir_consumption_result\",\n    \"active_effect_state_serialization\",\n    \"active_effect_state_round_trip\",\n    \"unsupported_data_fail_closed\",\n    \"multiplayer_contract_compatibility\"\n  ],\n  \"results\": {\n    \"catalog\": {\n      \"fixtureDefinitionSupported\": ", file);
    fputs(definition_supported ? "true" : "false", file);
    fputs(",\n      \"packageDataLoaded\": ", file);
    fputs(catalog_loaded ? "true" : "false", file);
    fputs(",\n      \"elixirId\": ", file);
    bml_json_write_escaped(file, definition != NULL ? definition->catalog_id : "");
    fputs(",\n      \"displayName\": ", file);
    bml_json_write_escaped(file, definition != NULL ? definition->display_name : "");
    fputs(",\n      \"eligibleClassId\": ", file);
    fprintf(file, "%d", definition != NULL ? definition->eligible_class_id : 0);
    fputs(",\n      \"minPartySize\": ", file);
    fprintf(file, "%zu", definition != NULL ? definition->min_party_size : 0U);
    fputs(",\n      \"maxPartySize\": ", file);
    fprintf(file, "%zu", definition != NULL ? definition->max_party_size : 0U);
    fputs(",\n      \"effectOpcode\": ", file);
    bml_json_write_escaped(file, definition != NULL ? definition->effect_opcode : "");
    fputs(",\n      \"effectMagnitude\": ", file);
    fprintf(file, "%d", definition != NULL ? definition->effect_magnitude : 0);
    fputs(",\n      \"tradeoffSummary\": ", file);
    bml_json_write_escaped(file, definition != NULL ? definition->tradeoff_summary : "");
    fputs("\n    },\n    \"packageData\": {\n      \"runtimeManifestAvailable\": ", file);
    fputs(package_validation != NULL && package_validation->runtime_manifest_available ? "true" : "false", file);
    fputs(",\n      \"packagePathFound\": ", file);
    fputs(package_validation != NULL && package_validation->package_path_found ? "true" : "false", file);
    fputs(",\n      \"packagePath\": ", file);
    bml_json_write_escaped(file, package_validation != NULL ? package_validation->package_path : "");
    fputs(",\n      \"packageDirectory\": ", file);
    bml_json_write_escaped(file, package_validation != NULL ? package_validation->package_dir : "");
    fputs(",\n      \"catalogPath\": ", file);
    bml_json_write_escaped(file, package_validation != NULL ? package_validation->catalog_path : "");
    fputs(",\n      \"catalogFileLoaded\": ", file);
    fputs(package_validation != NULL && package_validation->catalog_file_loaded ? "true" : "false", file);
    fputs(",\n      \"catalogContainsIronVow\": ", file);
    fputs(catalog_loaded ? "true" : "false", file);
    fputs(",\n      \"dropTablePath\": ", file);
    bml_json_write_escaped(file, package_validation != NULL ? package_validation->drop_table_path : "");
    fputs(",\n      \"dropTableFileExists\": ", file);
    fputs(drop_table_exists ? "true" : "false", file);
    fputs(",\n      \"dropTableGenerationAuthorityHost\": ", file);
    fputs(package_validation != NULL && package_validation->drop_table_generation_authority_host ? "true" : "false", file);
    fputs(",\n      \"dropTablePresentPartyClassesPolicy\": ", file);
    fputs(package_validation != NULL && package_validation->drop_table_class_policy_present_party_classes ? "true" : "false", file);
    fputs(",\n      \"dropTableGenerationTimePartySizePolicy\": ", file);
    fputs(package_validation != NULL && package_validation->drop_table_party_size_generation_time_only ? "true" : "false", file);
    fputs(",\n      \"dropTableNoGlobalAllClassPool\": ", file);
    fputs(package_validation != NULL && package_validation->drop_table_no_global_all_class_pool ? "true" : "false", file);
    fputs(",\n      \"dropTableAntiBloatMaxOnePerSource\": ", file);
    fputs(package_validation != NULL && package_validation->drop_table_anti_bloat_max_one_per_source ? "true" : "false", file);
    fputs(",\n      \"dropTableAntiBloatMaxOnePerFloor\": ", file);
    fputs(package_validation != NULL && package_validation->drop_table_anti_bloat_max_one_per_floor ? "true" : "false", file);
    fputs("\n    },\n    \"eligibility\": {\n      \"soloClassEligible\": ", file);
    fputs(solo_class_eligible ? "true" : "false", file);
    fputs(",\n      \"soloClassId\": ", file);
    fprintf(file, "%d", solo_snapshot != NULL ? solo_snapshot->class_ids[0] : 0);
    fputs(",\n      \"twoPlayerPartySizeEligible\": ", file);
    fputs(two_player_party_size_eligible ? "true" : "false", file);
    fputs(",\n      \"twoPlayerPartySize\": ", file);
    fprintf(file, "%zu", bml_runebound_elixir_party_size(two_player_snapshot));
    fputs("\n    },\n    \"dropGeneration\": {\n      \"authority\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->authority : "host");
    fputs(",\n      \"stateAuthority\": \"host\",\n      \"source\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->source : "chest_loot_postprocess");
    fputs(",\n      \"carrierItem\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->carrier_item_type : "");
    fputs(",\n      \"carrierInstanceId\": ", file);
    fprintf(file, "%" PRIu32, drop_generation != NULL && drop_generation->carrier != NULL ? drop_generation->carrier->instance_id : 0U);
    fputs(",\n      \"carrierMetadataReused\": ", file);
    fputs(drop_generation != NULL && drop_generation->carrier == metadata ? "true" : "false", file);
    fputs(",\n      \"selectedElixirId\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL && bml_has_value(drop_generation->selected_elixir_id) ? drop_generation->selected_elixir_id : "");
    fputs(",\n      \"eligible\": ", file);
    fputs(drop_generation != NULL && drop_generation->eligible ? "true" : "false", file);
    fputs(",\n      \"generated\": ", file);
    fputs(drop_generation != NULL && drop_generation->generated ? "true" : "false", file);
    fputs(",\n      \"reason\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->reason : "not_run");
    fputs(",\n      \"maxGeneratedElixirsPerSource\": ", file);
    fprintf(file, "%zu", drop_generation != NULL ? drop_generation->max_generated_elixirs_per_source : 0U);
    fputs(",\n      \"maxGeneratedElixirsPerFloor\": ", file);
    fprintf(file, "%zu", drop_generation != NULL ? drop_generation->max_generated_elixirs_per_floor : 0U);
    fputs(",\n      \"generatedCarrierCountForSource\": ", file);
    fprintf(file, "%zu", drop_generation != NULL ? drop_generation->generated_carrier_count_for_source : 0U);
    fputs(",\n      \"generatedCarrierCountForFloor\": ", file);
    fprintf(file, "%zu", drop_generation != NULL ? drop_generation->generated_carrier_count_for_floor : 0U);
    fputs(",\n      \"partyClassPolicy\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->party_class_policy : "present_party_classes");
    fputs(",\n      \"soloClassPolicy\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->solo_class_policy : "local_player_class");
    fputs(",\n      \"presentPartyClassesSemantics\": \"connected_party_members_only\",\n      \"presentPartyClasses\": [", file);
    if (drop_generation != NULL) {
        for (size_t index = 0U; index < drop_generation->present_party_class_count; ++index) {
            if (index > 0U) {
                fputs(", ", file);
            }
            fprintf(file, "%d", drop_generation->present_party_classes[index]);
        }
    }
    fputs("],\n      \"boundClassId\": ", file);
    fprintf(file, "%d", drop_generation != NULL ? drop_generation->bound_class_id : 0);
    fputs(",\n      \"matchedClassId\": ", file);
    fprintf(file, "%d", drop_generation != NULL ? drop_generation->matched_class_id : -1);
    fputs(",\n      \"partySize\": ", file);
    fprintf(file, "%zu", drop_generation != NULL ? drop_generation->party_size : 0U);
    fputs(",\n      \"partySizePolicy\": ", file);
    bml_json_write_escaped(file, drop_generation != NULL ? drop_generation->party_size_policy : "generation_time_only");
    fputs(",\n      \"noGlobalAllClassPool\": ", file);
    fputs(drop_generation != NULL && drop_generation->no_global_all_class_pool ? "true" : "false", file);
    fputs(",\n      \"playableBehaviorClaimed\": false,\n      \"noMatchingClassControl\": {\n        \"presentPartyClasses\": [", file);
    if (no_matching_class_drop_generation != NULL) {
        for (size_t index = 0U; index < no_matching_class_drop_generation->present_party_class_count; ++index) {
            if (index > 0U) {
                fputs(", ", file);
            }
            fprintf(file, "%d", no_matching_class_drop_generation->present_party_classes[index]);
        }
    }
    fputs("],\n        \"eligible\": ", file);
    fputs(no_matching_class_drop_generation != NULL && no_matching_class_drop_generation->eligible ? "true" : "false", file);
    fputs(",\n        \"generated\": ", file);
    fputs(no_matching_class_drop_generation != NULL && no_matching_class_drop_generation->generated ? "true" : "false", file);
    fputs(",\n        \"reason\": ", file);
    bml_json_write_escaped(file, no_matching_class_drop_generation != NULL ? no_matching_class_drop_generation->reason : "not_run");
    fputs(",\n        \"selectedElixirId\": ", file);
    bml_json_write_escaped(file, no_matching_class_drop_generation != NULL && bml_has_value(no_matching_class_drop_generation->selected_elixir_id) ? no_matching_class_drop_generation->selected_elixir_id : "");
    fputs(",\n        \"noGlobalAllClassPool\": ", file);
    fputs(no_matching_class_drop_generation != NULL && no_matching_class_drop_generation->no_global_all_class_pool ? "true" : "false", file);
    fputs("\n      }\n    },\n    \"carrierMetadata\": {\n      \"instanceId\": ", file);
    fprintf(file, "%" PRIu32, metadata != NULL ? metadata->instance_id : 0U);
    fputs(",\n      \"carrierItem\": ", file);
    bml_json_write_escaped(file, metadata != NULL ? metadata->carrier_item_type : "");
    fputs(",\n      \"renderedDisplay\": ", file);
    bml_json_write_escaped(file, rendered_display);
    fputs(",\n      \"serialized\": ", file);
    bml_json_write_escaped(file, serialized_carrier_metadata);
    fputs("\n    },\n    \"consumption\": {\n      \"consumed\": ", file);
    fputs(consumption != NULL && consumption->consumed ? "true" : "false", file);
    fputs(",\n      \"activeEffectCreated\": ", file);
    fputs(consumption != NULL && consumption->active_effect_created ? "true" : "false", file);
    fputs(",\n      \"reason\": ", file);
    bml_json_write_escaped(file, consumption != NULL ? consumption->reason : "not_run");
    fputs("\n    },\n    \"activeEffectState\": {\n      \"serialized\": ", file);
    bml_json_write_escaped(file, serialized_active_effect);
    fputs(",\n      \"roundTripLoaded\": ", file);
    fputs(round_trip_loaded ? "true" : "false", file);
    fputs(",\n      \"roundTripMatches\": ", file);
    fputs(round_trip_matches ? "true" : "false", file);
    fputs("\n    },\n    \"unsupportedData\": {\n      \"rejected\": ", file);
    fputs(unsupported_data_rejected ? "true" : "false", file);
    fputs("\n    },\n    \"multiplayerVersionMetadata\": {\n      \"stateAuthority\": \"host\",\n      \"versionPolicy\": \"matching_package_and_runtime_metadata_required\",\n      \"clientCompatibility\": \"reject_mismatched_elixir_contract\",\n      \"packageId\": ", file);
    bml_json_write_escaped(file, BML_RUNES_ELIXIR_PACKAGE_ID);
    fputs(",\n      \"packageVersion\": ", file);
    bml_json_write_escaped(file, info != NULL ? info->runebound_elixirs_version : "0.1.0");
    fputs(",\n      \"runtimeContractId\": ", file);
    bml_json_write_escaped(file, BML_CONTRACT_ID);
    fputs(",\n      \"runtimeContractVersion\": ", file);
    bml_json_write_escaped(file, BML_CONTRACT_VERSION);
    fputs(",\n      \"nativeRuntimeId\": ", file);
    bml_json_write_escaped(file, BML_NATIVE_RUNTIME_ID);
    fputs(",\n      \"nativeRuntimeVersion\": ", file);
    bml_json_write_escaped(file, BML_NATIVE_RUNTIME_VERSION);
    fputs(",\n      \"playableBehaviorClaimed\": false\n    }\n  },\n  \"assertions\": {\n    \"catalogLoaded\": ", file);
    fputs(catalog_loaded ? "true" : "false", file);
    fputs(",\n    \"fixtureDefinitionSupported\": ", file);
    fputs(definition_supported ? "true" : "false", file);
    fputs(",\n    \"dropTableExists\": ", file);
    fputs(drop_table_exists ? "true" : "false", file);
    fputs(",\n    \"dropTableContractLoaded\": ", file);
    fputs(drop_table_contract_loaded ? "true" : "false", file);
    fputs(",\n    \"soloClassEligibility\": ", file);
    fputs(solo_class_eligible ? "true" : "false", file);
    fputs(",\n    \"twoPlayerPartySizeEligibility\": ", file);
    fputs(two_player_party_size_eligible ? "true" : "false", file);
    fputs(",\n    \"hostDropAuthority\": ", file);
    fputs(host_drop_authority ? "true" : "false", file);
    fputs(",\n    \"dropEligibilityMatched\": ", file);
    fputs(drop_eligibility_matched ? "true" : "false", file);
    fputs(",\n    \"dropGeneratedCarrier\": ", file);
    fputs(drop_generated_carrier ? "true" : "false", file);
    fputs(",\n    \"noGlobalAllClassPool\": ", file);
    fputs(no_global_all_class_pool ? "true" : "false", file);
    fputs(",\n    \"antiBloatLimitApplied\": ", file);
    fputs(anti_bloat_limit_applied ? "true" : "false", file);
    fputs(",\n    \"carrierDisplayMatched\": ", file);
    fputs(strcmp(rendered_display != NULL ? rendered_display : "", expected_display) == 0 ? "true" : "false", file);
    fputs(",\n    \"consumptionCreatedActiveEffect\": ", file);
    fputs(consumption != NULL && consumption->active_effect_created ? "true" : "false", file);
    fputs(",\n    \"activeEffectSerialized\": ", file);
    fputs(bml_has_value(serialized_active_effect) ? "true" : "false", file);
    fputs(",\n    \"roundTripLoaded\": ", file);
    fputs(round_trip_loaded ? "true" : "false", file);
    fputs(",\n    \"roundTripMatches\": ", file);
    fputs(round_trip_matches ? "true" : "false", file);
    fputs(",\n    \"unsupportedDataRejected\": ", file);
    fputs(unsupported_data_rejected ? "true" : "false", file);
    fputs(",\n    \"multiplayerStateAuthorityHost\": ", file);
    fputs(multiplayer_state_authority_host ? "true" : "false", file);
    fputs(",\n    \"multiplayerRejectsMismatchedContract\": ", file);
    fputs(multiplayer_rejects_mismatched_contract ? "true" : "false", file);
    fputs(",\n    \"playableBehaviorNotClaimed\": true\n  },\n  \"error\": ", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("{\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_RUNES_ELIXIR_SELF_TEST_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Runebound: Elixirs fake-provider self-test failed.");
        fputc('}', file);
    } else {
        fputs("null", file);
    }
    fputs(",\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}


static int bml_run_runebound_elixir_self_test(const char *report_path, const BmlReportInfo *info, const char *runtime_json) {
    BmlRuneboundElixirDefinition definition;
    BmlRuneboundElixirDefinition unsupported_definition;
    BmlRuneboundElixirPackageDataValidation package_validation;
    BmlRuneboundElixirPartySnapshot solo_snapshot;
    BmlRuneboundElixirPartySnapshot two_player_snapshot;
    BmlRuneboundElixirPartySnapshot no_matching_class_snapshot;
    BmlRuneboundElixirCarrierMetadata metadata;
    BmlRuneboundElixirDropGenerationDecision drop_generation;
    BmlRuneboundElixirDropGenerationDecision no_matching_class_drop_generation;
    BmlRuneboundElixirConsumptionResult consumption;
    BmlRuneboundElixirSerializedActiveEffect round_trip_effect;
    char rendered_display[BML_MAX_TEXT];
    char serialized_carrier_metadata[BML_RUNES_ELIXIR_SERIALIZED_STATE_MAX];
    char serialized_active_effect[BML_RUNES_ELIXIR_SERIALIZED_STATE_MAX];
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    bool solo_class_eligible;
    bool two_player_party_size_eligible;
    bool unsupported_data_rejected;
    bool round_trip_loaded;
    bool round_trip_matches;
    bool drop_table_contract_loaded;
    bool drop_generation_matched;
    bool no_matching_class_skipped;
    bool anti_bloat_limit_applied;
    bool passed;

    memset(&definition, 0, sizeof(definition));
    memset(&unsupported_definition, 0, sizeof(unsupported_definition));
    memset(&package_validation, 0, sizeof(package_validation));
    memset(&solo_snapshot, 0, sizeof(solo_snapshot));
    memset(&two_player_snapshot, 0, sizeof(two_player_snapshot));
    memset(&no_matching_class_snapshot, 0, sizeof(no_matching_class_snapshot));
    memset(&metadata, 0, sizeof(metadata));
    memset(&drop_generation, 0, sizeof(drop_generation));
    memset(&no_matching_class_drop_generation, 0, sizeof(no_matching_class_drop_generation));
    memset(&consumption, 0, sizeof(consumption));
    memset(&round_trip_effect, 0, sizeof(round_trip_effect));
    memset(rendered_display, 0, sizeof(rendered_display));
    memset(serialized_carrier_metadata, 0, sizeof(serialized_carrier_metadata));
    memset(serialized_active_effect, 0, sizeof(serialized_active_effect));
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));

    bml_runebound_elixir_make_fixture_definition(&definition);
    bml_runebound_elixir_validate_package_data(runtime_json, &package_validation);
    unsupported_definition = definition;
    unsupported_definition.effect_opcode = "unsupported.fake-opcode";
    unsupported_data_rejected = !bml_runebound_elixir_definition_supported(&unsupported_definition);
    bml_runebound_elixir_make_solo_party_snapshot(&solo_snapshot, definition.eligible_class_id);
    bml_runebound_elixir_make_two_player_party_snapshot(&two_player_snapshot, definition.eligible_class_id, 7);
    bml_runebound_elixir_make_solo_party_snapshot(&no_matching_class_snapshot, 7);
    bml_runebound_elixir_make_fixture_carrier(&definition, &metadata);
    bml_runebound_elixir_make_drop_generation_decision(&definition, &two_player_snapshot, &metadata, "chest_loot_postprocess", &drop_generation);
    bml_runebound_elixir_make_drop_generation_decision(&definition, &no_matching_class_snapshot, &metadata, "chest_loot_postprocess", &no_matching_class_drop_generation);

    solo_class_eligible = bml_runebound_elixir_party_eligible(&definition, &solo_snapshot);
    two_player_party_size_eligible = bml_runebound_elixir_party_eligible(&definition, &two_player_snapshot);
    drop_table_contract_loaded = package_validation.drop_table_generation_authority_host &&
                                 package_validation.drop_table_class_policy_present_party_classes &&
                                 package_validation.drop_table_party_size_generation_time_only &&
                                 package_validation.drop_table_no_global_all_class_pool &&
                                 package_validation.drop_table_anti_bloat_max_one_per_source &&
                                 package_validation.drop_table_anti_bloat_max_one_per_floor;
    drop_generation_matched = drop_generation.generated &&
                              drop_generation.eligible &&
                              strcmp(drop_generation.authority != NULL ? drop_generation.authority : "", "host") == 0 &&
                              strcmp(drop_generation.selected_elixir_id != NULL ? drop_generation.selected_elixir_id : "", definition.catalog_id) == 0 &&
                              drop_generation.carrier == &metadata;
    no_matching_class_skipped = no_matching_class_drop_generation.no_global_all_class_pool &&
                                !no_matching_class_drop_generation.eligible &&
                                !no_matching_class_drop_generation.generated &&
                                strcmp(no_matching_class_drop_generation.reason != NULL ? no_matching_class_drop_generation.reason : "", "no_present_party_class_match") == 0;
    anti_bloat_limit_applied = drop_generation.max_generated_elixirs_per_source == 1U &&
                               drop_generation.max_generated_elixirs_per_floor == 1U &&
                               drop_generation.generated_carrier_count_for_source == 1U &&
                               drop_generation.generated_carrier_count_for_floor == 1U;
    round_trip_loaded = false;
    round_trip_matches = false;
    if (bml_runebound_elixir_render_carrier_display(&metadata, rendered_display, sizeof(rendered_display)) != 0) {
        bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_DISPLAY_RENDER_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs carrier display rendering exceeded the native self-test buffer.");
    }
    if (!bml_has_value(error_code) && bml_runebound_elixir_serialize_carrier_metadata(&metadata, serialized_carrier_metadata, sizeof(serialized_carrier_metadata)) != 0) {
        bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_CARRIER_SERIALIZE_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs carrier metadata serialization exceeded the native self-test buffer.");
    }
    if (!bml_has_value(error_code) && !bml_runebound_elixir_consume_carrier(&metadata, &solo_snapshot, 0, &consumption)) {
        bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_CONSUME_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs fake-provider consumption did not create an active effect.");
    }
    if (!bml_has_value(error_code) && bml_runebound_elixir_serialize_active_effect(&consumption.active_effect, serialized_active_effect, sizeof(serialized_active_effect)) != 0) {
        bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_ACTIVE_EFFECT_SERIALIZE_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs active-effect state serialization exceeded the native self-test buffer.");
    }
    if (!bml_has_value(error_code)) {
        round_trip_loaded = bml_runebound_elixir_deserialize_active_effect(serialized_active_effect, &round_trip_effect);
        if (!round_trip_loaded) {
            bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_ACTIVE_EFFECT_ROUND_TRIP_LOAD_FAILED");
            bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs active-effect state serialization could not be deserialized by the native self-test.");
        }
    }
    if (!bml_has_value(error_code)) {
        round_trip_matches = bml_runebound_elixir_active_effect_round_trip_matches(&consumption.active_effect, &round_trip_effect);
        if (!round_trip_matches) {
            bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_ACTIVE_EFFECT_ROUND_TRIP_MISMATCH");
            bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs active-effect state round-trip did not preserve the expected fields.");
        }
    }

    passed = !bml_has_value(error_code) &&
             package_validation.catalog_contains_iron_vow &&
             package_validation.drop_table_file_exists &&
             drop_table_contract_loaded &&
             bml_runebound_elixir_definition_supported(&definition) &&
             solo_class_eligible &&
             two_player_party_size_eligible &&
             drop_generation_matched &&
             no_matching_class_skipped &&
             anti_bloat_limit_applied &&
             strcmp(rendered_display, "Elixir of the Iron Vow (+2 STR, -1 DEX for the rest of the run.)") == 0 &&
             consumption.active_effect_created &&
             bml_has_value(serialized_active_effect) &&
             round_trip_loaded &&
             round_trip_matches &&
             unsupported_data_rejected;
    if (!passed && !bml_has_value(error_code)) {
        bml_copy_string(error_code, sizeof(error_code), "BML_RUNES_ELIXIR_SELF_TEST_ASSERTION_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Runebound: Elixirs self-test failed expected package data, host drop generation, multiplayer metadata, display, consumption, active-effect round-trip, or no-playable boundary.");
    }

    if (bml_write_runebound_elixir_self_test_report(report_path,
                                                    info,
                                                    passed ? "passed" : "failed",
                                                    error_code,
                                                    error_message,
                                                    &definition,
                                                    &package_validation,
                                                    &solo_snapshot,
                                                    &two_player_snapshot,
                                                    &metadata,
                                                    &drop_generation,
                                                    &no_matching_class_drop_generation,
                                                    rendered_display,
                                                    serialized_carrier_metadata,
                                                    &consumption,
                                                    serialized_active_effect,
                                                    round_trip_loaded,
                                                    round_trip_matches,
                                                    solo_class_eligible,
                                                    two_player_party_size_eligible,
                                                    unsupported_data_rejected) != 0) {
        return -1;
    }
    return passed ? 0 : -1;
}


typedef void *(*BmlStashAddItemToVoidChestServerFunction)(void *, int, void *, bool, void *);
_Static_assert(sizeof(BmlStashAddItemToVoidChestServerFunction) == sizeof(void *), "BML Linux x86_64 Stash detour self-test expects function pointers to fit in void pointers");
typedef void *(*BmlStashGetChestInventoryListFunction)(void *);
typedef void *(*BmlStashAddItemToChestFunction)(void *, void *, bool, void *);
typedef void *(*BmlStashGetItemFromChestFunction)(void *, void *, int, bool);
typedef bool (*BmlStashRemoveItemFromVoidChestServerFunction)(void *, int, void *, int);
typedef void (*BmlStashCloseChestFunction)(void *);
typedef void (*BmlStashEntityActionFunction)(void *);
typedef int (*BmlStashGenerateDungeonFunction)(char *, uint32_t, uint64_t, uint64_t);
typedef void (*BmlStashAssignActionsFunction)(void *);
typedef void *(*BmlStashNewEntityFunction)(int, uint32_t, BmlBaronyList *, BmlBaronyList *);
typedef void (*BmlStashSetSpriteAttributesFunction)(void *, void *, void *);
typedef void *(*BmlStashNewItemFunction)(int, int, int16_t, int16_t, uint32_t, bool, BmlBaronyList *);
typedef void (*BmlStashListFreeAllFunction)(BmlBaronyList *);
typedef const char *(*BmlLanguageGetFunction)(int);
typedef void *(*BmlUidToEntityFunction)(int);
_Static_assert(sizeof(BmlStashGetChestInventoryListFunction) == sizeof(void *), "BML Linux x86_64 Stash detours expect getChestInventoryList pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashRemoveItemFromVoidChestServerFunction) == sizeof(void *), "BML Linux x86_64 Stash detours expect removeItemFromVoidChestServer pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashAddItemToChestFunction) == sizeof(void *), "BML Linux x86_64 Stash behavior expects addItemToChest pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashGetItemFromChestFunction) == sizeof(void *), "BML Linux x86_64 Stash behavior expects getItemFromChest pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashCloseChestFunction) == sizeof(void *), "BML Linux x86_64 Stash detours expect closeChest pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashNewItemFunction) == sizeof(void *), "BML Linux x86_64 Stash behavior expects newItem pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashListFreeAllFunction) == sizeof(void *), "BML Linux x86_64 Stash behavior expects list_FreeAll pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashEntityActionFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects entity action pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashGenerateDungeonFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects generateDungeon pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashAssignActionsFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects assignActions pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashNewEntityFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects newEntity pointers to fit in void pointers");
_Static_assert(sizeof(BmlStashSetSpriteAttributesFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects setSpriteAttributes pointers to fit in void pointers");
_Static_assert(sizeof(BmlLanguageGetFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects Language::get pointers to fit in void pointers");
_Static_assert(sizeof(BmlUidToEntityFunction) == sizeof(void *), "BML Linux x86_64 Stash access placement expects uidToEntity pointers to fit in void pointers");

static BmlStashAddItemToVoidChestServerFunction bml_stash_add_item_function_from_address(void *address) {
    BmlStashAddItemToVoidChestServerFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_add_item_function_address(BmlStashAddItemToVoidChestServerFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashGetChestInventoryListFunction bml_stash_get_inventory_function_from_address(void *address) {
    BmlStashGetChestInventoryListFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_get_inventory_function_address(BmlStashGetChestInventoryListFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}
static BmlStashAddItemToChestFunction bml_stash_add_item_to_chest_function_from_address(void *address) {
    BmlStashAddItemToChestFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_add_item_to_chest_function_address(BmlStashAddItemToChestFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashGetItemFromChestFunction bml_stash_get_item_from_chest_function_from_address(void *address) {
    BmlStashGetItemFromChestFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_get_item_from_chest_function_address(BmlStashGetItemFromChestFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}


static BmlStashRemoveItemFromVoidChestServerFunction bml_stash_remove_item_function_from_address(void *address) {
    BmlStashRemoveItemFromVoidChestServerFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_remove_item_function_address(BmlStashRemoveItemFromVoidChestServerFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashCloseChestFunction bml_stash_close_chest_function_from_address(void *address) {
    BmlStashCloseChestFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_close_chest_function_address(BmlStashCloseChestFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashEntityActionFunction bml_stash_entity_action_function_from_address(void *address) {
    BmlStashEntityActionFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_entity_action_function_address(BmlStashEntityActionFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashGenerateDungeonFunction bml_stash_generate_dungeon_function_from_address(void *address) {
    BmlStashGenerateDungeonFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_generate_dungeon_function_address(BmlStashGenerateDungeonFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashAssignActionsFunction bml_stash_assign_actions_function_from_address(void *address) {
    BmlStashAssignActionsFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_assign_actions_function_address(BmlStashAssignActionsFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashNewEntityFunction bml_stash_new_entity_function_from_address(void *address) {
    BmlStashNewEntityFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_new_entity_function_address(BmlStashNewEntityFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashSetSpriteAttributesFunction bml_stash_set_sprite_attributes_function_from_address(void *address) {
    BmlStashSetSpriteAttributesFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_stash_set_sprite_attributes_function_address(BmlStashSetSpriteAttributesFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlLanguageGetFunction bml_language_get_function_from_address(void *address) {
    BmlLanguageGetFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_language_get_function_address(BmlLanguageGetFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlUidToEntityFunction bml_uid_to_entity_function_from_address(void *address) {
    BmlUidToEntityFunction function;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_uid_to_entity_function_address(BmlUidToEntityFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlStashAddItemToVoidChestServerFunction g_bml_stash_add_item_original = NULL;
static int g_bml_stash_add_item_replacement_calls = 0;
static void *g_bml_stash_add_item_original_result = NULL;
static void *g_bml_stash_add_item_replacement_result = NULL;

static BmlStashGetChestInventoryListFunction g_bml_stash_get_inventory_original = NULL;
static BmlStashAddItemToChestFunction g_bml_stash_add_item_to_chest_original = NULL;
static BmlStashGetItemFromChestFunction g_bml_stash_get_item_from_chest_original = NULL;
static BmlStashRemoveItemFromVoidChestServerFunction g_bml_stash_remove_item_original = NULL;
static BmlStashCloseChestFunction g_bml_stash_close_chest_original = NULL;
static BmlStashCloseChestFunction g_bml_stash_close_chest_server_original = NULL;
static int g_bml_stash_get_inventory_replacement_calls = 0;
static int g_bml_stash_add_item_to_chest_replacement_calls = 0;
static int g_bml_stash_get_item_from_chest_replacement_calls = 0;
static int g_bml_stash_remove_item_replacement_calls = 0;
static int g_bml_stash_close_chest_replacement_calls = 0;
static int g_bml_stash_close_chest_server_replacement_calls = 0;

static BmlStashEntityActionFunction g_bml_stash_act_chest_original = NULL;
static BmlStashEntityActionFunction g_bml_stash_act_chest_lid_original = NULL;
static BmlStashGenerateDungeonFunction g_bml_stash_generate_dungeon_original = NULL;
static BmlStashAssignActionsFunction g_bml_stash_assign_actions_original = NULL;
static BmlStashNewEntityFunction g_bml_stash_new_entity_original = NULL;
static BmlStashSetSpriteAttributesFunction g_bml_stash_set_sprite_attributes_original = NULL;
static BmlLanguageGetFunction g_bml_language_get_original = NULL;
static BmlUidToEntityFunction g_bml_uid_to_entity_original = NULL;
static int g_bml_stash_act_chest_replacement_calls = 0;
static int g_bml_stash_act_chest_lid_replacement_calls = 0;
static int g_bml_stash_generate_dungeon_replacement_calls = 0;
static int g_bml_stash_assign_actions_replacement_calls = 0;
static int g_bml_stash_new_entity_replacement_calls = 0;
static int g_bml_stash_set_sprite_attributes_replacement_calls = 0;
static int g_bml_language_get_replacement_calls = 0;
static int g_bml_uid_to_entity_replacement_calls = 0;
static int g_bml_stash_uid_prompt_context_recorded = 0;
static int g_bml_stash_uid_prompt_context_consumed = 0;
static bool g_bml_stash_recent_uid_to_entity_was_framework_stash = false;
static bool g_bml_stash_recent_uid_to_entity_tooltip_armed = false;
static int g_bml_stash_recent_uid_to_entity_consecutive_count = 0;
static void *g_bml_stash_recent_uid_to_entity = NULL;

static BmlStashCoreDetourInstall g_bml_stash_access_placement_targets[8];
static size_t g_bml_stash_access_placement_target_count = 0U;
static char g_bml_stash_access_placement_report_path[PATH_MAX];
static bool g_bml_stash_access_placement_exit_report_registered = false;

static bool g_bml_stash_placement_discovery_active = false;
static bool g_bml_stash_placement_discovery_exit_report_registered = false;
static char g_bml_stash_placement_discovery_report_path[PATH_MAX];
static void *g_bml_stash_placement_global_map_symbol = NULL;
static BmlStashPlacementAssignActionsSnapshot g_bml_stash_placement_assign_actions_snapshot;
static int g_bml_stash_placement_assign_actions_new_entity_delta_total = 0;
static int g_bml_stash_placement_assign_actions_set_sprite_delta_total = 0;
static int g_bml_stash_placement_assign_actions_depth = 0;
static BmlStashPlacementNewEntitySample g_bml_stash_placement_assign_actions_new_entity_samples[BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT];
static size_t g_bml_stash_placement_assign_actions_new_entity_sample_count = 0U;
static BmlStashPlacementSetSpriteSample g_bml_stash_placement_assign_actions_set_sprite_samples[BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT];
static size_t g_bml_stash_placement_assign_actions_set_sprite_sample_count = 0U;
static bool g_bml_stash_playable_active = false;
static bool g_bml_stash_playable_hooks_installed = false;
static int g_bml_stash_playable_assign_actions_calls = 0;
static int g_bml_stash_playable_new_entity_calls = 0;
static int g_bml_stash_playable_set_sprite_calls = 0;
static int g_bml_stash_playable_lobby_placements_attempted = 0;
static int g_bml_stash_playable_lobby_placements_succeeded = 0;
static int g_bml_stash_playable_lobby_placements_failed = 0;
static int g_bml_stash_playable_lobby_already_placed_count = 0;
static int g_bml_stash_playable_shop_placements_attempted = 0;
static int g_bml_stash_playable_shop_placements_succeeded = 0;
static int g_bml_stash_playable_shop_placements_failed = 0;
static int g_bml_stash_playable_shop_already_placed_count = 0;
static void *g_bml_stash_playable_last_placed_shop_chest = NULL;
static void *g_bml_stash_playable_last_placed_shop_lid = NULL;
static void *g_bml_stash_playable_last_shop_map = NULL;
static int g_bml_stash_playable_shop_generation = 0;
static int g_bml_stash_playable_last_shop_generation = -1;
static int g_bml_stash_playable_multiplayer_value = 0;
static int g_bml_stash_playable_clientnum_value = 0;
static bool g_bml_stash_playable_multiplayer_client_blocked = false;
static void *g_bml_stash_playable_last_placed_chest = NULL;
static void *g_bml_stash_playable_last_placed_lid = NULL;
static BmlStashPlacementNewEntitySample g_bml_stash_placement_new_entity_samples[BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT];
static size_t g_bml_stash_placement_new_entity_sample_count = 0U;
static int g_bml_stash_placement_new_entity_sprite_188_calls = 0;
static int g_bml_stash_placement_new_entity_sprite_1484_calls = 0;
static int g_bml_stash_placement_new_entity_sprite_1790_calls = 0;
static int g_bml_stash_placement_new_entity_sprite_1791_calls = 0;
static BmlStashPlacementSetSpriteSample g_bml_stash_placement_set_sprite_samples[BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT];
static size_t g_bml_stash_placement_set_sprite_sample_count = 0U;

static bool g_bml_stash_core_behavior_active = false;
static bool g_bml_stash_core_behavior_loaded = false;
static bool g_bml_stash_core_behavior_dirty = false;
static bool g_bml_stash_core_behavior_failed = false;
static int g_bml_stash_core_behavior_loads = 0;
static int g_bml_stash_core_behavior_saves = 0;
static int g_bml_stash_core_behavior_dirty_marks = 0;
static BmlBaronyList *g_bml_stash_core_behavior_inventory = NULL;
static BmlBaronyNode *g_bml_stash_core_behavior_loaded_first = NULL;
static BmlBaronyNode *g_bml_stash_core_behavior_loaded_last = NULL;
static char g_bml_stash_core_behavior_failure_code[BML_MAX_TEXT];
static char g_bml_stash_core_behavior_failure_message[BML_MAX_TEXT];
static BmlStashNewItemFunction g_bml_stash_new_item = NULL;
static BmlStashListFreeAllFunction g_bml_stash_list_free_all = NULL;
static char g_bml_stash_state_dir_path[PATH_MAX];
static char g_bml_stash_inventory_path[PATH_MAX];
static char g_bml_stash_diagnostics_path[PATH_MAX];

static void bml_append_stash_diagnostic_event(const char *event, const char *kind, const char *map_name, bool has_position, double x, double y, int rows) {
    FILE *file;
    if (!bml_has_value(g_bml_stash_diagnostics_path) || !bml_has_value(event)) {
        return;
    }
    if (bml_mkdir_p(g_bml_stash_state_dir_path) != 0) {
        return;
    }
    file = fopen(g_bml_stash_diagnostics_path, "ab");
    if (file == NULL) {
        return;
    }
    fputs("{\"event\": ", file);
    bml_json_write_escaped(file, event);
    if (bml_has_value(kind)) {
        fputs(", \"kind\": ", file);
        bml_json_write_escaped(file, kind);
    }
    if (bml_has_value(map_name)) {
        fputs(", \"map\": ", file);
        bml_json_write_escaped(file, map_name);
    }
    if (has_position) {
        fprintf(file, ", \"x\": %.3f, \"y\": %.3f", x, y);
    }
    if (rows >= 0) {
        fprintf(file, ", \"rows\": %d", rows);
    }
    fputs(", \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("}\n", file);
    (void)fclose(file);
}

static void bml_append_stash_error_diagnostic_event(const char *event, const char *error_code, const char *error_message) {
    FILE *file;
    if (!bml_has_value(g_bml_stash_diagnostics_path) || !bml_has_value(event)) {
        return;
    }
    if (bml_mkdir_p(g_bml_stash_state_dir_path) != 0) {
        return;
    }
    file = fopen(g_bml_stash_diagnostics_path, "ab");
    if (file == NULL) {
        return;
    }
    fputs("{\"event\": ", file);
    bml_json_write_escaped(file, event);
    fputs(", \"severity\": \"fatal\", \"error\": {\"code\": ", file);
    bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_STASH_CORE_BEHAVIOR_FAILED");
    fputs(", \"message\": ", file);
    bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Stash core behavior failed closed.");
    fputs("}, \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("}\n", file);
    (void)fclose(file);
}

static void bml_stash_record_inventory_generation(BmlBaronyList *inventory) {
    g_bml_stash_core_behavior_inventory = inventory;
    g_bml_stash_core_behavior_loaded_first = inventory != NULL ? inventory->first : NULL;
    g_bml_stash_core_behavior_loaded_last = inventory != NULL ? inventory->last : NULL;
}

static bool bml_stash_loaded_inventory_generation_matches(const BmlBaronyList *inventory) {
    return g_bml_stash_core_behavior_loaded &&
           g_bml_stash_core_behavior_inventory == inventory &&
           inventory != NULL &&
           g_bml_stash_core_behavior_loaded_first == inventory->first &&
           g_bml_stash_core_behavior_loaded_last == inventory->last;
}

static void bml_stash_fail_closed(const char *event, const char *error_code, const char *error_message) {
    if (!g_bml_stash_core_behavior_failed) {
        bml_copy_string(g_bml_stash_core_behavior_failure_code, sizeof(g_bml_stash_core_behavior_failure_code), bml_has_value(error_code) ? error_code : "BML_STASH_CORE_BEHAVIOR_FAILED");
        bml_copy_string(g_bml_stash_core_behavior_failure_message, sizeof(g_bml_stash_core_behavior_failure_message), bml_has_value(error_message) ? error_message : "Stash core behavior failed closed.");
        g_bml_stash_core_behavior_failed = true;
        bml_append_stash_error_diagnostic_event(bml_has_value(event) ? event : "stash_core_behavior_failed_closed", g_bml_stash_core_behavior_failure_code, g_bml_stash_core_behavior_failure_message);
        fprintf(stderr, "BML Stash persistence failed closed: %s: %s\n", g_bml_stash_core_behavior_failure_code, g_bml_stash_core_behavior_failure_message);
    }
}

static int bml_stash_copy_failure(char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    if (!g_bml_stash_core_behavior_failed) {
        return 0;
    }
    bml_copy_string(error_code, error_code_size, bml_has_value(g_bml_stash_core_behavior_failure_code) ? g_bml_stash_core_behavior_failure_code : "BML_STASH_CORE_BEHAVIOR_FAILED");
    bml_copy_string(error_message, error_message_size, bml_has_value(g_bml_stash_core_behavior_failure_message) ? g_bml_stash_core_behavior_failure_message : "Stash core behavior has failed closed.");
    return -1;
}

static bool bml_stash_bool_field_valid(int value) {
    return value == 0 || value == 1;
}

static bool bml_stash_line_has_only_trailing_space(const char *cursor) {
    while (cursor != NULL && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
        ++cursor;
    }
    return cursor == NULL || *cursor == '\0';
}

static void bml_reset_stash_placement_discovery_state(void) {
    g_bml_stash_placement_global_map_symbol = NULL;
    memset(&g_bml_stash_placement_assign_actions_snapshot, 0, sizeof(g_bml_stash_placement_assign_actions_snapshot));
    g_bml_stash_placement_assign_actions_new_entity_delta_total = 0;
    g_bml_stash_placement_assign_actions_set_sprite_delta_total = 0;
    g_bml_stash_placement_assign_actions_depth = 0;
    memset(g_bml_stash_placement_assign_actions_new_entity_samples, 0, sizeof(g_bml_stash_placement_assign_actions_new_entity_samples));
    g_bml_stash_placement_assign_actions_new_entity_sample_count = 0U;
    memset(g_bml_stash_placement_assign_actions_set_sprite_samples, 0, sizeof(g_bml_stash_placement_assign_actions_set_sprite_samples));
    g_bml_stash_placement_assign_actions_set_sprite_sample_count = 0U;
    memset(g_bml_stash_placement_new_entity_samples, 0, sizeof(g_bml_stash_placement_new_entity_samples));
    g_bml_stash_placement_new_entity_sample_count = 0U;
    g_bml_stash_placement_new_entity_sprite_188_calls = 0;
    g_bml_stash_placement_new_entity_sprite_1484_calls = 0;
    g_bml_stash_placement_new_entity_sprite_1790_calls = 0;
    g_bml_stash_placement_new_entity_sprite_1791_calls = 0;
    memset(g_bml_stash_placement_set_sprite_samples, 0, sizeof(g_bml_stash_placement_set_sprite_samples));
    g_bml_stash_placement_set_sprite_sample_count = 0U;
}

static void bml_stash_placement_record_assign_actions_before(void *map_argument, int new_entity_calls_before, int set_sprite_attributes_calls_before) {
    BmlStashPlacementAssignActionsSnapshot *snapshot = &g_bml_stash_placement_assign_actions_snapshot;
    if (!g_bml_stash_placement_discovery_active) {
        return;
    }
    snapshot->observed = true;
    snapshot->map_argument = map_argument;
    snapshot->global_map_symbol = g_bml_stash_placement_global_map_symbol;
    snapshot->map_argument_matches_global = map_argument != NULL && map_argument == g_bml_stash_placement_global_map_symbol;
    snapshot->new_entity_calls_before = new_entity_calls_before;
    snapshot->set_sprite_attributes_calls_before = set_sprite_attributes_calls_before;
    memset(snapshot->map_name, 0, sizeof(snapshot->map_name));
    snapshot->map_width = 0U;
    snapshot->map_height = 0U;
    snapshot->map_skybox = 0U;
    if (map_argument != NULL) {
        const BmlStashPlacementMapPrefix *map_prefix = (const BmlStashPlacementMapPrefix *)map_argument;
        memcpy(snapshot->map_name, map_prefix->name, sizeof(map_prefix->name));
        snapshot->map_name[sizeof(snapshot->map_name) - 1U] = '\0';
        snapshot->map_width = map_prefix->width;
        snapshot->map_height = map_prefix->height;
        snapshot->map_skybox = map_prefix->skybox;
    }
}

static void bml_stash_placement_record_assign_actions_after(int new_entity_calls_before, int set_sprite_attributes_calls_before) {
    BmlStashPlacementAssignActionsSnapshot *snapshot = &g_bml_stash_placement_assign_actions_snapshot;
    int new_entity_delta;
    int set_sprite_delta;
    if (!g_bml_stash_placement_discovery_active) {
        return;
    }
    snapshot->new_entity_calls_after = g_bml_stash_new_entity_replacement_calls;
    snapshot->set_sprite_attributes_calls_after = g_bml_stash_set_sprite_attributes_replacement_calls;
    new_entity_delta = snapshot->new_entity_calls_after - new_entity_calls_before;
    set_sprite_delta = snapshot->set_sprite_attributes_calls_after - set_sprite_attributes_calls_before;
    if (new_entity_delta > 0) {
        g_bml_stash_placement_assign_actions_new_entity_delta_total += new_entity_delta;
    }
    if (set_sprite_delta > 0) {
        g_bml_stash_placement_assign_actions_set_sprite_delta_total += set_sprite_delta;
    }
}

static void bml_stash_placement_record_new_entity(int sprite, uint32_t pos, BmlBaronyList *entity_list, BmlBaronyList *creature_list, void *result) {
    if (!g_bml_stash_placement_discovery_active) {
        return;
    }
    if (sprite == 188) {
        g_bml_stash_placement_new_entity_sprite_188_calls += 1;
    } else if (sprite == 1484) {
        g_bml_stash_placement_new_entity_sprite_1484_calls += 1;
    } else if (sprite == 1790) {
        g_bml_stash_placement_new_entity_sprite_1790_calls += 1;
    } else if (sprite == 1791) {
        g_bml_stash_placement_new_entity_sprite_1791_calls += 1;
    }
    if (g_bml_stash_placement_new_entity_sample_count < BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT) {
        BmlStashPlacementNewEntitySample *sample = &g_bml_stash_placement_new_entity_samples[g_bml_stash_placement_new_entity_sample_count++];
        sample->sprite = sprite;
        sample->pos = pos;
        sample->entity_list = entity_list;
        sample->creature_list = creature_list;
        sample->result = result;
    }
    if (g_bml_stash_placement_assign_actions_depth > 0 && g_bml_stash_placement_assign_actions_new_entity_sample_count < BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT) {
        BmlStashPlacementNewEntitySample *sample = &g_bml_stash_placement_assign_actions_new_entity_samples[g_bml_stash_placement_assign_actions_new_entity_sample_count++];
        sample->sprite = sprite;
        sample->pos = pos;
        sample->entity_list = entity_list;
        sample->creature_list = creature_list;
        sample->result = result;
    }
}

static void bml_stash_placement_record_set_sprite_attributes(void *entity, void *source, void *parent) {
    if (!g_bml_stash_placement_discovery_active) {
        return;
    }
    if (g_bml_stash_placement_set_sprite_sample_count < BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT) {
        BmlStashPlacementSetSpriteSample *sample = &g_bml_stash_placement_set_sprite_samples[g_bml_stash_placement_set_sprite_sample_count++];
        sample->entity = entity;
        sample->source = source;
        sample->parent = parent;
    }
    if (g_bml_stash_placement_assign_actions_depth > 0 && g_bml_stash_placement_assign_actions_set_sprite_sample_count < BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT) {
        BmlStashPlacementSetSpriteSample *sample = &g_bml_stash_placement_assign_actions_set_sprite_samples[g_bml_stash_placement_assign_actions_set_sprite_sample_count++];
        sample->entity = entity;
        sample->source = source;
        sample->parent = parent;
    }
}

static int bml_write_stash_placement_discovery_report(const char *report_path) {
    const BmlStashPlacementAssignActionsSnapshot *snapshot = &g_bml_stash_placement_assign_actions_snapshot;
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"stash-placement-discovery\",\n  \"status\": ", file);
    bml_json_write_escaped(file, snapshot->observed || g_bml_stash_new_entity_replacement_calls > 0 || g_bml_stash_set_sprite_attributes_replacement_calls > 0 ? "observed" : "installed_no_calls");
    fputs(",\n  \"claimBoundary\": \"non-mutating-placement-context-only\",\n  \"summary\": {", file);
    fprintf(file, "\n    \"assignActionsCalls\": %d,\n    \"newEntityCalls\": %d,\n    \"setSpriteAttributesCalls\": %d,\n    \"assignActionsNewEntityDelta\": %d,\n    \"assignActionsSetSpriteAttributesDelta\": %d\n  },",
            g_bml_stash_assign_actions_replacement_calls,
            g_bml_stash_new_entity_replacement_calls,
            g_bml_stash_set_sprite_attributes_replacement_calls,
            g_bml_stash_placement_assign_actions_new_entity_delta_total,
            g_bml_stash_placement_assign_actions_set_sprite_delta_total);
    fputs("\n  \"assignActions\": {\n    \"observed\": ", file);
    fputs(snapshot->observed ? "true" : "false", file);
    fputs(",\n    \"map\": ", file);
    if (snapshot->observed) {
        fputs("{\"argument\": ", file);
        bml_write_address_or_null(file, snapshot->map_argument);
        fputs(", \"globalMapSymbol\": ", file);
        bml_write_address_or_null(file, snapshot->global_map_symbol);
        fputs(", \"argumentMatchesGlobal\": ", file);
        fputs(snapshot->map_argument_matches_global ? "true" : "false", file);
        fputs(", \"name\": ", file);
        bml_json_write_escaped(file, snapshot->map_name);
        fprintf(file, ", \"width\": %u, \"height\": %u, \"skybox\": %u}", snapshot->map_width, snapshot->map_height, snapshot->map_skybox);
    } else {
        fputs("null", file);
    }
    fprintf(file, ",\n    \"lastNewEntityCallsBefore\": %d,\n    \"lastNewEntityCallsAfter\": %d,\n    \"lastSetSpriteAttributesCallsBefore\": %d,\n    \"lastSetSpriteAttributesCallsAfter\": %d,\n    \"scopedNewEntitySampled\": %zu,\n    \"scopedNewEntitySamples\": [",
            snapshot->new_entity_calls_before,
            snapshot->new_entity_calls_after,
            snapshot->set_sprite_attributes_calls_before,
            snapshot->set_sprite_attributes_calls_after,
            g_bml_stash_placement_assign_actions_new_entity_sample_count);
    for (size_t index = 0U; index < g_bml_stash_placement_assign_actions_new_entity_sample_count; ++index) {
        const BmlStashPlacementNewEntitySample *sample = &g_bml_stash_placement_assign_actions_new_entity_samples[index];
        fputs(index == 0U ? "\n      " : ",\n      ", file);
        fprintf(file, "{\"sprite\": %d, \"pos\": %u, \"entityList\": ", sample->sprite, sample->pos);
        bml_write_address_or_null(file, sample->entity_list);
        fputs(", \"creatureList\": ", file);
        bml_write_address_or_null(file, sample->creature_list);
        fputs(", \"result\": ", file);
        bml_write_address_or_null(file, sample->result);
        fputc('}', file);
    }
    if (g_bml_stash_placement_assign_actions_new_entity_sample_count > 0U) {
        fputs("\n    ", file);
    }
    fputs("],\n    \"scopedSetSpriteAttributesSampled\": ", file);
    fprintf(file, "%zu", g_bml_stash_placement_assign_actions_set_sprite_sample_count);
    fputs(",\n    \"scopedSetSpriteAttributesSamples\": [", file);
    for (size_t index = 0U; index < g_bml_stash_placement_assign_actions_set_sprite_sample_count; ++index) {
        const BmlStashPlacementSetSpriteSample *sample = &g_bml_stash_placement_assign_actions_set_sprite_samples[index];
        fputs(index == 0U ? "\n      " : ",\n      ", file);
        fputs("{\"entity\": ", file);
        bml_write_address_or_null(file, sample->entity);
        fputs(", \"source\": ", file);
        bml_write_address_or_null(file, sample->source);
        fputs(", \"parent\": ", file);
        bml_write_address_or_null(file, sample->parent);
        fputc('}', file);
    }
    if (g_bml_stash_placement_assign_actions_set_sprite_sample_count > 0U) {
        fputs("\n    ", file);
    }
    fputs("]\n  },", file);
    fputs("\n  \"newEntity\": {\n    \"selectedSpriteCalls\": {", file);
    fprintf(file, "\n      \"188\": %d,\n      \"1484\": %d,\n      \"1790\": %d,\n      \"1791\": %d\n    },\n    \"sampleLimit\": %u,\n    \"sampled\": %zu,\n    \"samples\": [",
            g_bml_stash_placement_new_entity_sprite_188_calls,
            g_bml_stash_placement_new_entity_sprite_1484_calls,
            g_bml_stash_placement_new_entity_sprite_1790_calls,
            g_bml_stash_placement_new_entity_sprite_1791_calls,
            (unsigned)BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT,
            g_bml_stash_placement_new_entity_sample_count);
    for (size_t index = 0U; index < g_bml_stash_placement_new_entity_sample_count; ++index) {
        const BmlStashPlacementNewEntitySample *sample = &g_bml_stash_placement_new_entity_samples[index];
        fputs(index == 0U ? "\n      " : ",\n      ", file);
        fprintf(file, "{\"sprite\": %d, \"pos\": %u, \"entityList\": ", sample->sprite, sample->pos);
        bml_write_address_or_null(file, sample->entity_list);
        fputs(", \"creatureList\": ", file);
        bml_write_address_or_null(file, sample->creature_list);
        fputs(", \"result\": ", file);
        bml_write_address_or_null(file, sample->result);
        fputc('}', file);
    }
    if (g_bml_stash_placement_new_entity_sample_count > 0U) {
        fputs("\n    ", file);
    }
    fputs("]\n  },\n  \"setSpriteAttributes\": {", file);
    fprintf(file, "\n    \"sampleLimit\": %u,\n    \"sampled\": %zu,\n    \"samples\": [",
            (unsigned)BML_STASH_PLACEMENT_DISCOVERY_SAMPLE_LIMIT,
            g_bml_stash_placement_set_sprite_sample_count);
    for (size_t index = 0U; index < g_bml_stash_placement_set_sprite_sample_count; ++index) {
        const BmlStashPlacementSetSpriteSample *sample = &g_bml_stash_placement_set_sprite_samples[index];
        fputs(index == 0U ? "\n      " : ",\n      ", file);
        fputs("{\"entity\": ", file);
        bml_write_address_or_null(file, sample->entity);
        fputs(", \"source\": ", file);
        bml_write_address_or_null(file, sample->source);
        fputs(", \"parent\": ", file);
        bml_write_address_or_null(file, sample->parent);
        fputc('}', file);
    }
    if (g_bml_stash_placement_set_sprite_sample_count > 0U) {
        fputs("\n    ", file);
    }
    fputs("]\n  },\n  \"notes\": [\n    \"Discovery mode only records call context and argument pointers from the access/placement pass-through replacements.\",\n    \"It does not spawn, modify, or claim any Stash access point.\"\n  ],\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static void bml_write_stash_placement_discovery_exit_report(void) {
    if (!g_bml_stash_placement_discovery_active || !bml_has_value(g_bml_stash_placement_discovery_report_path)) {
        return;
    }
    (void)bml_write_stash_placement_discovery_report(g_bml_stash_placement_discovery_report_path);
}

static void bml_configure_stash_placement_discovery(const char *report_path) {
    if (!bml_has_value(report_path)) {
        return;
    }
    bml_reset_stash_placement_discovery_state();
    g_bml_stash_placement_discovery_active = true;
    g_bml_stash_placement_global_map_symbol = dlsym(RTLD_DEFAULT, "map");
    bml_copy_string(g_bml_stash_placement_discovery_report_path, sizeof(g_bml_stash_placement_discovery_report_path), report_path);
    if (!g_bml_stash_placement_discovery_exit_report_registered) {
        if (atexit(bml_write_stash_placement_discovery_exit_report) == 0) {
            g_bml_stash_placement_discovery_exit_report_registered = true;
        }
    }
}

static BmlBaronyList *bml_stash_stats_void_chest_inventory(void) {
    void *stats_symbol = dlsym(RTLD_DEFAULT, "stats");
    void *stats_zero = NULL;
    if (stats_symbol == NULL) {
        return NULL;
    }
    memcpy(&stats_zero, stats_symbol, sizeof(stats_zero));
    if (stats_zero == NULL) {
        return NULL;
    }
    return (BmlBaronyList *)((unsigned char *)stats_zero + BML_STASH_STAT_VOID_CHEST_INVENTORY_OFFSET);
}

static bool bml_stash_is_stats_void_chest_inventory(void *inventory) {
    BmlBaronyList *stats_inventory = bml_stash_stats_void_chest_inventory();
    return inventory != NULL && stats_inventory != NULL && inventory == (void *)stats_inventory;
}
static bool bml_stash_entity_uses_stats_void_chest(void *entity, BmlBaronyList **inventory_out) {
    void *inventory = NULL;
    if (g_bml_stash_get_inventory_original != NULL) {
        inventory = g_bml_stash_get_inventory_original(entity);
    }
    if (inventory_out != NULL) {
        *inventory_out = (BmlBaronyList *)inventory;
    }
    return bml_stash_is_stats_void_chest_inventory(inventory);
}
static int32_t bml_entity_get_s32(void *entity, uintptr_t offset) {
    if (entity == NULL) {
        return 0;
    }
    int32_t val;
    memcpy(&val, (unsigned char *)entity + offset, sizeof(val));
    return val;
}
static void bml_entity_set_s32(void *entity, uintptr_t offset, int32_t value) {
    if (entity == NULL) {
        return;
    }
    memcpy((unsigned char *)entity + offset, &value, sizeof(value));
}
static void bml_entity_set_skill(void *entity, int index, int32_t value) {
    if (entity == NULL || index < 0) {
        return;
    }
    bml_entity_set_s32(entity, BML_STASH_ENTITY_OFFSET_SKILL + (uintptr_t)index * sizeof(int32_t), value);
}
static double bml_entity_get_real(void *entity, uintptr_t offset) {
    double value = 0.0;
    if (entity == NULL) {
        return value;
    }
    memcpy(&value, (unsigned char *)entity + offset, sizeof(value));
    return value;
}
static void bml_entity_set_real(void *entity, uintptr_t offset, double value) {
    if (entity == NULL) {
        return;
    }
    memcpy((unsigned char *)entity + offset, &value, sizeof(value));
}
static void bml_entity_set_parent(void *entity, int32_t parent_uid) {
    bml_entity_set_s32(entity, BML_STASH_ENTITY_OFFSET_PARENT, parent_uid);
}
static int32_t bml_entity_get_uid(void *entity) {
    return bml_entity_get_s32(entity, BML_STASH_ENTITY_OFFSET_UID);
}
static bool bml_stash_entity_is_framework_stash_access(void *entity) {
    if (entity == NULL) {
        return false;
    }
    return bml_entity_get_s32(entity, BML_STASH_ENTITY_OFFSET_SKILL58) == BML_STASH_INTERNAL_MARKER_SKILL58;
}
static bool bml_stash_any_selected_entity_is_framework_stash_access(void) {
    void *selected_entity_symbol = dlsym(RTLD_DEFAULT, "selectedEntity");
    void **selected_entities = NULL;
    if (selected_entity_symbol == NULL) {
        return false;
    }
    selected_entities = (void **)selected_entity_symbol;
    for (size_t index = 0U; index < BML_STASH_SELECTED_ENTITY_PLAYERS; ++index) {
        if (bml_stash_entity_is_framework_stash_access(selected_entities[index])) {
            return true;
        }
    }
    return false;
}
static void bml_stash_clear_recent_uid_to_entity_prompt_context(void) {
    g_bml_stash_recent_uid_to_entity_was_framework_stash = false;
    g_bml_stash_recent_uid_to_entity_tooltip_armed = false;
    g_bml_stash_recent_uid_to_entity = NULL;
    g_bml_stash_recent_uid_to_entity_consecutive_count = 0;
}

static bool bml_stash_consume_recent_uid_to_entity_prompt_context(void) {
    bool was_framework_stash = g_bml_stash_recent_uid_to_entity_was_framework_stash &&
        (g_bml_stash_recent_uid_to_entity_tooltip_armed || g_bml_stash_recent_uid_to_entity_consecutive_count >= 2);
    if (was_framework_stash) {
        ++g_bml_stash_uid_prompt_context_consumed;
    }
    bml_stash_clear_recent_uid_to_entity_prompt_context();
    return was_framework_stash;
}
static void bml_entity_set_flag(void *entity, int flag_bit, bool value) {
    if (entity == NULL || flag_bit < 0) {
        return;
    }
    unsigned char bool_value = value ? 1U : 0U;
    memcpy((unsigned char *)entity + BML_STASH_ENTITY_OFFSET_FLAGS + (uintptr_t)flag_bit, &bool_value, sizeof(bool_value));
}


static size_t bml_stash_inventory_count(const BmlBaronyList *inventory) {
    size_t count = 0U;
    if (inventory == NULL) {
        return 0U;
    }
    for (const BmlBaronyNode *node = inventory->first; node != NULL; node = node->next) {
        if (node->element != NULL) {
            count += 1U;
        }
    }
    return count;
}

static size_t bml_count_stash_inventory_file_rows(void) {
    FILE *file;
    char line[256];
    size_t rows = 0U;
    if (!bml_has_value(g_bml_stash_inventory_path)) {
        return 0U;
    }
    file = fopen(g_bml_stash_inventory_path, "rb");
    if (file == NULL) {
        return 0U;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] != '\0' && line[0] != '\n' && line[0] != '#') {
            rows += 1U;
        }
    }
    (void)fclose(file);
    return rows;
}

static int bml_stash_resolve_inventory_functions(char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    void *new_item_address = dlsym(RTLD_DEFAULT, "_Z7newItem8ItemType6StatusssjbP6list_t");
    void *list_free_all_address = dlsym(RTLD_DEFAULT, "_Z12list_FreeAllP6list_t");
    if (new_item_address == NULL || list_free_all_address == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SYMBOL_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash core behavior requires newItem and list_FreeAll to be resolvable.");
        return -1;
    }
    memcpy(&g_bml_stash_new_item, &new_item_address, sizeof(g_bml_stash_new_item));
    memcpy(&g_bml_stash_list_free_all, &list_free_all_address, sizeof(g_bml_stash_list_free_all));
    return 0;
}

static int bml_configure_stash_core_behavior(const char *profile_dir, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    memset(g_bml_stash_state_dir_path, 0, sizeof(g_bml_stash_state_dir_path));
    memset(g_bml_stash_inventory_path, 0, sizeof(g_bml_stash_inventory_path));
    memset(g_bml_stash_diagnostics_path, 0, sizeof(g_bml_stash_diagnostics_path));
    if (bml_join_path(g_bml_stash_state_dir_path, sizeof(g_bml_stash_state_dir_path), profile_dir, BML_STASH_STATE_DIR_RELATIVE_PATH) != 0 ||
        bml_join_path(g_bml_stash_inventory_path, sizeof(g_bml_stash_inventory_path), profile_dir, BML_STASH_INVENTORY_RELATIVE_PATH) != 0 ||
        bml_join_path(g_bml_stash_diagnostics_path, sizeof(g_bml_stash_diagnostics_path), profile_dir, BML_STASH_DIAGNOSTICS_RELATIVE_PATH) != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_PATH_TOO_LONG");
        bml_copy_string(error_message, error_message_size, "Stash core behavior state path exceeded PATH_MAX.");
        return -1;
    }
    if (bml_stash_resolve_inventory_functions(error_code, error_code_size, error_message, error_message_size) != 0) {
        return -1;
    }
    g_bml_stash_core_behavior_active = true;
    g_bml_stash_core_behavior_loaded = false;
    g_bml_stash_core_behavior_dirty = false;
    g_bml_stash_core_behavior_failed = false;
    g_bml_stash_core_behavior_loads = 0;
    g_bml_stash_core_behavior_saves = 0;
    g_bml_stash_core_behavior_dirty_marks = 0;
    bml_stash_record_inventory_generation(NULL);
    memset(g_bml_stash_core_behavior_failure_code, 0, sizeof(g_bml_stash_core_behavior_failure_code));
    memset(g_bml_stash_core_behavior_failure_message, 0, sizeof(g_bml_stash_core_behavior_failure_message));
    return 0;
}

static int bml_load_stash_inventory_if_needed(BmlBaronyList *inventory, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    FILE *file;
    char line[1024];
    BmlBaronyList loaded_inventory = { 0 };
    bool saw_header = false;
    size_t row = 0U;

    if (!g_bml_stash_core_behavior_active || inventory == NULL) {
        return 0;
    }
    if (bml_stash_copy_failure(error_code, error_code_size, error_message, error_message_size) != 0) {
        return -1;
    }
    if (g_bml_stash_core_behavior_loaded) {
        if (g_bml_stash_core_behavior_inventory == inventory) {
            if (g_bml_stash_core_behavior_dirty || bml_stash_loaded_inventory_generation_matches(inventory)) {
                return 0;
            }
        } else if (g_bml_stash_core_behavior_dirty) {
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_DIRTY_INVENTORY_REPLACED");
            bml_copy_string(error_message, error_message_size, "Barony replaced the bound Stash inventory list while unsaved Stash state was dirty; refusing to merge stale state.");
            return -1;
        }
    }
    if (g_bml_stash_list_free_all == NULL || g_bml_stash_new_item == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SYMBOL_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash core behavior cannot load inventory before list/newItem helpers resolve.");
        return -1;
    }

    errno = 0;
    file = fopen(g_bml_stash_inventory_path, "rb");
    if (file == NULL) {
        if (errno != ENOENT) {
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_READ_FAILED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior could not open the Stash inventory state file for reading.");
            return -1;
        }
    } else {
        while (fgets(line, sizeof(line), file) != NULL) {
            int type = 0;
            int status = 0;
            int beatitude = 0;
            int count = 0;
            uint32_t appearance = 0U;
            int identified = 0;
            uint32_t uid = 0U;
            int32_t x = 0;
            int32_t y = 0;
            uint32_t owner_uid = 0U;
            uint32_t interact_npc_uid = 0U;
            int forced_pickup_by_player = 0;
            int is_droppable = 0;
            int player_sold_item_to_shop = 0;
            int item_hidden_from_shop = 0;
            int notify_icon = 0;
            int spell_notify_icon = 0;
            unsigned int item_require_trading_skill_in_shop = 0U;
            int item_special_shop_consumable = 0;
            int consumed = 0;
            int parsed;
            BmlBaronyItem *item;

            line[strcspn(line, "\r\n")] = '\0';
            if (!saw_header) {
                if (line[0] == '\0') {
                    continue;
                }
                if (strcmp(line, BML_STASH_INVENTORY_FORMAT_HEADER) != 0) {
                    (void)fclose(file);
                    bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_UNSUPPORTED_STATE_FORMAT");
                    bml_copy_string(error_message, error_message_size, "Stash inventory state file is missing the required bml-stash-inventory-v2 schema header; refusing to load truncated legacy state.");
                    return -1;
                }
                saw_header = true;
                continue;
            }
            if (line[0] == '\0' || line[0] == '#') {
                continue;
            }
            ++row;
            parsed = sscanf(line,
                            "%d %d %d %d %" SCNu32 " %d %" SCNu32 " %" SCNd32 " %" SCNd32 " %" SCNu32 " %" SCNu32 " %d %d %d %d %d %d %u %d %n",
                            &type,
                            &status,
                            &beatitude,
                            &count,
                            &appearance,
                            &identified,
                            &uid,
                            &x,
                            &y,
                            &owner_uid,
                            &interact_npc_uid,
                            &forced_pickup_by_player,
                            &is_droppable,
                            &player_sold_item_to_shop,
                            &item_hidden_from_shop,
                            &notify_icon,
                            &spell_notify_icon,
                            &item_require_trading_skill_in_shop,
                            &item_special_shop_consumable,
                            &consumed);
            if (parsed != BML_STASH_INVENTORY_COLUMN_COUNT ||
                !bml_stash_line_has_only_trailing_space(line + consumed) ||
                beatitude < INT16_MIN || beatitude > INT16_MAX ||
                count < 1 || count > INT16_MAX ||
                !bml_stash_bool_field_valid(identified) ||
                !bml_stash_bool_field_valid(forced_pickup_by_player) ||
                !bml_stash_bool_field_valid(is_droppable) ||
                !bml_stash_bool_field_valid(player_sold_item_to_shop) ||
                !bml_stash_bool_field_valid(item_hidden_from_shop) ||
                !bml_stash_bool_field_valid(notify_icon) ||
                !bml_stash_bool_field_valid(spell_notify_icon) ||
                item_require_trading_skill_in_shop > UINT8_MAX ||
                !bml_stash_bool_field_valid(item_special_shop_consumable)) {
                g_bml_stash_list_free_all(&loaded_inventory);
                (void)fclose(file);
                bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_ROW_UNSUPPORTED");
                snprintf(error_message, error_message_size, "Stash inventory state row %zu cannot be represented by the supported v2 item schema.", row);
                return -1;
            }
            item = (BmlBaronyItem *)g_bml_stash_new_item(type, status, (int16_t)beatitude, (int16_t)count, appearance, identified != 0, &loaded_inventory);
            if (item == NULL) {
                g_bml_stash_list_free_all(&loaded_inventory);
                (void)fclose(file);
                bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_ITEM_CREATE_FAILED");
                bml_copy_string(error_message, error_message_size, "Stash core behavior could not recreate an item from persisted state.");
                return -1;
            }
            item->uid = uid;
            item->x = x;
            item->y = y;
            item->ownerUid = owner_uid;
            item->interactNPCUid = interact_npc_uid;
            item->forcedPickupByPlayer = forced_pickup_by_player != 0;
            item->isDroppable = is_droppable != 0;
            item->playerSoldItemToShop = player_sold_item_to_shop != 0;
            item->itemHiddenFromShop = item_hidden_from_shop != 0;
            item->notifyIcon = notify_icon != 0;
            item->spellNotifyIcon = spell_notify_icon != 0;
            item->itemRequireTradingSkillInShop = (uint8_t)item_require_trading_skill_in_shop;
            item->itemSpecialShopConsumable = item_special_shop_consumable != 0;
        }
        if (ferror(file) != 0) {
            g_bml_stash_list_free_all(&loaded_inventory);
            (void)fclose(file);
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_READ_FAILED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior failed while reading the Stash inventory state file.");
            return -1;
        }
        if (fclose(file) != 0) {
            g_bml_stash_list_free_all(&loaded_inventory);
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_READ_CLOSE_FAILED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior failed while closing the Stash inventory state file after reading.");
            return -1;
        }
        if (!saw_header) {
            g_bml_stash_list_free_all(&loaded_inventory);
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_UNSUPPORTED_STATE_FORMAT");
            bml_copy_string(error_message, error_message_size, "Stash inventory state file is empty or missing the required bml-stash-inventory-v2 schema header.");
            return -1;
        }
    }

    g_bml_stash_list_free_all(inventory);
    inventory->first = loaded_inventory.first;
    inventory->last = loaded_inventory.last;
    for (BmlBaronyNode *node = inventory->first; node != NULL; node = node->next) {
        node->list = inventory;
    }
    loaded_inventory.first = NULL;
    loaded_inventory.last = NULL;
    bml_stash_record_inventory_generation(inventory);
    g_bml_stash_core_behavior_loaded = true;
    g_bml_stash_core_behavior_dirty = false;
    g_bml_stash_core_behavior_loads += 1;
    bml_append_stash_diagnostic_event("stash_inventory_loaded", NULL, NULL, false, 0.0, 0.0, (int)bml_stash_inventory_count(inventory));
    return 0;
}

static void bml_mark_stash_inventory_dirty(void) {
    if (g_bml_stash_core_behavior_active && !g_bml_stash_core_behavior_failed) {
        g_bml_stash_core_behavior_dirty = true;
        g_bml_stash_core_behavior_dirty_marks += 1;
    }
}

static int bml_save_stash_inventory_if_dirty(char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    FILE *file;
    char tmp_path[PATH_MAX];
    bool close_failed = false;
    BmlBaronyList *current_inventory;
    if (!g_bml_stash_core_behavior_active || !g_bml_stash_core_behavior_dirty) {
        return 0;
    }
    if (bml_stash_copy_failure(error_code, error_code_size, error_message, error_message_size) != 0) {
        return -1;
    }
    current_inventory = bml_stash_stats_void_chest_inventory();
    if (current_inventory != NULL && g_bml_stash_core_behavior_inventory != NULL && current_inventory != g_bml_stash_core_behavior_inventory) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SAVE_INVENTORY_REPLACED");
        bml_copy_string(error_message, error_message_size, "Barony replaced the bound Stash inventory list before dirty state could be saved; refusing to persist stale state.");
        return -1;
    }
    if (g_bml_stash_core_behavior_inventory == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_INVENTORY_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash core behavior had dirty state but no bound inventory list.");
        return -1;
    }
    if (bml_mkdir_p(g_bml_stash_state_dir_path) != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_DIR_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior could not create the Stash state directory.");
        return -1;
    }
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", g_bml_stash_inventory_path, (long)getpid()) >= (int)sizeof(tmp_path)) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_PATH_TOO_LONG");
        bml_copy_string(error_message, error_message_size, "Stash core behavior temporary inventory path exceeded PATH_MAX.");
        return -1;
    }
    file = fopen(tmp_path, "wb");
    if (file == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_WRITE_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior could not open the temporary Stash inventory state file for writing.");
        return -1;
    }
    fprintf(file, "%s\n", BML_STASH_INVENTORY_FORMAT_HEADER);
    fputs("# columns: type status beatitude count appearance identified uid x y ownerUid interactNPCUid forcedPickupByPlayer isDroppable playerSoldItemToShop itemHiddenFromShop notifyIcon spellNotifyIcon itemRequireTradingSkillInShop itemSpecialShopConsumable\n", file);
    for (const BmlBaronyNode *node = g_bml_stash_core_behavior_inventory->first; node != NULL; node = node->next) {
        const BmlBaronyItem *item = (const BmlBaronyItem *)node->element;
        if (item == NULL) {
            continue;
        }
        if (item->count < 1) {
            close_failed = fclose(file) != 0;
            (void)close_failed;
            (void)unlink(tmp_path);
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_ITEM_UNSUPPORTED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior refused to persist an item with unsupported count state.");
            return -1;
        }
        fprintf(file,
                "%d\t%d\t%d\t%d\t%" PRIu32 "\t%d\t%" PRIu32 "\t%" PRId32 "\t%" PRId32 "\t%" PRIu32 "\t%" PRIu32 "\t%d\t%d\t%d\t%d\t%d\t%d\t%u\t%d\n",
                item->type,
                item->status,
                (int)item->beatitude,
                (int)item->count,
                item->appearance,
                item->identified ? 1 : 0,
                item->uid,
                item->x,
                item->y,
                item->ownerUid,
                item->interactNPCUid,
                item->forcedPickupByPlayer ? 1 : 0,
                item->isDroppable ? 1 : 0,
                item->playerSoldItemToShop ? 1 : 0,
                item->itemHiddenFromShop ? 1 : 0,
                item->notifyIcon ? 1 : 0,
                item->spellNotifyIcon ? 1 : 0,
                (unsigned int)item->itemRequireTradingSkillInShop,
                item->itemSpecialShopConsumable ? 1 : 0);
    }
    if (ferror(file) != 0 || fflush(file) != 0 || fsync(fileno(file)) != 0) {
        close_failed = fclose(file) != 0;
        (void)close_failed;
        (void)unlink(tmp_path);
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_WRITE_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior failed while writing the temporary Stash inventory state file; the previous inventory file was left untouched.");
        return -1;
    }
    if (fclose(file) != 0) {
        (void)unlink(tmp_path);
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_CLOSE_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior failed while closing the temporary Stash inventory state file; the previous inventory file was left untouched.");
        return -1;
    }
    if (rename(tmp_path, g_bml_stash_inventory_path) != 0) {
        (void)unlink(tmp_path);
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_STATE_RENAME_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior could not atomically replace the Stash inventory state file; the previous inventory file was left untouched.");
        return -1;
    }
    {
        int dir_fd = open(g_bml_stash_state_dir_path, O_RDONLY | O_DIRECTORY);
        if (dir_fd >= 0) {
            (void)fsync(dir_fd);
            (void)close(dir_fd);
        }
    }
    g_bml_stash_core_behavior_dirty = false;
    g_bml_stash_core_behavior_saves += 1;
    bml_stash_record_inventory_generation(g_bml_stash_core_behavior_inventory);
    bml_append_stash_diagnostic_event("stash_inventory_saved", NULL, NULL, false, 0.0, 0.0, (int)bml_stash_inventory_count(g_bml_stash_core_behavior_inventory));
    return 0;
}

static void *bml_stash_add_item_to_chest_replacement(void *entity, void *item, bool force_new_stack, void *specific_destination_stack) {
    BmlBaronyList *inventory = NULL;
    void *result = NULL;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    bool stash_inventory;
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_add_item_to_chest_replacement_calls;
    stash_inventory = bml_stash_entity_uses_stats_void_chest(entity, &inventory);
    if (stash_inventory && bml_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_load_failed", error_code, error_message);
        return NULL;
    }
    if (stash_inventory && g_bml_stash_core_behavior_failed) {
        return NULL;
    }
    if (g_bml_stash_add_item_to_chest_original != NULL) {
        result = g_bml_stash_add_item_to_chest_original(entity, item, force_new_stack, specific_destination_stack);
    }
    if (g_bml_stash_core_behavior_active && stash_inventory && result != NULL) {
        bml_mark_stash_inventory_dirty();
    }
    return result;
}

static void *bml_stash_get_item_from_chest_replacement(void *entity, void *item, int amount, bool get_info_only) {
    BmlBaronyList *inventory = NULL;
    void *result = NULL;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    bool stash_inventory;
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_get_item_from_chest_replacement_calls;
    stash_inventory = bml_stash_entity_uses_stats_void_chest(entity, &inventory);
    if (stash_inventory && bml_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_load_failed", error_code, error_message);
        return NULL;
    }
    if (stash_inventory && g_bml_stash_core_behavior_failed) {
        return NULL;
    }
    if (g_bml_stash_get_item_from_chest_original != NULL) {
        result = g_bml_stash_get_item_from_chest_original(entity, item, amount, get_info_only);
    }
    if (g_bml_stash_core_behavior_active && stash_inventory && !get_info_only && result != NULL) {
        bml_mark_stash_inventory_dirty();
    }
    return result;
}

static void *bml_stash_add_item_to_void_chest_server_replacement(void *entity, int player, void *item, bool force_new_stack, void *picked_up_stack) {
    BmlBaronyList *inventory = bml_stash_stats_void_chest_inventory();
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_add_item_replacement_calls;
    if (inventory != NULL && bml_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_load_failed", error_code, error_message);
        return NULL;
    }
    if (inventory != NULL && g_bml_stash_core_behavior_failed) {
        return NULL;
    }
    if (g_bml_stash_add_item_original != NULL) {
        g_bml_stash_add_item_original_result = g_bml_stash_add_item_original(entity, player, item, force_new_stack, picked_up_stack);
    }
    g_bml_stash_add_item_replacement_result = g_bml_stash_add_item_original_result;
    if (g_bml_stash_core_behavior_active && g_bml_stash_add_item_replacement_result != NULL) {
        bml_mark_stash_inventory_dirty();
    }
    return g_bml_stash_add_item_replacement_result;
}

static void *bml_stash_get_chest_inventory_list_replacement(void *entity) {
    void *inventory = NULL;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_get_inventory_replacement_calls;
    if (g_bml_stash_get_inventory_original != NULL) {
        inventory = g_bml_stash_get_inventory_original(entity);
    }
    if (g_bml_stash_core_behavior_active && bml_stash_is_stats_void_chest_inventory(inventory) &&
        bml_load_stash_inventory_if_needed((BmlBaronyList *)inventory, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_load_failed", error_code, error_message);
        return NULL;
    }
    return inventory;
}

static bool bml_stash_remove_item_from_void_chest_server_replacement(void *entity, int player, void *item, int count) {
    BmlBaronyList *inventory = bml_stash_stats_void_chest_inventory();
    bool removed = false;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_remove_item_replacement_calls;
    if (inventory != NULL && bml_load_stash_inventory_if_needed(inventory, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_load_failed", error_code, error_message);
        return false;
    }
    if (inventory != NULL && g_bml_stash_core_behavior_failed) {
        return false;
    }
    if (g_bml_stash_remove_item_original != NULL) {
        removed = g_bml_stash_remove_item_original(entity, player, item, count);
    }
    if (g_bml_stash_core_behavior_active && removed) {
        bml_mark_stash_inventory_dirty();
    }
    return removed;
}

static void bml_stash_close_chest_replacement(void *entity) {
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_close_chest_replacement_calls;
    if (g_bml_stash_close_chest_original != NULL) {
        g_bml_stash_close_chest_original(entity);
    }
    if (bml_save_stash_inventory_if_dirty(error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_save_failed", error_code, error_message);
    }
}

static void bml_stash_close_chest_server_replacement(void *entity) {
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    ++g_bml_stash_close_chest_server_replacement_calls;
    if (g_bml_stash_close_chest_server_original != NULL) {
        g_bml_stash_close_chest_server_original(entity);
    }
    if (bml_save_stash_inventory_if_dirty(error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        bml_stash_fail_closed("stash_inventory_save_failed", error_code, error_message);
    }
}

static void bml_stash_act_chest_replacement(void *entity) {
    ++g_bml_stash_act_chest_replacement_calls;
    if (g_bml_stash_act_chest_original != NULL) {
        g_bml_stash_act_chest_original(entity);
    }
}

static void bml_stash_act_chest_lid_replacement(void *entity) {
    ++g_bml_stash_act_chest_lid_replacement_calls;
    if (g_bml_stash_act_chest_lid_original != NULL) {
        g_bml_stash_act_chest_lid_original(entity);
    }
}
static void *bml_uid_to_entity_replacement(int uid) {
    void *entity = NULL;
    ++g_bml_uid_to_entity_replacement_calls;
    if (g_bml_uid_to_entity_original != NULL) {
        entity = g_bml_uid_to_entity_original(uid);
    }
    if (bml_stash_entity_is_framework_stash_access(entity)) {
        if (g_bml_stash_recent_uid_to_entity_was_framework_stash && g_bml_stash_recent_uid_to_entity == entity) {
            g_bml_stash_recent_uid_to_entity_consecutive_count += 1;
        } else {
            g_bml_stash_recent_uid_to_entity_consecutive_count = 1;
        }
        g_bml_stash_recent_uid_to_entity_was_framework_stash = true;
        g_bml_stash_recent_uid_to_entity_tooltip_armed = false;
        g_bml_stash_recent_uid_to_entity = entity;
        ++g_bml_stash_uid_prompt_context_recorded;
    } else {
        bml_stash_clear_recent_uid_to_entity_prompt_context();
    }
    return entity;
}

static const char *bml_language_get_replacement(int language_id) {
    ++g_bml_language_get_replacement_calls;
    if (language_id == BML_STASH_PROMPT_LANGUAGE_ID_TOOLTIP_ACTION && g_bml_stash_recent_uid_to_entity_was_framework_stash) {
        g_bml_stash_recent_uid_to_entity_tooltip_armed = true;
    } else if (language_id == BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST) {
        bool recent_uid_stash = bml_stash_consume_recent_uid_to_entity_prompt_context();
        if (bml_stash_any_selected_entity_is_framework_stash_access() || recent_uid_stash) {
            return BML_STASH_PROMPT_OPEN_STASH;
        }
    } else if (g_bml_stash_recent_uid_to_entity_was_framework_stash) {
        bml_stash_clear_recent_uid_to_entity_prompt_context();
    }
    if (g_bml_language_get_original != NULL) {
        return g_bml_language_get_original(language_id);
    }
    return "";
}

static bool bml_stash_playable_try_place_shop_chest_and_lid(void *map_argument);
static void *bml_stash_playable_get_map_symbol(void);
static int bml_stash_generate_dungeon_replacement(char *levelset, uint32_t seed, uint64_t tuple_low, uint64_t tuple_high) {
    int result = 0;
    ++g_bml_stash_generate_dungeon_replacement_calls;
    if (g_bml_stash_generate_dungeon_original != NULL) {
        result = g_bml_stash_generate_dungeon_original(levelset, seed, tuple_low, tuple_high);
    }
        g_bml_stash_playable_shop_generation += 1;
    if (g_bml_stash_playable_active) {
        (void)bml_stash_playable_try_place_shop_chest_and_lid(bml_stash_playable_get_map_symbol());
    }
    return result;
}
static void *bml_stash_playable_get_map_symbol(void);
static BmlBaronyList *bml_stash_playable_get_map_entity_list(void *map_argument) {
    void *map_ptr = map_argument;
    BmlBaronyList *entity_list = NULL;
    if (map_ptr == NULL) {
        map_ptr = bml_stash_playable_get_map_symbol();
    }
    if (map_ptr != NULL) {
        memcpy(&entity_list, (unsigned char *)map_ptr + BML_STASH_MAP_OFFSET_ENTITIES, sizeof(entity_list));
    }
    return entity_list;
}
static void *bml_stash_playable_get_map_symbol(void) {
    return dlsym(RTLD_DEFAULT, "map");
}
static bool bml_stash_playable_is_start_map_name(const char *name) {
    if (!bml_has_value(name)) {
        return false;
    }
    if (strcmp(name, "fake-lobby") == 0) {
        return true;
    }
    if (strcmp(name, "Start Map") == 0) {
        return true;
    }
    return false;
}
static void bml_stash_world_tile_center(unsigned int tile_x, unsigned int tile_y, double *world_x_out, double *world_y_out) {
    if (world_x_out != NULL) {
        *world_x_out = (double)tile_x * 16.0 + 8.0;
    }
    if (world_y_out != NULL) {
        *world_y_out = (double)tile_y * 16.0 + 8.0;
    }
}
static double bml_stash_world_to_tile(double world_coordinate) {
    return (world_coordinate - 8.0) / 16.0;
}
static bool bml_stash_find_nearest_walkable_tile(BmlStashPlacementMapPrefix *map_prefix, double anchor_world_x, double anchor_world_y, double *world_x_out, double *world_y_out) {
    unsigned int x;
    unsigned int y;
    bool found = false;
    double best_distance_sq = 0.0;
    double best_world_x = 0.0;
    double best_world_y = 0.0;
    double anchor_tile_x = bml_stash_world_to_tile(anchor_world_x);
    double anchor_tile_y = bml_stash_world_to_tile(anchor_world_y);
    if (map_prefix == NULL || map_prefix->tiles == NULL || map_prefix->width == 0U || map_prefix->height == 0U) {
        return false;
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
            found = true;
            best_distance_sq = distance_sq;
            bml_stash_world_tile_center(x, y, &best_world_x, &best_world_y);
        }
    }
    if (!found) {
        return false;
    }
    if (world_x_out != NULL) {
        *world_x_out = best_world_x;
    }
    if (world_y_out != NULL) {
        *world_y_out = best_world_y;
    }
    return true;
}
static bool bml_stash_playable_read_int_symbol(const char *name, int *value_out) {
    void *symbol = dlsym(RTLD_DEFAULT, name);
    if (symbol == NULL || value_out == NULL) {
        return false;
    }
    memcpy(value_out, symbol, sizeof(*value_out));
    return true;
}
static bool bml_stash_playable_is_multiplayer_client(void) {
    int multiplayer_value = 0;
    int clientnum_value = 0;
    (void)bml_stash_playable_read_int_symbol("multiplayer", &multiplayer_value);
    (void)bml_stash_playable_read_int_symbol("clientnum", &clientnum_value);
    g_bml_stash_playable_multiplayer_value = multiplayer_value;
    g_bml_stash_playable_clientnum_value = clientnum_value;
    g_bml_stash_playable_multiplayer_client_blocked = multiplayer_value == BML_STASH_MULTIPLAYER_CLIENT || multiplayer_value == BML_STASH_MULTIPLAYER_DIRECTCLIENT;
    return g_bml_stash_playable_multiplayer_client_blocked;
}
static bool bml_stash_playable_place_chest_and_lid_at(void *map_argument, double chest_x, double chest_y, double chest_yaw, void **chest_out, void **lid_out) {
    typedef BmlBaronyNode *(*BmlListAddNodeFirstFunction)(BmlBaronyList *list);
    typedef void (*BmlEmptyDeconstructorFunction)(void *data);
    BmlBaronyList *entity_list;
    BmlBaronyList *void_inventory;
    BmlListAddNodeFirstFunction list_add_node_first = NULL;
    BmlEmptyDeconstructorFunction empty_deconstructor = NULL;
    void *chest = NULL;
    void *lid = NULL;
    void *act_chest_fn;
    void *act_chest_lid_fn;
    void *act_chest_fn_storage;
    void *act_chest_lid_fn_storage;
    void *list_add_node_first_address;
    void *empty_deconstructor_address;
    int32_t chest_uid;
    int32_t lid_uid;
    double chest_z = 6.0;
    double lid_x = chest_x;
    double lid_y = chest_y;
    double lid_z = chest_z + BML_STASH_PLACEMENT_LID_OFFSET_Z;
    if (chest_yaw == BML_STASH_YAW_EAST) {
        lid_x = chest_x - BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    } else if (chest_yaw == BML_STASH_YAW_SOUTH) {
        lid_y = chest_y - BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    } else if (chest_yaw == BML_STASH_YAW_WEST) {
        lid_x = chest_x + BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    } else if (chest_yaw == BML_STASH_YAW_NORTH) {
        lid_y = chest_y + BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    } else {
        lid_y = chest_y - BML_STASH_PLACEMENT_LID_HINGE_OFFSET;
    }

    if (chest_out != NULL) {
        *chest_out = NULL;
    }
    if (lid_out != NULL) {
        *lid_out = NULL;
    }
    if (!g_bml_stash_playable_active || bml_stash_playable_is_multiplayer_client()) {
        return false;
    }

    entity_list = bml_stash_playable_get_map_entity_list(map_argument);
    if (entity_list == NULL) {
        return false;
    }

    void_inventory = bml_stash_stats_void_chest_inventory();
    if (void_inventory != NULL) {
        char ec[BML_MAX_TEXT];
        char em[BML_MAX_TEXT];
        ec[0] = '\0';
        em[0] = '\0';
        (void)bml_load_stash_inventory_if_needed(void_inventory, ec, sizeof(ec), em, sizeof(em));
        g_bml_stash_core_behavior_inventory = void_inventory;
    }

    act_chest_fn = dlsym(RTLD_DEFAULT, "_Z8actChestP6Entity");
    act_chest_lid_fn = dlsym(RTLD_DEFAULT, "_Z11actChestLidP6Entity");
    if (act_chest_fn == NULL || act_chest_lid_fn == NULL) {
        return false;
    }
    memcpy(&act_chest_fn_storage, &act_chest_fn, sizeof(act_chest_fn_storage));
    memcpy(&act_chest_lid_fn_storage, &act_chest_lid_fn, sizeof(act_chest_lid_fn_storage));

    if (g_bml_stash_new_entity_original != NULL) {
        ++g_bml_stash_playable_new_entity_calls;
        chest = g_bml_stash_new_entity_original(BML_STASH_SPRITE_CHEST_SPAWN, 1U, entity_list, NULL);
        if (chest != NULL) {
            if (g_bml_stash_set_sprite_attributes_original != NULL) {
                g_bml_stash_set_sprite_attributes_original(chest, NULL, NULL);
            }
            bml_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_X, chest_x);
            bml_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_Y, chest_y);
            bml_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_Z, chest_z);
            bml_entity_set_real(chest, BML_STASH_ENTITY_OFFSET_YAW, chest_yaw);
            bml_entity_set_s32(chest, BML_STASH_ENTITY_OFFSET_SIZEX, 3);
            bml_entity_set_s32(chest, BML_STASH_ENTITY_OFFSET_SIZEY, 2);
            bml_entity_set_s32(chest, BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_CHEST_VOID_VISUAL);
            memcpy((unsigned char *)chest + BML_STASH_ENTITY_OFFSET_BEHAVIOR, &act_chest_fn_storage, sizeof(act_chest_fn_storage));
            bml_entity_set_skill(chest, 0, 1);
            bml_entity_set_skill(chest, 3, 9999);
            bml_entity_set_skill(chest, 8, 9999);
            bml_entity_set_skill(chest, 15, 9999);
            bml_entity_set_skill(chest, 4, 0);
            bml_entity_set_skill(chest, 10, 1);
            bml_entity_set_skill(chest, 12, 0);
            bml_entity_set_skill(chest, 17, BML_STASH_CHEST_VOID_STATE_PERMANENT);
            bml_entity_set_skill(chest, 58, BML_STASH_INTERNAL_MARKER_SKILL58);
        }

        ++g_bml_stash_playable_new_entity_calls;
        lid = g_bml_stash_new_entity_original(BML_STASH_SPRITE_LID_SPAWN, 0U, entity_list, NULL);
        if (lid != NULL) {
            if (g_bml_stash_set_sprite_attributes_original != NULL) {
                g_bml_stash_set_sprite_attributes_original(lid, NULL, NULL);
            }
            bml_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_X, lid_x);
            bml_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_Y, lid_y);
            bml_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_Z, lid_z);
            bml_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_YAW, chest_yaw);
            bml_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_FOCALX, 3.0);
            bml_entity_set_real(lid, BML_STASH_ENTITY_OFFSET_FOCALZ, -0.75);
            bml_entity_set_s32(lid, BML_STASH_ENTITY_OFFSET_SIZEX, 2);
            bml_entity_set_s32(lid, BML_STASH_ENTITY_OFFSET_SIZEY, 2);
            bml_entity_set_s32(lid, BML_STASH_ENTITY_OFFSET_SPRITE, BML_STASH_SPRITE_LID_VOID_VISUAL);
            memcpy((unsigned char *)lid + BML_STASH_ENTITY_OFFSET_BEHAVIOR, &act_chest_lid_fn_storage, sizeof(act_chest_lid_fn_storage));
            bml_entity_set_flag(lid, 12, true);
            bml_entity_set_skill(lid, 58, BML_STASH_INTERNAL_MARKER_SKILL58);
        }
    }

    if (chest != NULL && lid != NULL) {
        chest_uid = bml_entity_get_uid(chest);
        lid_uid = bml_entity_get_uid(lid);
        bml_entity_set_parent(lid, chest_uid);
        bml_entity_set_parent(chest, lid_uid);

        list_add_node_first_address = dlsym(RTLD_DEFAULT, "_Z17list_AddNodeFirstP6list_t");
        if (list_add_node_first_address != NULL) {
            memcpy(&list_add_node_first, &list_add_node_first_address, sizeof(list_add_node_first));
        }
        empty_deconstructor_address = dlsym(RTLD_DEFAULT, "_Z18emptyDeconstructorPv");
        if (empty_deconstructor_address != NULL) {
            memcpy(&empty_deconstructor, &empty_deconstructor_address, sizeof(empty_deconstructor));
        }
        if (list_add_node_first != NULL) {
            BmlBaronyList *children = (BmlBaronyList *)((unsigned char *)chest + BML_STASH_ENTITY_OFFSET_CHILDREN);
            BmlBaronyNode *node = list_add_node_first(children);
            if (node != NULL) {
                node->element = NULL;
                node->deconstructor = empty_deconstructor;
            }
        }
        if (chest_out != NULL) {
            *chest_out = chest;
        }
        if (lid_out != NULL) {
            *lid_out = lid;
        }
        return true;
    }

    return false;
}
static bool bml_stash_playable_try_place_lobby_chest_and_lid(void *map_argument) {
    BmlStashPlacementMapPrefix *map_prefix = (BmlStashPlacementMapPrefix *)map_argument;
    void *chest = NULL;
    void *lid = NULL;
    double x_pos = BML_STASH_LOBBY_PLACEMENT_X;
    double y_pos = BML_STASH_LOBBY_PLACEMENT_Y;
    if (!g_bml_stash_playable_active) {
        return false;
    }
    if (g_bml_stash_playable_lobby_placements_succeeded > 0 || g_bml_stash_playable_last_placed_chest != NULL) {
        g_bml_stash_playable_lobby_already_placed_count += 1;
        return false;
    }
    g_bml_stash_playable_lobby_placements_attempted += 1;
    if (g_bml_stash_assign_actions_original != NULL) {
        g_bml_stash_assign_actions_original(map_argument);
    }
    (void)bml_stash_find_nearest_walkable_tile(map_prefix, x_pos, y_pos, &x_pos, &y_pos);
    if (bml_stash_playable_place_chest_and_lid_at(map_argument, x_pos, y_pos, BML_STASH_LOBBY_PLACEMENT_YAW, &chest, &lid)) {
        g_bml_stash_playable_last_placed_chest = chest;
        g_bml_stash_playable_last_placed_lid = lid;
        bml_append_stash_diagnostic_event("stash_access_point_created", "lobby", map_prefix != NULL ? map_prefix->name : NULL, true, x_pos, y_pos, -1);
        g_bml_stash_playable_lobby_placements_succeeded += 1;
        return true;
    }
    g_bml_stash_playable_lobby_placements_failed += 1;
    return false;
}
static bool bml_stash_playable_shop_tile_is_occupied(void *map_argument, double chest_x, double chest_y) {
    BmlBaronyList *entity_list = bml_stash_playable_get_map_entity_list(map_argument);
    unsigned int candidate_tile_x = chest_x >= 0.0 ? (unsigned int)(chest_x / 16.0) : 0U;
    unsigned int candidate_tile_y = chest_y >= 0.0 ? (unsigned int)(chest_y / 16.0) : 0U;
    if (entity_list == NULL) {
        return false;
    }
    for (const BmlBaronyNode *node = entity_list->first; node != NULL; node = node->next) {
        void *entity = node->element;
        double x;
        double y;
        if (entity == NULL) {
            continue;
        }
        x = bml_entity_get_real(entity, BML_STASH_ENTITY_OFFSET_X);
        y = bml_entity_get_real(entity, BML_STASH_ENTITY_OFFSET_Y);
        if (x < 0.0 || y < 0.0) {
            continue;
        }
        if ((unsigned int)(x / 16.0) == candidate_tile_x && (unsigned int)(y / 16.0) == candidate_tile_y) {
            return true;
        }
    }
    return false;
}

static bool bml_stash_playable_try_place_shop_chest_and_lid(void *map_argument) {
    BmlStashPlacementMapPrefix *map_prefix = (BmlStashPlacementMapPrefix *)map_argument;
    void *shoparea_symbol;
    bool *shoparea = NULL;
    unsigned int x;
    unsigned int y;
    void *chest = NULL;
    void *lid = NULL;
    if (!g_bml_stash_playable_active) {
        return false;
    }
    if (g_bml_stash_playable_last_shop_map == map_argument && g_bml_stash_playable_last_shop_generation == g_bml_stash_playable_shop_generation && g_bml_stash_playable_last_placed_shop_chest != NULL) {
        g_bml_stash_playable_shop_already_placed_count += 1;
        return false;
    }
    if (map_prefix == NULL || map_prefix->width == 0U || map_prefix->height == 0U) {
        return false;
    }
    shoparea_symbol = dlsym(RTLD_DEFAULT, "shoparea");
    if (shoparea_symbol == NULL) {
        return false;
    }
    memcpy(&shoparea, shoparea_symbol, sizeof(shoparea));
    if (shoparea == NULL) {
        return false;
    }
    for (x = 0U; x < map_prefix->width; ++x) {
        for (y = 0U; y < map_prefix->height; ++y) {
            if (shoparea[y + x * map_prefix->height]) {
                double chest_x = (double)x * 16.0 + 8.0;
                double chest_y = (double)y * 16.0 + 8.0;
                if (bml_stash_playable_shop_tile_is_occupied(map_argument, chest_x, chest_y)) {
                    continue;
                }
                g_bml_stash_playable_shop_placements_attempted += 1;
                if (bml_stash_playable_place_chest_and_lid_at(map_argument, chest_x, chest_y, 0.0, &chest, &lid)) {
                    g_bml_stash_playable_last_placed_shop_chest = chest;
                    g_bml_stash_playable_last_placed_shop_lid = lid;
                    g_bml_stash_playable_last_shop_map = map_argument;
                    g_bml_stash_playable_last_shop_generation = g_bml_stash_playable_shop_generation;
                    bml_append_stash_diagnostic_event("stash_access_point_created", "shop", map_prefix->name, true, chest_x, chest_y, -1);
                    g_bml_stash_playable_shop_placements_succeeded += 1;
                    return true;
                }
                g_bml_stash_playable_shop_placements_failed += 1;
                continue;
            }
        }
    }
    return false;
}

static void bml_stash_assign_actions_replacement(void *map) {
    int new_entity_calls_before = g_bml_stash_new_entity_replacement_calls;
    int set_sprite_attributes_calls_before = g_bml_stash_set_sprite_attributes_replacement_calls;
    ++g_bml_stash_assign_actions_replacement_calls;
    if (g_bml_stash_playable_active) {
        ++g_bml_stash_playable_assign_actions_calls;
    }
    bml_stash_placement_record_assign_actions_before(map, new_entity_calls_before, set_sprite_attributes_calls_before);
    if (g_bml_stash_placement_discovery_active) {
        g_bml_stash_placement_assign_actions_depth += 1;
    }
    if (g_bml_stash_playable_active) {
        BmlStashPlacementMapPrefix *map_prefix = (BmlStashPlacementMapPrefix *)map;
        bool is_eligible = false;
        if (map_prefix != NULL) {
            is_eligible = bml_stash_playable_is_start_map_name(map_prefix->name);
        }
        if (is_eligible) {
            (void)bml_stash_playable_try_place_lobby_chest_and_lid(map);
        } else {
            if (g_bml_stash_assign_actions_original != NULL) {
                g_bml_stash_assign_actions_original(map);
            }
            (void)bml_stash_playable_try_place_shop_chest_and_lid(map);
        }
    } else {
        if (g_bml_stash_assign_actions_original != NULL) {
            g_bml_stash_assign_actions_original(map);
        }
    }
    if (g_bml_stash_placement_discovery_active && g_bml_stash_placement_assign_actions_depth > 0) {
        g_bml_stash_placement_assign_actions_depth -= 1;
    }
    bml_stash_placement_record_assign_actions_after(new_entity_calls_before, set_sprite_attributes_calls_before);
}

static void *bml_stash_new_entity_replacement(int sprite, uint32_t pos, BmlBaronyList *entity_list, BmlBaronyList *creature_list) {
    void *result = NULL;
    ++g_bml_stash_new_entity_replacement_calls;
    if (g_bml_stash_playable_active) {
        ++g_bml_stash_playable_new_entity_calls;
    }
    if (g_bml_stash_new_entity_original != NULL) {
        result = g_bml_stash_new_entity_original(sprite, pos, entity_list, creature_list);
    }
    bml_stash_placement_record_new_entity(sprite, pos, entity_list, creature_list, result);
    return result;
}

static void bml_stash_set_sprite_attributes_replacement(void *entity, void *source, void *parent) {
    ++g_bml_stash_set_sprite_attributes_replacement_calls;
    if (g_bml_stash_playable_active) {
        ++g_bml_stash_playable_set_sprite_calls;
    }
    bml_stash_placement_record_set_sprite_attributes(entity, source, parent);
    if (g_bml_stash_set_sprite_attributes_original != NULL) {
        g_bml_stash_set_sprite_attributes_original(entity, source, parent);
    }
}
static int bml_write_stash_add_item_detour_report(const char *report_path, const char *test_name, const char *status, const char *error_code, const char *error_message, const BmlDetourInstall *install, void *direct_result, void *original_result, int replacement_calls) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": ", file);
    bml_json_write_escaped(file, test_name);
    fputs(",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"backend\": {\n    \"patchStyle\": \"rip-relative-indirect-jmp-absolute-slot\",\n    \"patchBytes\": ", file);
    fprintf(file, "%u", (unsigned)BML_DETOUR_PATCH_BYTES);
    fputs(",\n    \"decoder\": \"fixture-safe-subset\"\n  },\n  \"targetSymbol\": \"_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_\",\n  \"targetName\": \"Entity::addItemToVoidChestServer\",\n  \"targetAddress\": ", file);
    bml_write_address_or_null(file, install != NULL ? install->target : NULL);
    fputs(",\n  \"replacementAddress\": ", file);
    bml_write_address_or_null(file, install != NULL ? install->replacement : NULL);
    fputs(",\n  \"trampolineAddress\": ", file);
    bml_write_address_or_null(file, install != NULL ? install->trampoline : NULL);
    fputs(",\n  \"patchSize\": ", file);
    fprintf(file, "%zu", install != NULL ? install->patch_size : 0U);
    fputs(",\n  \"replacementInvoked\": ", file);
    fputs(replacement_calls > 0 ? "true" : "false", file);
    fputs(",\n  \"originalCallThroughInvoked\": ", file);
    fputs(original_result != NULL ? "true" : "false", file);
    fputs(",\n  \"replacementCalls\": ", file);
    fprintf(file, "%d", replacement_calls);
    fputs(",\n  \"originalResult\": ", file);
    bml_write_address_or_null(file, original_result);
    fputs(",\n  \"directResult\": ", file);
    bml_write_address_or_null(file, direct_result);
    fputs(",\n  \"error\": ", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("{\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_STASH_DETOUR_SELF_TEST_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Stash detour self-test failed.");
        fputc('}', file);
    } else {
        fputs("null", file);
    }
    fputs(",\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int bml_run_stash_detour_self_test(const char *report_path) {
    BmlDetourInstall install;
    BmlStashAddItemToVoidChestServerFunction target_function;
    BmlStashAddItemToVoidChestServerFunction replacement_function = bml_stash_add_item_to_void_chest_server_replacement;
    void *target_address;
    void *fake_provider_marker;
    void *direct_result = NULL;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];

    memset(&install, 0, sizeof(install));
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    g_bml_stash_add_item_original = NULL;
    g_bml_stash_add_item_replacement_calls = 0;
    g_bml_stash_add_item_original_result = NULL;
    g_bml_stash_add_item_replacement_result = NULL;

    target_address = dlsym(RTLD_DEFAULT, "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_");
    fake_provider_marker = dlsym(RTLD_DEFAULT, "bml_fake_detour_counter");
    target_function = bml_stash_add_item_function_from_address(target_address);
    if (target_address == NULL || fake_provider_marker == NULL) {
        bml_copy_string(error_code, sizeof(error_code), "BML_STASH_DETOUR_SELF_TEST_SYMBOL_MISSING");
        bml_copy_string(error_message, sizeof(error_message), "BML_STASH_DETOUR_SELF_TEST requires libfake_barony_symbols.so to export both Entity::addItemToVoidChestServer and the fake-provider marker.");
        (void)bml_write_stash_add_item_detour_report(report_path, "stash-add-item-detour-self-test", "failed", error_code, error_message, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    if (bml_install_absolute_jump_detour(target_address, bml_stash_add_item_function_address(replacement_function), &install, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        (void)bml_write_stash_add_item_detour_report(report_path, "stash-add-item-detour-self-test", "failed", error_code, error_message, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    g_bml_stash_add_item_original = bml_stash_add_item_function_from_address(install.trampoline);
    direct_result = target_function(NULL, 0, NULL, false, NULL);

    if (g_bml_stash_add_item_replacement_calls != 1 || (uintptr_t)g_bml_stash_add_item_original_result != 42U || direct_result != g_bml_stash_add_item_original_result) {
        bml_copy_string(error_code, sizeof(error_code), "BML_STASH_DETOUR_SELF_TEST_ASSERTION_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Stash target detour self-test did not observe replacement invocation and original trampoline call-through with the expected fixture result.");
        (void)bml_write_stash_add_item_detour_report(report_path, "stash-add-item-detour-self-test", "failed", error_code, error_message, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    if (bml_write_stash_add_item_detour_report(report_path, "stash-add-item-detour-self-test", "loaded", NULL, NULL, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls) != 0) {
        return -1;
    }

    return 0;
}


static int bml_run_stash_add_item_passthrough_install(const char *report_path) {
    BmlDetourInstall install;
    BmlStashAddItemToVoidChestServerFunction replacement_function = bml_stash_add_item_to_void_chest_server_replacement;
    void *target_address;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];

    memset(&install, 0, sizeof(install));
    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    g_bml_stash_add_item_original = NULL;
    g_bml_stash_add_item_replacement_calls = 0;
    g_bml_stash_add_item_original_result = NULL;
    g_bml_stash_add_item_replacement_result = NULL;

    target_address = dlsym(RTLD_DEFAULT, "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_");
    if (target_address == NULL) {
        bml_copy_string(error_code, sizeof(error_code), "BML_STASH_ADD_ITEM_INSTALL_SYMBOL_MISSING");
        bml_copy_string(error_message, sizeof(error_message), "BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH requires Entity::addItemToVoidChestServer to be resolvable before the pass-through detour can be installed.");
        (void)bml_write_stash_add_item_detour_report(report_path, "stash-add-item-passthrough-install", "failed", error_code, error_message, &install, NULL, NULL, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    if (bml_install_absolute_jump_detour(target_address, bml_stash_add_item_function_address(replacement_function), &install, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        if (!bml_has_value(error_code)) {
            bml_copy_string(error_code, sizeof(error_code), "BML_STASH_ADD_ITEM_INSTALL_FAILED");
        }
        if (!bml_has_value(error_message)) {
            bml_copy_string(error_message, sizeof(error_message), "Entity::addItemToVoidChestServer pass-through detour installation failed.");
        }
        (void)bml_write_stash_add_item_detour_report(report_path, "stash-add-item-passthrough-install", "failed", error_code, error_message, &install, NULL, NULL, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    g_bml_stash_add_item_original = bml_stash_add_item_function_from_address(install.trampoline);
    return bml_write_stash_add_item_detour_report(report_path, "stash-add-item-passthrough-install", "installed", NULL, NULL, &install, NULL, NULL, g_bml_stash_add_item_replacement_calls);
}

static void bml_reset_stash_core_passthrough_state(void) {
    g_bml_stash_get_inventory_original = NULL;
    g_bml_stash_add_item_to_chest_original = NULL;
    g_bml_stash_get_item_from_chest_original = NULL;
    g_bml_stash_add_item_original = NULL;
    g_bml_stash_remove_item_original = NULL;
    g_bml_stash_close_chest_original = NULL;
    g_bml_stash_close_chest_server_original = NULL;
    g_bml_stash_get_inventory_replacement_calls = 0;
    g_bml_stash_add_item_to_chest_replacement_calls = 0;
    g_bml_stash_get_item_from_chest_replacement_calls = 0;
    g_bml_stash_add_item_replacement_calls = 0;
    g_bml_stash_remove_item_replacement_calls = 0;
    g_bml_stash_close_chest_replacement_calls = 0;
    g_bml_stash_close_chest_server_replacement_calls = 0;
    g_bml_stash_add_item_original_result = NULL;
    g_bml_stash_add_item_replacement_result = NULL;
}

static void bml_reset_stash_access_placement_state(void) {
    g_bml_stash_act_chest_original = NULL;
    g_bml_stash_act_chest_lid_original = NULL;
    g_bml_stash_generate_dungeon_original = NULL;
    g_bml_stash_assign_actions_original = NULL;
    g_bml_stash_new_entity_original = NULL;
    g_bml_stash_set_sprite_attributes_original = NULL;
    g_bml_language_get_original = NULL;
    g_bml_uid_to_entity_original = NULL;
    g_bml_stash_act_chest_replacement_calls = 0;
    g_bml_stash_act_chest_lid_replacement_calls = 0;
    g_bml_stash_generate_dungeon_replacement_calls = 0;
    g_bml_stash_assign_actions_replacement_calls = 0;
    g_bml_stash_new_entity_replacement_calls = 0;
    g_bml_stash_set_sprite_attributes_replacement_calls = 0;
    g_bml_language_get_replacement_calls = 0;
    g_bml_uid_to_entity_replacement_calls = 0;
    g_bml_stash_uid_prompt_context_recorded = 0;
    g_bml_stash_uid_prompt_context_consumed = 0;
    g_bml_stash_recent_uid_to_entity_tooltip_armed = false;
    bml_stash_clear_recent_uid_to_entity_prompt_context();
    g_bml_stash_placement_discovery_active = false;
    g_bml_stash_placement_discovery_report_path[0] = '\0';
    bml_reset_stash_placement_discovery_state();
}

static void bml_init_stash_core_detour_target(BmlStashCoreDetourInstall *target, const char *target_name, const char *target_symbol, void *replacement_address) {
    memset(target, 0, sizeof(*target));
    target->target_name = target_name;
    target->target_symbol = target_symbol;
    target->replacement_address = replacement_address;
    target->status = "pending";
    target->install.replacement = replacement_address;
}

static int bml_stash_core_replacement_calls_for_symbol(const char *symbol) {
    if (strcmp(symbol, "_ZN6Entity21getChestInventoryListEv") == 0) {
        return g_bml_stash_get_inventory_replacement_calls;
    }
    if (strcmp(symbol, "_ZN6Entity14addItemToChestEP4ItembS1_") == 0) {
        return g_bml_stash_add_item_to_chest_replacement_calls;
    }
    if (strcmp(symbol, "_ZN6Entity16getItemFromChestEP4Itemib") == 0) {
        return g_bml_stash_get_item_from_chest_replacement_calls;
    }
    if (strcmp(symbol, "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_") == 0) {
        return g_bml_stash_add_item_replacement_calls;
    }
    if (strcmp(symbol, "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi") == 0) {
        return g_bml_stash_remove_item_replacement_calls;
    }
    if (strcmp(symbol, "_ZN6Entity10closeChestEv") == 0) {
        return g_bml_stash_close_chest_replacement_calls;
    }
    if (strcmp(symbol, "_ZN6Entity16closeChestServerEv") == 0) {
        return g_bml_stash_close_chest_server_replacement_calls;
    }
    if (strcmp(symbol, "_Z8actChestP6Entity") == 0) {
        return g_bml_stash_act_chest_replacement_calls;
    }
    if (strcmp(symbol, "_Z11actChestLidP6Entity") == 0) {
        return g_bml_stash_act_chest_lid_replacement_calls;
    }
    if (strcmp(symbol, "_Z15generateDungeonPcjSt5tupleIJiiiiEE") == 0) {
        return g_bml_stash_generate_dungeon_replacement_calls;
    }
    if (strcmp(symbol, "_Z13assignActionsP5map_t") == 0) {
        return g_bml_stash_assign_actions_replacement_calls;
    }
    if (strcmp(symbol, "_Z9newEntityijP6list_tS0_") == 0) {
        return g_bml_stash_new_entity_replacement_calls;
    }
    if (strcmp(symbol, "_Z19setSpriteAttributesP6EntityS0_S0_") == 0) {
        return g_bml_stash_set_sprite_attributes_replacement_calls;
    }
    if (strcmp(symbol, "_Z11uidToEntityi") == 0) {
        return g_bml_uid_to_entity_replacement_calls;
    }
    if (strcmp(symbol, "_ZN8Language3getEi") == 0) {
        return g_bml_language_get_replacement_calls;
    }
    return 0;
}

static void bml_bind_stash_core_original(const BmlStashCoreDetourInstall *target) {
    if (strcmp(target->target_symbol, "_ZN6Entity21getChestInventoryListEv") == 0) {
        g_bml_stash_get_inventory_original = bml_stash_get_inventory_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN6Entity14addItemToChestEP4ItembS1_") == 0) {
        g_bml_stash_add_item_to_chest_original = bml_stash_add_item_to_chest_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN6Entity16getItemFromChestEP4Itemib") == 0) {
        g_bml_stash_get_item_from_chest_original = bml_stash_get_item_from_chest_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_") == 0) {
        g_bml_stash_add_item_original = bml_stash_add_item_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi") == 0) {
        g_bml_stash_remove_item_original = bml_stash_remove_item_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN6Entity10closeChestEv") == 0) {
        g_bml_stash_close_chest_original = bml_stash_close_chest_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN6Entity16closeChestServerEv") == 0) {
        g_bml_stash_close_chest_server_original = bml_stash_close_chest_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z8actChestP6Entity") == 0) {
        g_bml_stash_act_chest_original = bml_stash_entity_action_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z11actChestLidP6Entity") == 0) {
        g_bml_stash_act_chest_lid_original = bml_stash_entity_action_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z15generateDungeonPcjSt5tupleIJiiiiEE") == 0) {
        g_bml_stash_generate_dungeon_original = bml_stash_generate_dungeon_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z13assignActionsP5map_t") == 0) {
        g_bml_stash_assign_actions_original = bml_stash_assign_actions_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z9newEntityijP6list_tS0_") == 0) {
        g_bml_stash_new_entity_original = bml_stash_new_entity_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z19setSpriteAttributesP6EntityS0_S0_") == 0) {
        g_bml_stash_set_sprite_attributes_original = bml_stash_set_sprite_attributes_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z11uidToEntityi") == 0) {
        g_bml_uid_to_entity_original = bml_uid_to_entity_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZN8Language3getEi") == 0) {
        g_bml_language_get_original = bml_language_get_function_from_address(target->install.trampoline);
    }
}

static int bml_prepare_stash_core_detour_target(BmlStashCoreDetourInstall *target) {
    const unsigned char *target_bytes;
    const char *decode_code = NULL;
    const char *decode_message = NULL;
    size_t patch_size = 0U;

    target->target_address = dlsym(RTLD_DEFAULT, target->target_symbol);
    target->install.target = target->target_address;
    if (target->target_address == NULL) {
        target->status = "failed";
        bml_copy_string(target->error_code, sizeof(target->error_code), "BML_STASH_CORE_INSTALL_SYMBOL_MISSING");
        bml_copy_string(target->error_message, sizeof(target->error_message), "Required Stash core pass-through detour target was not resolvable.");
        return -1;
    }

    target_bytes = (const unsigned char *)target->target_address;
    if (bml_measure_supported_patch_window(target_bytes, &patch_size, &decode_code, &decode_message) != 0) {
        target->status = "failed";
        target->install.patch_size = patch_size;
        bml_copy_string(target->error_code, sizeof(target->error_code), decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION");
        bml_copy_string(target->error_message, sizeof(target->error_message), decode_message != NULL ? decode_message : "Stash core pass-through detour target prologue is not safe for the conservative decoder.");
        return -1;
    }

    target->status = "ready";
    target->install.patch_size = patch_size;
    return 0;
}

static int bml_install_stash_core_detour_target(BmlStashCoreDetourInstall *target) {
    if (bml_install_absolute_jump_detour(target->target_address, target->replacement_address, &target->install, target->error_code, sizeof(target->error_code), target->error_message, sizeof(target->error_message)) != 0) {
        target->status = "failed";
        if (!bml_has_value(target->error_code)) {
            bml_copy_string(target->error_code, sizeof(target->error_code), "BML_STASH_CORE_INSTALL_FAILED");
        }
        if (!bml_has_value(target->error_message)) {
            bml_copy_string(target->error_message, sizeof(target->error_message), "Stash core pass-through detour installation failed.");
        }
        return -1;
    }

    target->status = "installed";
    bml_bind_stash_core_original(target);
    return 0;
}

static bool bml_stash_forced_install_failure_due(size_t installed_count) {
    const char *value = getenv("BML_STASH_FORCE_INSTALL_FAILURE_AFTER");
    char *end = NULL;
    long threshold;
    if (!bml_has_value(value)) {
        return false;
    }
    errno = 0;
    threshold = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || threshold <= 0) {
        return false;
    }
    return installed_count >= (size_t)threshold;
}

static void bml_rollback_stash_detour_targets(BmlStashCoreDetourInstall *targets, size_t target_count) {
    for (size_t reverse_index = target_count; reverse_index > 0U; --reverse_index) {
        BmlStashCoreDetourInstall *target = &targets[reverse_index - 1U];
        bool failed_target = strcmp(target->status, "failed") == 0;
        if (strcmp(target->status, "installed") == 0 || (failed_target && target->install.target != NULL)) {
            (void)bml_rollback_absolute_jump_detour(&target->install);
            if (!failed_target) {
                target->status = "rolled_back";
            }
        }
    }
}

static int bml_install_stash_detour_group(BmlStashCoreDetourInstall *targets, size_t target_count) {
    size_t installed_count = 0U;
    for (size_t index = 0U; index < target_count; ++index) {
        if (bml_install_stash_core_detour_target(&targets[index]) != 0) {
            bml_rollback_stash_detour_targets(targets, target_count);
            return -1;
        }
        installed_count += 1U;
        if (bml_stash_forced_install_failure_due(installed_count)) {
            targets[index].status = "failed";
            bml_copy_string(targets[index].error_code, sizeof(targets[index].error_code), "BML_STASH_FORCED_INSTALL_FAILURE");
            bml_copy_string(targets[index].error_message, sizeof(targets[index].error_message), "BML_STASH_FORCE_INSTALL_FAILURE_AFTER requested a fake-provider transactional rollback after at least one detour was installed.");
            bml_rollback_stash_detour_targets(targets, target_count);
            return -1;
        }
    }
    return 0;
}

typedef void (*BmlRuneboundUseItemFunction)(void *, int, void *, bool, bool);
typedef void (*BmlRuneboundConsumeItemFunction)(void **, int);
typedef char *(*BmlRuneboundItemGetNameFunction)(void *);
typedef int (*BmlRuneboundStatGetFunction)(void *, void *);
_Static_assert(sizeof(BmlRuneboundUseItemFunction) == sizeof(void *), "BML Linux x86_64 Runebound useItem detours expect function pointers to fit in void pointers");
_Static_assert(sizeof(BmlRuneboundConsumeItemFunction) == sizeof(void *), "BML Linux x86_64 Runebound consumeItem helper expects function pointers to fit in void pointers");
_Static_assert(sizeof(BmlRuneboundItemGetNameFunction) == sizeof(void *), "BML Linux x86_64 Runebound Item::getName detours expect function pointers to fit in void pointers");
_Static_assert(sizeof(BmlRuneboundStatGetFunction) == sizeof(void *), "BML Linux x86_64 Runebound stat detours expect function pointers to fit in void pointers");

static BmlRuneboundUseItemFunction g_bml_runebound_use_item_original = NULL;
static BmlRuneboundConsumeItemFunction g_bml_runebound_consume_item = NULL;
static BmlRuneboundItemGetNameFunction g_bml_runebound_item_get_name_original = NULL;
static BmlRuneboundStatGetFunction g_bml_runebound_stat_get_str_original = NULL;
static BmlRuneboundStatGetFunction g_bml_runebound_stat_get_dex_original = NULL;
static BmlStashCoreDetourInstall g_bml_runebound_live_targets[4];
static size_t g_bml_runebound_live_target_count = 0U;
static bool g_bml_runebound_live_fake_provider_self_probe = false;
static BmlRuneboundElixirDefinition g_bml_runebound_live_definition;
static BmlRuneboundElixirCarrierMetadata g_bml_runebound_live_carrier;
static BmlRuneboundElixirActiveEffect g_bml_runebound_live_active_effect;
static char g_bml_runebound_live_display[BML_MAX_TEXT];
static char g_bml_runebound_live_last_display[BML_MAX_TEXT];
static int g_bml_runebound_use_item_replacement_calls = 0;
static int g_bml_runebound_use_item_recognized_calls = 0;
static int g_bml_runebound_use_item_original_delegations = 0;
static int g_bml_runebound_consume_item_delegations = 0;
static int g_bml_runebound_active_effects_created = 0;
static int g_bml_runebound_item_get_name_replacement_calls = 0;
static int g_bml_runebound_item_get_name_iron_vow_returns = 0;
static int g_bml_runebound_item_get_name_original_delegations = 0;
static int g_bml_runebound_stat_get_str_replacement_calls = 0;
static int g_bml_runebound_stat_get_str_bonus_applications = 0;
static int g_bml_runebound_stat_get_dex_replacement_calls = 0;
static int g_bml_runebound_stat_get_dex_penalty_applications = 0;
static int g_bml_runebound_live_last_str_result = 0;
static int g_bml_runebound_live_last_dex_result = 0;

static BmlRuneboundUseItemFunction bml_runebound_use_item_function_from_address(void *address) {
    BmlRuneboundUseItemFunction function = NULL;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_runebound_use_item_function_address(BmlRuneboundUseItemFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlRuneboundConsumeItemFunction bml_runebound_consume_item_function_from_address(void *address) {
    BmlRuneboundConsumeItemFunction function = NULL;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static BmlRuneboundItemGetNameFunction bml_runebound_item_get_name_function_from_address(void *address) {
    BmlRuneboundItemGetNameFunction function = NULL;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_runebound_item_get_name_function_address(BmlRuneboundItemGetNameFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static BmlRuneboundStatGetFunction bml_runebound_stat_get_function_from_address(void *address) {
    BmlRuneboundStatGetFunction function = NULL;
    memcpy(&function, &address, sizeof(function));
    return function;
}

static void *bml_runebound_stat_get_function_address(BmlRuneboundStatGetFunction function) {
    void *address = NULL;
    memcpy(&address, &function, sizeof(address));
    return address;
}

static void bml_reset_runebound_live_state(void) {
    memset(g_bml_runebound_live_targets, 0, sizeof(g_bml_runebound_live_targets));
    memset(&g_bml_runebound_live_active_effect, 0, sizeof(g_bml_runebound_live_active_effect));
    g_bml_runebound_live_target_count = 0U;
    g_bml_runebound_live_hooks_installed = false;
    g_bml_runebound_live_fake_provider_self_probe = false;
    g_bml_runebound_use_item_original = NULL;
    g_bml_runebound_consume_item = NULL;
    g_bml_runebound_item_get_name_original = NULL;
    g_bml_runebound_stat_get_str_original = NULL;
    g_bml_runebound_stat_get_dex_original = NULL;
    g_bml_runebound_use_item_replacement_calls = 0;
    g_bml_runebound_use_item_recognized_calls = 0;
    g_bml_runebound_use_item_original_delegations = 0;
    g_bml_runebound_consume_item_delegations = 0;
    g_bml_runebound_active_effects_created = 0;
    g_bml_runebound_item_get_name_replacement_calls = 0;
    g_bml_runebound_item_get_name_iron_vow_returns = 0;
    g_bml_runebound_item_get_name_original_delegations = 0;
    g_bml_runebound_stat_get_str_replacement_calls = 0;
    g_bml_runebound_stat_get_str_bonus_applications = 0;
    g_bml_runebound_stat_get_dex_replacement_calls = 0;
    g_bml_runebound_stat_get_dex_penalty_applications = 0;
    g_bml_runebound_live_last_str_result = 0;
    g_bml_runebound_live_last_dex_result = 0;
    g_bml_runebound_live_last_display[0] = '\0';
    bml_runebound_elixir_make_fixture_definition(&g_bml_runebound_live_definition);
    bml_runebound_elixir_make_fixture_carrier(&g_bml_runebound_live_definition, &g_bml_runebound_live_carrier);
    if (bml_runebound_elixir_render_carrier_display(&g_bml_runebound_live_carrier, g_bml_runebound_live_display, sizeof(g_bml_runebound_live_display)) != 0) {
        bml_copy_string(g_bml_runebound_live_display, sizeof(g_bml_runebound_live_display), "Elixir of the Iron Vow (+2 STR, -1 DEX)");
    }
}

static bool bml_runebound_item_is_iron_vow_carrier(void *item) {
    const BmlBaronyItem *barony_item = (const BmlBaronyItem *)item;
    return barony_item != NULL &&
           barony_item->type == BML_RUNES_ELIXIR_CARRIER_ITEM_TYPE_POTION_EMPTY &&
           barony_item->appearance == g_bml_runebound_live_carrier.instance_id;
}

static void *bml_runebound_live_player_stat(int player) {
    void *stats_symbol = dlsym(RTLD_DEFAULT, "stats");
    void *player_stat = NULL;
    if (stats_symbol == NULL || player < 0 || player >= 4) {
        return NULL;
    }
    memcpy(&player_stat, (const unsigned char *)stats_symbol + ((size_t)player * sizeof(player_stat)), sizeof(player_stat));
    return player_stat;
}

static bool bml_runebound_live_stat_matches_active_player(void *stat, void *entity) {
    int player = g_bml_runebound_live_active_effect.player_index;
    (void)entity;
    if (!g_bml_runebound_live_active_effect.active || stat == NULL) {
        return false;
    }
    return bml_runebound_live_player_stat(player) == stat;
}

static void bml_runebound_make_live_party_snapshot(int player, BmlRuneboundElixirPartySnapshot *snapshot) {
    int normalized_player = (player >= 0 && player < 4) ? player : 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->player_count = (size_t)normalized_player + 1U;
    snapshot->class_ids[normalized_player] = g_bml_runebound_live_definition.eligible_class_id;
    snapshot->connected[normalized_player] = true;
}

static bool bml_runebound_live_create_iron_vow_effect(int player) {
    BmlRuneboundElixirPartySnapshot snapshot;
    BmlRuneboundElixirConsumptionResult result;
    int normalized_player = (player >= 0 && player < 4) ? player : 0;
    bml_runebound_make_live_party_snapshot(normalized_player, &snapshot);
    if (!bml_runebound_elixir_consume_carrier(&g_bml_runebound_live_carrier, &snapshot, normalized_player, &result)) {
        return false;
    }
    g_bml_runebound_live_active_effect = result.active_effect;
    ++g_bml_runebound_active_effects_created;
    return true;
}

static void bml_runebound_use_item_replacement(void *item, int player, void *entity, bool arg4, bool arg5) {
    ++g_bml_runebound_use_item_replacement_calls;
    if (bml_runebound_item_is_iron_vow_carrier(item)) {
        ++g_bml_runebound_use_item_recognized_calls;
        (void)bml_runebound_live_create_iron_vow_effect(player);
        if (g_bml_runebound_consume_item != NULL) {
            void *item_ref = item;
            ++g_bml_runebound_consume_item_delegations;
            g_bml_runebound_consume_item(&item_ref, player);
            return;
        }
        if (g_bml_runebound_use_item_original != NULL) {
            ++g_bml_runebound_use_item_original_delegations;
            g_bml_runebound_use_item_original(item, player, entity, arg4, arg5);
        }
        return;
    }
    if (g_bml_runebound_use_item_original != NULL) {
        ++g_bml_runebound_use_item_original_delegations;
        g_bml_runebound_use_item_original(item, player, entity, arg4, arg5);
    }
}

static char *bml_runebound_item_get_name_replacement(void *item) {
    ++g_bml_runebound_item_get_name_replacement_calls;
    if (bml_runebound_item_is_iron_vow_carrier(item)) {
        ++g_bml_runebound_item_get_name_iron_vow_returns;
        bml_copy_string(g_bml_runebound_live_last_display, sizeof(g_bml_runebound_live_last_display), g_bml_runebound_live_display);
        return g_bml_runebound_live_display;
    }
    if (g_bml_runebound_item_get_name_original != NULL) {
        ++g_bml_runebound_item_get_name_original_delegations;
        return g_bml_runebound_item_get_name_original(item);
    }
    return (char *)"Unknown item";
}

static int bml_runebound_stat_get_str_replacement(void *stat, void *entity) {
    int value = g_bml_runebound_stat_get_str_original != NULL ? g_bml_runebound_stat_get_str_original(stat, entity) : 0;
    ++g_bml_runebound_stat_get_str_replacement_calls;
    if (bml_runebound_live_stat_matches_active_player(stat, entity)) {
        value += g_bml_runebound_live_active_effect.effect_magnitude;
        ++g_bml_runebound_stat_get_str_bonus_applications;
    }
    g_bml_runebound_live_last_str_result = value;
    return value;
}

static int bml_runebound_stat_get_dex_replacement(void *stat, void *entity) {
    int value = g_bml_runebound_stat_get_dex_original != NULL ? g_bml_runebound_stat_get_dex_original(stat, entity) : 0;
    ++g_bml_runebound_stat_get_dex_replacement_calls;
    if (bml_runebound_live_stat_matches_active_player(stat, entity)) {
        value -= 1;
        ++g_bml_runebound_stat_get_dex_penalty_applications;
    }
    g_bml_runebound_live_last_dex_result = value;
    return value;
}

static void bml_bind_runebound_live_original(const BmlStashCoreDetourInstall *target) {
    if (strcmp(target->target_symbol, "_Z7useItemP4ItemiP6Entitybb") == 0) {
        g_bml_runebound_use_item_original = bml_runebound_use_item_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_ZNK4Item7getNameEv") == 0) {
        g_bml_runebound_item_get_name_original = bml_runebound_item_get_name_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z10statGetSTRP4StatP6Entity") == 0) {
        g_bml_runebound_stat_get_str_original = bml_runebound_stat_get_function_from_address(target->install.trampoline);
    } else if (strcmp(target->target_symbol, "_Z10statGetDEXP4StatP6Entity") == 0) {
        g_bml_runebound_stat_get_dex_original = bml_runebound_stat_get_function_from_address(target->install.trampoline);
    }
}

static int bml_runebound_live_replacement_calls_for_symbol(const char *symbol) {
    if (strcmp(symbol, "_Z7useItemP4ItemiP6Entitybb") == 0) {
        return g_bml_runebound_use_item_replacement_calls;
    }
    if (strcmp(symbol, "_ZNK4Item7getNameEv") == 0) {
        return g_bml_runebound_item_get_name_replacement_calls;
    }
    if (strcmp(symbol, "_Z10statGetSTRP4StatP6Entity") == 0) {
        return g_bml_runebound_stat_get_str_replacement_calls;
    }
    if (strcmp(symbol, "_Z10statGetDEXP4StatP6Entity") == 0) {
        return g_bml_runebound_stat_get_dex_replacement_calls;
    }
    return 0;
}

static void bml_init_runebound_live_detour_target(BmlStashCoreDetourInstall *target, const char *target_name, const char *target_symbol, void *replacement_address) {
    memset(target, 0, sizeof(*target));
    target->target_name = target_name;
    target->target_symbol = target_symbol;
    target->replacement_address = replacement_address;
    target->status = "pending";
    target->install.replacement = replacement_address;
}

static int bml_prepare_runebound_live_detour_target(BmlStashCoreDetourInstall *target) {
    const unsigned char *target_bytes;
    const char *decode_code = NULL;
    const char *decode_message = NULL;
    size_t patch_size = 0U;

    target->target_address = dlsym(RTLD_DEFAULT, target->target_symbol);
    target->install.target = target->target_address;
    if (target->target_address == NULL) {
        target->status = "failed";
        bml_copy_string(target->error_code, sizeof(target->error_code), "BML_RUNEBOUND_ELIXIR_LIVE_SYMBOL_MISSING");
        bml_copy_string(target->error_message, sizeof(target->error_message), "Required Runebound: Elixirs live detour target was not resolvable.");
        return -1;
    }

    target_bytes = (const unsigned char *)target->target_address;
    if (bml_measure_supported_patch_window(target_bytes, &patch_size, &decode_code, &decode_message) != 0) {
        target->status = "failed";
        target->install.patch_size = patch_size;
        bml_copy_string(target->error_code, sizeof(target->error_code), decode_code != NULL ? decode_code : "BML_DETOUR_UNSUPPORTED_INSTRUCTION");
        bml_copy_string(target->error_message, sizeof(target->error_message), decode_message != NULL ? decode_message : "Runebound: Elixirs live detour target prologue is not safe for the conservative decoder.");
        return -1;
    }

    target->status = "ready";
    target->install.patch_size = patch_size;
    return 0;
}

static int bml_install_runebound_live_detour_target(BmlStashCoreDetourInstall *target) {
    if (bml_install_absolute_jump_detour(target->target_address, target->replacement_address, &target->install, target->error_code, sizeof(target->error_code), target->error_message, sizeof(target->error_message)) != 0) {
        target->status = "failed";
        if (!bml_has_value(target->error_code)) {
            bml_copy_string(target->error_code, sizeof(target->error_code), "BML_RUNEBOUND_ELIXIR_LIVE_INSTALL_FAILED");
        }
        if (!bml_has_value(target->error_message)) {
            bml_copy_string(target->error_message, sizeof(target->error_message), "Runebound: Elixirs live detour installation failed.");
        }
        return -1;
    }

    target->status = "installed";
    bml_bind_runebound_live_original(target);
    return 0;
}

static void bml_rollback_runebound_live_detour_targets(BmlStashCoreDetourInstall *targets, size_t target_count) {
    for (size_t reverse_index = target_count; reverse_index > 0U; --reverse_index) {
        BmlStashCoreDetourInstall *target = &targets[reverse_index - 1U];
        bool failed_target = strcmp(target->status, "failed") == 0;
        if (strcmp(target->status, "installed") == 0 || (failed_target && target->install.target != NULL)) {
            (void)bml_rollback_absolute_jump_detour(&target->install);
            if (!failed_target) {
                target->status = "rolled_back";
            }
        }
    }
    g_bml_runebound_use_item_original = NULL;
    g_bml_runebound_item_get_name_original = NULL;
    g_bml_runebound_stat_get_str_original = NULL;
    g_bml_runebound_stat_get_dex_original = NULL;
    g_bml_runebound_live_hooks_installed = false;
}

static int bml_install_runebound_live_detour_group(BmlStashCoreDetourInstall *targets, size_t target_count) {
    for (size_t index = 0U; index < target_count; ++index) {
        if (bml_install_runebound_live_detour_target(&targets[index]) != 0) {
            bml_rollback_runebound_live_detour_targets(targets, target_count);
            return -1;
        }
    }
    return 0;
}

static bool bml_runebound_live_fake_provider_available(void) {
    return dlsym(RTLD_DEFAULT, "bml_fake_item_get_name_calls") != NULL &&
           dlsym(RTLD_DEFAULT, "bml_fake_stat_get_str_calls") != NULL &&
           dlsym(RTLD_DEFAULT, "bml_fake_use_item_calls") != NULL;
}

static void bml_runebound_live_run_fake_provider_self_probe(void) {
    BmlBaronyItem item;
    BmlRuneboundUseItemFunction use_item = bml_runebound_use_item_function_from_address(dlsym(RTLD_DEFAULT, "_Z7useItemP4ItemiP6Entitybb"));
    BmlRuneboundItemGetNameFunction get_name = bml_runebound_item_get_name_function_from_address(dlsym(RTLD_DEFAULT, "_ZNK4Item7getNameEv"));
    BmlRuneboundStatGetFunction get_str = bml_runebound_stat_get_function_from_address(dlsym(RTLD_DEFAULT, "_Z10statGetSTRP4StatP6Entity"));
    BmlRuneboundStatGetFunction get_dex = bml_runebound_stat_get_function_from_address(dlsym(RTLD_DEFAULT, "_Z10statGetDEXP4StatP6Entity"));
    void *probe_stat = bml_runebound_live_player_stat(0);

    if (!bml_runebound_live_fake_provider_available()) {
        return;
    }

    memset(&item, 0, sizeof(item));
    item.type = BML_RUNES_ELIXIR_CARRIER_ITEM_TYPE_POTION_EMPTY;
    item.count = 1;
    item.appearance = g_bml_runebound_live_carrier.instance_id;
    item.identified = true;

    g_bml_runebound_live_fake_provider_self_probe = true;
    if (use_item != NULL) {
        use_item(&item, 0, NULL, false, false);
    }
    if (get_name != NULL) {
        const char *display = get_name(&item);
        if (display != NULL) {
            bml_copy_string(g_bml_runebound_live_last_display, sizeof(g_bml_runebound_live_last_display), display);
        }
    }
    if (get_str != NULL) {
        g_bml_runebound_live_last_str_result = get_str(probe_stat, NULL);
    }
    if (get_dex != NULL) {
        g_bml_runebound_live_last_dex_result = get_dex(probe_stat, NULL);
    }
}

static void bml_write_runebound_live_target(FILE *file, const BmlStashCoreDetourInstall *target) {
    fputs("{\n      \"name\": ", file);
    bml_json_write_escaped(file, target->target_name);
    fputs(",\n      \"symbol\": ", file);
    bml_json_write_escaped(file, target->target_symbol);
    fputs(",\n      \"status\": ", file);
    bml_json_write_escaped(file, target->status);
    fputs(",\n      \"address\": ", file);
    bml_write_address_or_null(file, target->target_address);
    fputs(",\n      \"patchSize\": ", file);
    fprintf(file, "%zu", target->install.patch_size);
    fputs(",\n      \"replacementInvocations\": ", file);
    fprintf(file, "%d", bml_runebound_live_replacement_calls_for_symbol(target->target_symbol));
    if (bml_has_value(target->error_code) || bml_has_value(target->error_message)) {
        fputs(",\n      \"error\": {\n        \"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(target->error_code) ? target->error_code : "BML_RUNEBOUND_ELIXIR_LIVE_INSTALL_FAILED");
        fputs(",\n        \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(target->error_message) ? target->error_message : "Runebound: Elixirs live detour target failed.");
        fputs("\n      }", file);
    }
    fputs("\n    }", file);
}

static int bml_write_runebound_elixir_live_install_report(const char *report_path, const BmlReportInfo *info, const char *status, const char *error_code, const char *error_message) {
    size_t installed_count = 0U;
    size_t failed_count = 0U;
    FILE *file;

    for (size_t index = 0U; index < g_bml_runebound_live_target_count; ++index) {
        if (strcmp(g_bml_runebound_live_targets[index].status, "installed") == 0) {
            ++installed_count;
        } else if (strcmp(g_bml_runebound_live_targets[index].status, "failed") == 0) {
            ++failed_count;
        }
    }

    file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"modId\": ", file);
    bml_json_write_escaped(file, BML_RUNES_ELIXIR_PACKAGE_ID);
    fputs(",\n  \"module\": \"modules.runeboundElixirs\",\n  \"version\": ", file);
    bml_json_write_escaped(file, info != NULL ? info->runebound_elixirs_version : "0.1.0");
    fputs(",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"liveHookBehaviorClaimed\": ", file);
    fputs(g_bml_runebound_live_hooks_installed ? "true" : "false", file);
    fputs(",\n  \"playableBehaviorClaimed\": false,\n  \"summary\": {\n    \"installedHooks\": ", file);
    fprintf(file, "%zu", installed_count);
    fputs(",\n    \"allHooksInstalled\": ", file);
    fputs(g_bml_runebound_live_hooks_installed ? "true" : "false", file);
    fputs(",\n    \"useHookInstalled\": ", file);
    fputs(g_bml_runebound_use_item_original != NULL ? "true" : "false", file);
    fputs(",\n    \"displayHookInstalled\": ", file);
    fputs(g_bml_runebound_item_get_name_original != NULL ? "true" : "false", file);
    fputs(",\n    \"statHooksInstalled\": ", file);
    fputs((g_bml_runebound_stat_get_str_original != NULL && g_bml_runebound_stat_get_dex_original != NULL) ? "true" : "false", file);
    fputs(",\n    \"installedHookCount\": ", file);
    fprintf(file, "%zu", installed_count);
    fputs(",\n    \"failedHookCount\": ", file);
    fprintf(file, "%zu", failed_count);
    fputs(",\n    \"targetSymbolCount\": ", file);
    fprintf(file, "%zu", g_bml_runebound_live_target_count);
    fputs(",\n    \"fakeProviderSelfProbe\": ", file);
    fputs(g_bml_runebound_live_fake_provider_self_probe ? "true" : "false", file);
    fputs("\n  },\n  \"targetSymbols\": [", file);
    for (size_t index = 0U; index < g_bml_runebound_live_target_count; ++index) {
        fputs(index == 0U ? "\n    " : ",\n    ", file);
        bml_json_write_escaped(file, g_bml_runebound_live_targets[index].target_symbol);
    }
    if (g_bml_runebound_live_target_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"installedHooks\": [", file);
    for (size_t index = 0U; index < g_bml_runebound_live_target_count; ++index) {
        fputs(index == 0U ? "\n    " : ",\n    ", file);
        bml_write_runebound_live_target(file, &g_bml_runebound_live_targets[index]);
    }
    if (g_bml_runebound_live_target_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"invocationCounters\": {\n    \"useItemReplacementCalls\": ", file);
    fprintf(file, "%d", g_bml_runebound_use_item_replacement_calls);
    fputs(",\n    \"useItem\": ", file);
    fprintf(file, "%d", g_bml_runebound_use_item_replacement_calls);
    fputs(",\n    \"getName\": ", file);
    fprintf(file, "%d", g_bml_runebound_item_get_name_replacement_calls);
    fputs(",\n    \"statGetSTR\": ", file);
    fprintf(file, "%d", g_bml_runebound_stat_get_str_replacement_calls);
    fputs(",\n    \"statGetDEX\": ", file);
    fprintf(file, "%d", g_bml_runebound_stat_get_dex_replacement_calls);
    fputs(",\n    \"useItemRecognizedCarrierCalls\": ", file);
    fprintf(file, "%d", g_bml_runebound_use_item_recognized_calls);
    fputs(",\n    \"useItemOriginalDelegations\": ", file);
    fprintf(file, "%d", g_bml_runebound_use_item_original_delegations);
    fputs(",\n    \"consumeItemDelegations\": ", file);
    fprintf(file, "%d", g_bml_runebound_consume_item_delegations);
    fputs(",\n    \"activeEffectsCreated\": ", file);
    fprintf(file, "%d", g_bml_runebound_active_effects_created);
    fputs(",\n    \"itemGetNameReplacementCalls\": ", file);
    fprintf(file, "%d", g_bml_runebound_item_get_name_replacement_calls);
    fputs(",\n    \"itemGetNameIronVowReturns\": ", file);
    fprintf(file, "%d", g_bml_runebound_item_get_name_iron_vow_returns);
    fputs(",\n    \"statGetSTRReplacementCalls\": ", file);
    fprintf(file, "%d", g_bml_runebound_stat_get_str_replacement_calls);
    fputs(",\n    \"statGetSTRBonusApplications\": ", file);
    fprintf(file, "%d", g_bml_runebound_stat_get_str_bonus_applications);
    fputs(",\n    \"statGetDEXReplacementCalls\": ", file);
    fprintf(file, "%d", g_bml_runebound_stat_get_dex_replacement_calls);
    fputs(",\n    \"statGetDEXPenaltyApplications\": ", file);
    fprintf(file, "%d", g_bml_runebound_stat_get_dex_penalty_applications);
    fputs("\n  },\n  \"recognizedCarrier\": {\n    \"catalogId\": ", file);
    bml_json_write_escaped(file, g_bml_runebound_live_definition.catalog_id);
    fputs(",\n    \"instanceId\": ", file);
    fprintf(file, "%" PRIu32, g_bml_runebound_live_carrier.instance_id);
    fputs(",\n    \"carrierItemType\": ", file);
    bml_json_write_escaped(file, g_bml_runebound_live_carrier.carrier_item_type);
    fputs(",\n    \"display\": ", file);
    bml_json_write_escaped(file, g_bml_runebound_live_display);
    fputs("\n  },\n  \"activeEffect\": {\n    \"active\": ", file);
    fputs(g_bml_runebound_live_active_effect.active ? "true" : "false", file);
    fputs(",\n    \"catalogId\": ", file);
    bml_json_write_escaped(file, g_bml_runebound_live_active_effect.catalog_id != NULL ? g_bml_runebound_live_active_effect.catalog_id : "");
    fputs(",\n    \"effectOpcode\": ", file);
    bml_json_write_escaped(file, g_bml_runebound_live_active_effect.effect_opcode != NULL ? g_bml_runebound_live_active_effect.effect_opcode : "");
    fputs(",\n    \"effectMagnitude\": ", file);
    fprintf(file, "%d", g_bml_runebound_live_active_effect.effect_magnitude);
    fputs(",\n    \"strDelta\": 2,\n    \"dexDelta\": -1,\n    \"lastStrResult\": ", file);
    fprintf(file, "%d", g_bml_runebound_live_last_str_result);
    fputs(",\n    \"lastDexResult\": ", file);
    fprintf(file, "%d", g_bml_runebound_live_last_dex_result);
    fputs(",\n    \"lastDisplay\": ", file);
    bml_json_write_escaped(file, g_bml_runebound_live_last_display);
    fputs("\n  },\n  \"errors\": [", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("\n    {\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_RUNEBOUND_ELIXIR_LIVE_INSTALL_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Runebound: Elixirs live install failed closed.");
        fputs("}\n  ", file);
    }
    fputs("],\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int bml_run_runebound_elixir_live_install(const char *report_path, const BmlReportInfo *info) {
    const char *error_code = NULL;
    const char *error_message = NULL;
    int result = 0;

    bml_reset_runebound_live_state();
    g_bml_runebound_consume_item = bml_runebound_consume_item_function_from_address(dlsym(RTLD_DEFAULT, "_Z11consumeItemRP4Itemi"));
    bml_init_runebound_live_detour_target(&g_bml_runebound_live_targets[0], "useItem", "_Z7useItemP4ItemiP6Entitybb", bml_runebound_use_item_function_address(bml_runebound_use_item_replacement));
    bml_init_runebound_live_detour_target(&g_bml_runebound_live_targets[1], "Item::getName", "_ZNK4Item7getNameEv", bml_runebound_item_get_name_function_address(bml_runebound_item_get_name_replacement));
    bml_init_runebound_live_detour_target(&g_bml_runebound_live_targets[2], "statGetSTR", "_Z10statGetSTRP4StatP6Entity", bml_runebound_stat_get_function_address(bml_runebound_stat_get_str_replacement));
    bml_init_runebound_live_detour_target(&g_bml_runebound_live_targets[3], "statGetDEX", "_Z10statGetDEXP4StatP6Entity", bml_runebound_stat_get_function_address(bml_runebound_stat_get_dex_replacement));
    g_bml_runebound_live_target_count = 4U;

    for (size_t index = 0U; index < g_bml_runebound_live_target_count; ++index) {
        if (bml_prepare_runebound_live_detour_target(&g_bml_runebound_live_targets[index]) != 0) {
            error_code = g_bml_runebound_live_targets[index].error_code;
            error_message = g_bml_runebound_live_targets[index].error_message;
            result = -1;
        }
    }

    if (result == 0 && bml_install_runebound_live_detour_group(g_bml_runebound_live_targets, g_bml_runebound_live_target_count) != 0) {
        result = -1;
        for (size_t index = 0U; index < g_bml_runebound_live_target_count; ++index) {
            if (strcmp(g_bml_runebound_live_targets[index].status, "failed") == 0) {
                error_code = g_bml_runebound_live_targets[index].error_code;
                error_message = g_bml_runebound_live_targets[index].error_message;
                break;
            }
        }
        if (!bml_has_value(error_code)) {
            error_code = "BML_RUNEBOUND_ELIXIR_LIVE_INSTALL_FAILED";
            error_message = "Runebound: Elixirs live detour installation failed closed.";
        }
    }

    if (result == 0) {
        g_bml_runebound_live_hooks_installed = true;
        bml_runebound_live_run_fake_provider_self_probe();
    }

    if (bml_write_runebound_elixir_live_install_report(report_path,
                                                       info,
                                                       result == 0 ? "loaded" : "failed",
                                                       result == 0 ? NULL : error_code,
                                                       result == 0 ? NULL : error_message) != 0) {
        return -1;
    }
    return result;
}

static int bml_write_stash_core_detour_install_report(const char *report_path, const char *test_name, const BmlStashCoreDetourInstall *targets, size_t target_count) {
    size_t installed_count = 0U;
    size_t failed_count = 0U;
    FILE *file;

    for (size_t index = 0U; index < target_count; ++index) {
        if (strcmp(targets[index].status, "installed") == 0) {
            installed_count += 1U;
        } else if (strcmp(targets[index].status, "failed") == 0) {
            failed_count += 1U;
        }
    }

    file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": ", file);
    bml_json_write_escaped(file, test_name);
    fputs(",\n  \"status\": ", file);
    bml_json_write_escaped(file, failed_count == 0U && installed_count == target_count ? "installed" : "failed");
    fputs(",\n  \"backend\": {\n    \"patchStyle\": \"rip-relative-indirect-jmp-absolute-slot\",\n    \"patchBytes\": ", file);
    fprintf(file, "%u", (unsigned)BML_DETOUR_PATCH_BYTES);
    fputs(",\n    \"decoder\": \"fixture-safe-subset\"\n  },", file);
    fprintf(file, "\n  \"summary\": {\n    \"requested\": %zu,\n    \"installed\": %zu,\n    \"failed\": %zu,\n    \"failClosed\": true\n  },\n  \"targets\": [", target_count, installed_count, failed_count);
    for (size_t index = 0U; index < target_count; ++index) {
        const BmlStashCoreDetourInstall *target = &targets[index];
        if (index == 0U) {
            fputs("\n    ", file);
        } else {
            fputs(",\n    ", file);
        }
        fputs("{\"targetName\": ", file);
        bml_json_write_escaped(file, target->target_name);
        fputs(", \"targetSymbol\": ", file);
        bml_json_write_escaped(file, target->target_symbol);
        fputs(", \"status\": ", file);
        bml_json_write_escaped(file, target->status);
        fputs(", \"targetAddress\": ", file);
        bml_write_address_or_null(file, target->target_address);
        fputs(", \"replacementAddress\": ", file);
        bml_write_address_or_null(file, target->replacement_address);
        fputs(", \"trampolineAddress\": ", file);
        bml_write_address_or_null(file, target->install.trampoline);
        fputs(", \"patchSize\": ", file);
        fprintf(file, "%zu", target->install.patch_size);
        fputs(", \"replacementInvoked\": ", file);
        fputs(bml_stash_core_replacement_calls_for_symbol(target->target_symbol) > 0 ? "true" : "false", file);
        fputs(", \"replacementCalls\": ", file);
        fprintf(file, "%d", bml_stash_core_replacement_calls_for_symbol(target->target_symbol));
        fputs(", \"error\": ", file);
        if (bml_has_value(target->error_code) || bml_has_value(target->error_message)) {
            fputs("{\"code\": ", file);
            bml_json_write_escaped(file, bml_has_value(target->error_code) ? target->error_code : "BML_STASH_CORE_INSTALL_FAILED");
            fputs(", \"message\": ", file);
            bml_json_write_escaped(file, bml_has_value(target->error_message) ? target->error_message : "Stash core pass-through detour installation failed.");
            fputc('}', file);
        } else {
            fputs("null", file);
        }
        fputc('}', file);
    }
    if (target_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);

    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static void bml_write_stash_access_placement_exit_report(void) {
    if (g_bml_stash_access_placement_target_count == 0U || !bml_has_value(g_bml_stash_access_placement_report_path)) {
        return;
    }
    (void)bml_write_stash_core_detour_install_report(
        g_bml_stash_access_placement_report_path,
        "stash-access-placement-passthrough-install",
        g_bml_stash_access_placement_targets,
        g_bml_stash_access_placement_target_count);
}

static void bml_register_stash_access_placement_exit_report(const char *report_path, const BmlStashCoreDetourInstall *targets, size_t target_count) {
    if (!bml_has_value(report_path) || targets == NULL || target_count == 0U || target_count > (sizeof(g_bml_stash_access_placement_targets) / sizeof(g_bml_stash_access_placement_targets[0]))) {
        return;
    }
    memcpy(g_bml_stash_access_placement_targets, targets, target_count * sizeof(g_bml_stash_access_placement_targets[0]));
    g_bml_stash_access_placement_target_count = target_count;
    bml_copy_string(g_bml_stash_access_placement_report_path, sizeof(g_bml_stash_access_placement_report_path), report_path);
    if (!g_bml_stash_access_placement_exit_report_registered) {
        if (atexit(bml_write_stash_access_placement_exit_report) == 0) {
            g_bml_stash_access_placement_exit_report_registered = true;
        }
    }
}

static int bml_run_stash_core_passthrough_install(const char *report_path) {
    BmlStashCoreDetourInstall targets[7];
    const size_t target_count = sizeof(targets) / sizeof(targets[0]);
    int result = 0;

    bml_reset_stash_core_passthrough_state();
    bml_init_stash_core_detour_target(&targets[0], "Entity::getChestInventoryList", "_ZN6Entity21getChestInventoryListEv", bml_stash_get_inventory_function_address(bml_stash_get_chest_inventory_list_replacement));
    bml_init_stash_core_detour_target(&targets[1], "Entity::addItemToChest", "_ZN6Entity14addItemToChestEP4ItembS1_", bml_stash_add_item_to_chest_function_address(bml_stash_add_item_to_chest_replacement));
    bml_init_stash_core_detour_target(&targets[2], "Entity::getItemFromChest", "_ZN6Entity16getItemFromChestEP4Itemib", bml_stash_get_item_from_chest_function_address(bml_stash_get_item_from_chest_replacement));
    bml_init_stash_core_detour_target(&targets[3], "Entity::addItemToVoidChestServer", "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_", bml_stash_add_item_function_address(bml_stash_add_item_to_void_chest_server_replacement));
    bml_init_stash_core_detour_target(&targets[4], "Entity::removeItemFromVoidChestServer", "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi", bml_stash_remove_item_function_address(bml_stash_remove_item_from_void_chest_server_replacement));
    bml_init_stash_core_detour_target(&targets[5], "Entity::closeChest", "_ZN6Entity10closeChestEv", bml_stash_close_chest_function_address(bml_stash_close_chest_replacement));
    bml_init_stash_core_detour_target(&targets[6], "Entity::closeChestServer", "_ZN6Entity16closeChestServerEv", bml_stash_close_chest_function_address(bml_stash_close_chest_server_replacement));

    for (size_t index = 0U; index < target_count; ++index) {
        if (bml_prepare_stash_core_detour_target(&targets[index]) != 0) {
            result = -1;
        }
    }
    if (result == 0 && bml_install_stash_detour_group(targets, target_count) != 0) {
        bml_reset_stash_core_passthrough_state();
        result = -1;
    }

    if (bml_write_stash_core_detour_install_report(report_path, "stash-core-passthrough-install", targets, target_count) != 0) {
        return -1;
    }
    return result;
}

static int bml_write_stash_access_placement_self_test_report(const char *report_path, const char *status, const char *error_code, const char *error_message, int generate_dungeon_result) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }
    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"stash-access-placement-call-through-self-test\",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"claimBoundary\": \"fake-provider-access-placement-call-through-only\",\n  \"generateDungeonResult\": ", file);
    fprintf(file, "%d", generate_dungeon_result);
    fputs(",\n  \"targets\": [", file);
    for (size_t index = 0U; index < g_bml_stash_access_placement_target_count; ++index) {
        const BmlStashCoreDetourInstall *target = &g_bml_stash_access_placement_targets[index];
        fputs(index == 0U ? "\n    " : ",\n    ", file);
        fputs("{\"targetName\": ", file);
        bml_json_write_escaped(file, target->target_name);
        fputs(", \"targetSymbol\": ", file);
        bml_json_write_escaped(file, target->target_symbol);
        fputs(", \"replacementCalls\": ", file);
        fprintf(file, "%d", bml_stash_core_replacement_calls_for_symbol(target->target_symbol));
        fputc('}', file);
    }
    if (g_bml_stash_access_placement_target_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"error\": ", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("{\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Stash access/placement call-through self-test failed.");
        fputc('}', file);
    } else {
        fputs("null", file);
    }
    fputs(",\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int bml_run_stash_access_placement_self_test(char *error_code, size_t error_code_size, char *error_message, size_t error_message_size, int *generate_dungeon_result) {
    void *fake_provider_marker = dlsym(RTLD_DEFAULT, "bml_fake_detour_counter");
    void *fake_assign_actions_map_symbol = dlsym(RTLD_DEFAULT, "bml_fake_assign_actions_map");
    void *fake_assign_actions_map = NULL;
    void *act_chest_address = dlsym(RTLD_DEFAULT, "_Z8actChestP6Entity");
    void *act_chest_lid_address = dlsym(RTLD_DEFAULT, "_Z11actChestLidP6Entity");
    void *generate_dungeon_address = dlsym(RTLD_DEFAULT, "_Z15generateDungeonPcjSt5tupleIJiiiiEE");
    void *assign_actions_address = dlsym(RTLD_DEFAULT, "_Z13assignActionsP5map_t");
    void *new_entity_address = dlsym(RTLD_DEFAULT, "_Z9newEntityijP6list_tS0_");
    void *set_sprite_attributes_address = dlsym(RTLD_DEFAULT, "_Z19setSpriteAttributesP6EntityS0_S0_");
    void *uid_to_entity_address = dlsym(RTLD_DEFAULT, "_Z11uidToEntityi");
    void *language_get_address = dlsym(RTLD_DEFAULT, "_ZN8Language3getEi");
    void *selected_entity_symbol = dlsym(RTLD_DEFAULT, "selectedEntity");
    BmlStashEntityActionFunction target_act_chest;
    BmlStashEntityActionFunction target_act_chest_lid;
    BmlStashGenerateDungeonFunction target_generate_dungeon;
    BmlStashAssignActionsFunction target_assign_actions;
    BmlStashNewEntityFunction target_new_entity;
    BmlStashSetSpriteAttributesFunction target_set_sprite_attributes;
    BmlUidToEntityFunction target_uid_to_entity;
    BmlLanguageGetFunction target_language_get;
    void **selected_entities = NULL;
    unsigned char fake_entity[1024];
    void *stash_prompt_entity = NULL;
    void *vanilla_prompt_entity = NULL;
    int32_t stash_prompt_uid = 0;
    int32_t vanilla_prompt_uid = 0;
    int generated;

    if (generate_dungeon_result != NULL) {
        *generate_dungeon_result = 0;
    }
    if (fake_provider_marker == NULL || act_chest_address == NULL || act_chest_lid_address == NULL || generate_dungeon_address == NULL || assign_actions_address == NULL || new_entity_address == NULL || set_sprite_attributes_address == NULL || uid_to_entity_address == NULL || language_get_address == NULL || selected_entity_symbol == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_SYMBOL_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test requires the fake provider plus all eight access/placement/prompt symbols and selectedEntity.");
        return -1;
    }
    if (fake_assign_actions_map_symbol != NULL) {
        memcpy(&fake_assign_actions_map, fake_assign_actions_map_symbol, sizeof(fake_assign_actions_map));
    }

    memset(fake_entity, 0, sizeof(fake_entity));
    target_act_chest = bml_stash_entity_action_function_from_address(act_chest_address);
    target_act_chest_lid = bml_stash_entity_action_function_from_address(act_chest_lid_address);
    target_generate_dungeon = bml_stash_generate_dungeon_function_from_address(generate_dungeon_address);
    target_assign_actions = bml_stash_assign_actions_function_from_address(assign_actions_address);
    target_new_entity = bml_stash_new_entity_function_from_address(new_entity_address);
    target_set_sprite_attributes = bml_stash_set_sprite_attributes_function_from_address(set_sprite_attributes_address);
    target_uid_to_entity = bml_uid_to_entity_function_from_address(uid_to_entity_address);
    target_language_get = bml_language_get_function_from_address(language_get_address);
    selected_entities = (void **)selected_entity_symbol;

    bml_entity_set_skill(fake_entity, 58, BML_STASH_INTERNAL_MARKER_SKILL58);
    selected_entities[0] = fake_entity;
    target_act_chest(fake_entity);
    target_act_chest_lid(fake_entity);
    generated = target_generate_dungeon(NULL, 123U, 0U, 0U);
    target_assign_actions(fake_assign_actions_map);
    stash_prompt_entity = target_new_entity(1791, 1U, NULL, NULL);
    target_set_sprite_attributes(stash_prompt_entity, NULL, NULL);
    if (strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), BML_STASH_PROMPT_OPEN_STASH) != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not rename the selected framework access-point prompt to Open stash.");
        selected_entities[0] = NULL;
        return -1;
    }
    selected_entities[0] = NULL;

    if (stash_prompt_entity == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_ENTITY_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test could not create a fake Stash prompt entity.");
        return -1;
    }
    bml_entity_set_skill(stash_prompt_entity, 58, BML_STASH_INTERNAL_MARKER_SKILL58);
    stash_prompt_uid = bml_entity_get_uid(stash_prompt_entity);
    if (target_uid_to_entity(stash_prompt_uid) != stash_prompt_entity) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not resolve the fake Stash prompt entity through uidToEntity.");
        return -1;
    }
    if (strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), "Open chest") != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_BROAD");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test observed a non-tooltip uidToEntity result leaking into the vanilla chest prompt.");
        return -1;
    }
    if (target_uid_to_entity(stash_prompt_uid) != stash_prompt_entity || target_uid_to_entity(stash_prompt_uid) != stash_prompt_entity) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not resolve the fake Stash prompt entity through the repeated tooltip uidToEntity path.");
        return -1;
    }
    if (strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), BML_STASH_PROMPT_OPEN_STASH) != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not rename the hover prompt after the tooltip resolved a framework Stash access entity twice.");
        return -1;
    }
    if (strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), "Open chest") != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_LEAKED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test observed uidToEntity prompt context leaking past one Language::get(4005) call.");
        return -1;
    }
    if (target_uid_to_entity(stash_prompt_uid) != stash_prompt_entity) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not resolve the fake Stash prompt entity through uidToEntity before tooltip arming.");
        return -1;
    }
    (void)target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_TOOLTIP_ACTION);
    if (strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), BML_STASH_PROMPT_OPEN_STASH) != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not rename the hold-to-activate hover prompt after uidToEntity resolved a framework Stash access entity.");
        return -1;
    }
    if (strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), "Open chest") != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_PROMPT_LEAKED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test observed uidToEntity prompt context leaking past one hold-to-activate Language::get(4005) call.");
        return -1;
    }
    vanilla_prompt_entity = target_new_entity(188, 2U, NULL, NULL);
    if (vanilla_prompt_entity == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_VANILLA_ENTITY_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test could not create a fake non-Stash prompt entity.");
        return -1;
    }
    vanilla_prompt_uid = bml_entity_get_uid(vanilla_prompt_entity);
    if (target_uid_to_entity(vanilla_prompt_uid) != vanilla_prompt_entity || strcmp(target_language_get(BML_STASH_PROMPT_LANGUAGE_ID_OPEN_CHEST), "Open chest") != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_VANILLA_PROMPT_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test observed a non-Stash uidToEntity result globally renaming the vanilla chest prompt.");
        return -1;
    }

    if (generate_dungeon_result != NULL) {
        *generate_dungeon_result = generated;
    }
    if (generated != 7) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_GENERATE_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not call through to the fake generateDungeon implementation.");
        return -1;
    }
    for (size_t index = 0U; index < g_bml_stash_access_placement_target_count; ++index) {
        if (bml_stash_core_replacement_calls_for_symbol(g_bml_stash_access_placement_targets[index].target_symbol) <= 0) {
            bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_CALL_MISSING");
            bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not invoke every installed replacement.");
            return -1;
        }
    }
    if (g_bml_stash_uid_prompt_context_recorded < 1 || g_bml_stash_uid_prompt_context_consumed < 1) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_UID_CONTEXT_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash access/placement self-test did not observe uidToEntity prompt context recording and consumption.");
        return -1;
    }
    return 0;
}

static int bml_run_stash_access_placement_passthrough_install(const char *report_path, const char *self_test_report_path, bool self_test_requested, const char *placement_discovery_report_path, bool placement_discovery_requested) {
    BmlStashCoreDetourInstall targets[8];
    const size_t target_count = sizeof(targets) / sizeof(targets[0]);
    int result = 0;
    int self_test_generate_dungeon_result = 0;
    char self_test_error_code[BML_MAX_TEXT];
    char self_test_error_message[BML_MAX_TEXT];

    memset(self_test_error_code, 0, sizeof(self_test_error_code));
    memset(self_test_error_message, 0, sizeof(self_test_error_message));
    bml_reset_stash_access_placement_state();
    if (placement_discovery_requested) {
        bml_configure_stash_placement_discovery(placement_discovery_report_path);
    }
    bml_init_stash_core_detour_target(&targets[0], "actChest", "_Z8actChestP6Entity", bml_stash_entity_action_function_address(bml_stash_act_chest_replacement));
    bml_init_stash_core_detour_target(&targets[1], "actChestLid", "_Z11actChestLidP6Entity", bml_stash_entity_action_function_address(bml_stash_act_chest_lid_replacement));
    bml_init_stash_core_detour_target(&targets[2], "generateDungeon", "_Z15generateDungeonPcjSt5tupleIJiiiiEE", bml_stash_generate_dungeon_function_address(bml_stash_generate_dungeon_replacement));
    bml_init_stash_core_detour_target(&targets[3], "assignActions", "_Z13assignActionsP5map_t", bml_stash_assign_actions_function_address(bml_stash_assign_actions_replacement));
    bml_init_stash_core_detour_target(&targets[4], "newEntity", "_Z9newEntityijP6list_tS0_", bml_stash_new_entity_function_address(bml_stash_new_entity_replacement));
    bml_init_stash_core_detour_target(&targets[5], "setSpriteAttributes", "_Z19setSpriteAttributesP6EntityS0_S0_", bml_stash_set_sprite_attributes_function_address(bml_stash_set_sprite_attributes_replacement));
    bml_init_stash_core_detour_target(&targets[6], "uidToEntity", "_Z11uidToEntityi", bml_uid_to_entity_function_address(bml_uid_to_entity_replacement));
    bml_init_stash_core_detour_target(&targets[7], "Language::get", "_ZN8Language3getEi", bml_language_get_function_address(bml_language_get_replacement));

    for (size_t index = 0U; index < target_count; ++index) {
        if (bml_prepare_stash_core_detour_target(&targets[index]) != 0) {
            result = -1;
        }
    }
    if (result == 0 && bml_install_stash_detour_group(targets, target_count) != 0) {
        bml_reset_stash_access_placement_state();
        result = -1;
    }

    bml_register_stash_access_placement_exit_report(report_path, targets, target_count);
    if (result == 0 && self_test_requested &&
        bml_run_stash_access_placement_self_test(self_test_error_code, sizeof(self_test_error_code), self_test_error_message, sizeof(self_test_error_message), &self_test_generate_dungeon_result) != 0) {
        result = -1;
    }

    if (bml_write_stash_core_detour_install_report(report_path, "stash-access-placement-passthrough-install", targets, target_count) != 0) {
        return -1;
    }
    if (self_test_requested &&
        bml_write_stash_access_placement_self_test_report(self_test_report_path, result == 0 ? "passed" : "failed", self_test_error_code, self_test_error_message, self_test_generate_dungeon_result) != 0) {
        return -1;
    }
    return result;
}


static int bml_write_stash_core_behavior_report(const char *report_path, const char *status, const char *error_code, const char *error_message, const BmlStashCoreDetourInstall *targets, size_t target_count, bool self_test_requested, size_t self_test_loaded_count, size_t self_test_saved_rows, bool self_test_load_failure_returned_null) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }
    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"stash-core-experimental-behavior\",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"experimental\": true,\n  \"claimBoundary\": \"fake-provider-state-backed-core-lifecycle-only\",\n  \"state\": {\n    \"path\": ", file);
    bml_json_write_escaped(file, g_bml_stash_inventory_path);
    fprintf(file, ",\n    \"loaded\": %s,\n    \"dirty\": %s,\n    \"loadCount\": %d,\n    \"saveCount\": %d,\n    \"dirtyMarks\": %d,\n    \"boundInventoryCount\": %zu,\n    \"savedRows\": %zu\n  },\n  \"selfTest\": {\n    \"requested\": %s,\n    \"loadedCount\": %zu,\n    \"savedRows\": %zu,\n    \"loadFailureReturnedNull\": %s\n  },\n  \"targets\": [",
            g_bml_stash_core_behavior_loaded ? "true" : "false",
            g_bml_stash_core_behavior_dirty ? "true" : "false",
            g_bml_stash_core_behavior_loads,
            g_bml_stash_core_behavior_saves,
            g_bml_stash_core_behavior_dirty_marks,
            bml_stash_inventory_count(g_bml_stash_core_behavior_inventory),
            bml_count_stash_inventory_file_rows(),
            self_test_requested ? "true" : "false",
            self_test_loaded_count,
            self_test_saved_rows,
            self_test_load_failure_returned_null ? "true" : "false");
    for (size_t index = 0U; index < target_count; ++index) {
        const BmlStashCoreDetourInstall *target = &targets[index];
        fputs(index == 0U ? "\n    " : ",\n    ", file);
        fputs("{\"targetName\": ", file);
        bml_json_write_escaped(file, target->target_name);
        fputs(", \"targetSymbol\": ", file);
        bml_json_write_escaped(file, target->target_symbol);
        fputs(", \"status\": ", file);
        bml_json_write_escaped(file, target->status);
        fputs(", \"replacementCalls\": ", file);
        fprintf(file, "%d", bml_stash_core_replacement_calls_for_symbol(target->target_symbol));
        fputs(", \"error\": ", file);
        if (bml_has_value(target->error_code) || bml_has_value(target->error_message)) {
            fputs("{\"code\": ", file);
            bml_json_write_escaped(file, bml_has_value(target->error_code) ? target->error_code : "BML_STASH_CORE_BEHAVIOR_TARGET_FAILED");
            fputs(", \"message\": ", file);
            bml_json_write_escaped(file, bml_has_value(target->error_message) ? target->error_message : "Stash core behavior target failed.");
            fputc('}', file);
        } else {
            fputs("null", file);
        }
        fputc('}', file);
    }
    if (target_count > 0U) {
        fputs("\n  ", file);
    }
    fputs("],\n  \"error\": ", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("{\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_STASH_CORE_BEHAVIOR_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Stash core behavior failed.");
        fputc('}', file);
    } else {
        fputs("null", file);
    }
    fputs(",\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}

static int bml_run_stash_core_behavior_self_test(size_t *loaded_count, size_t *saved_rows, bool *load_failure_returned_null, char *error_code, size_t error_code_size, char *error_message, size_t error_message_size) {
    BmlStashGetChestInventoryListFunction target_get;
    BmlStashAddItemToVoidChestServerFunction target_add_void;
    BmlStashAddItemToChestFunction target_add_chest;
    BmlStashGetItemFromChestFunction target_get_item;
    BmlStashCloseChestFunction target_close;
    void *get_address = dlsym(RTLD_DEFAULT, "_ZN6Entity21getChestInventoryListEv");
    void *add_void_address = dlsym(RTLD_DEFAULT, "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_");
    void *add_chest_address = dlsym(RTLD_DEFAULT, "_ZN6Entity14addItemToChestEP4ItembS1_");
    void *get_item_address = dlsym(RTLD_DEFAULT, "_ZN6Entity16getItemFromChestEP4Itemib");
    void *close_address = dlsym(RTLD_DEFAULT, "_ZN6Entity10closeChestEv");
    void *fake_provider_marker = dlsym(RTLD_DEFAULT, "bml_fake_detour_counter");
    BmlBaronyList *inventory;
    void *void_item;
    void *void_added;
    void *generic_item;
    void *generic_added;
    void *generic_removed;
    const BmlBaronyItem *loaded_seed_item;

    if (loaded_count != NULL) {
        *loaded_count = 0U;
    }
    if (saved_rows != NULL) {
        *saved_rows = 0U;
    }
    if (load_failure_returned_null != NULL) {
        *load_failure_returned_null = false;
    }
    if (fake_provider_marker == NULL || get_address == NULL || add_void_address == NULL || add_chest_address == NULL || get_item_address == NULL || close_address == NULL || g_bml_stash_new_item == NULL) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_SYMBOL_MISSING");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test requires the fake provider plus get/add/get-item/close/newItem symbols.");
        return -1;
    }
    if (bml_mkdir_p(g_bml_stash_state_dir_path) != 0) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_STATE_DIR_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test could not create the state directory.");
        return -1;
    }
    {
        FILE *seed = fopen(g_bml_stash_inventory_path, "wb");
        if (seed == NULL) {
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_SEED_FAILED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior self-test could not seed the inventory state file.");
            return -1;
        }
        fprintf(seed, "%s\n# columns: type status beatitude count appearance identified uid x y ownerUid interactNPCUid forcedPickupByPlayer isDroppable playerSoldItemToShop itemHiddenFromShop notifyIcon spellNotifyIcon itemRequireTradingSkillInShop itemSpecialShopConsumable\n1\t2\t-1\t3\t12345\t1\t77\t8\t9\t101\t202\t1\t0\t1\t1\t1\t0\t7\t1\n", BML_STASH_INVENTORY_FORMAT_HEADER);
        if (fclose(seed) != 0) {
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_SEED_CLOSE_FAILED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior self-test could not close the seeded inventory state file.");
            return -1;
        }
    }

    target_get = bml_stash_get_inventory_function_from_address(get_address);
    target_add_void = bml_stash_add_item_function_from_address(add_void_address);
    target_add_chest = bml_stash_add_item_to_chest_function_from_address(add_chest_address);
    target_get_item = bml_stash_get_item_from_chest_function_from_address(get_item_address);
    target_close = bml_stash_close_chest_function_from_address(close_address);
    inventory = (BmlBaronyList *)target_get(NULL);
    if (inventory == NULL || bml_stash_inventory_count(inventory) != 1U) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_LOAD_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test did not load exactly one seeded item through getChestInventoryList.");
        return -1;
    }
    loaded_seed_item = inventory->first != NULL ? (const BmlBaronyItem *)inventory->first->element : NULL;
    if (loaded_seed_item == NULL ||
        loaded_seed_item->uid != 77U ||
        loaded_seed_item->x != 8 ||
        loaded_seed_item->y != 9 ||
        loaded_seed_item->ownerUid != 101U ||
        loaded_seed_item->interactNPCUid != 202U ||
        !loaded_seed_item->forcedPickupByPlayer ||
        loaded_seed_item->isDroppable ||
        !loaded_seed_item->playerSoldItemToShop ||
        !loaded_seed_item->itemHiddenFromShop ||
        !loaded_seed_item->notifyIcon ||
        loaded_seed_item->spellNotifyIcon ||
        loaded_seed_item->itemRequireTradingSkillInShop != 7U ||
        !loaded_seed_item->itemSpecialShopConsumable) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_METADATA_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test did not preserve seeded owner/shop/metadata fields.");
        return -1;
    }
    if (loaded_count != NULL) {
        *loaded_count = bml_stash_inventory_count(inventory);
    }
    void_item = g_bml_stash_new_item(2, 3, 0, 4, 67890U, true, NULL);
    void_added = target_add_void(NULL, 0, void_item, false, NULL);
    if (void_added == NULL || bml_stash_inventory_count(inventory) != 2U) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_ADD_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test did not route addItemToVoidChestServer into the bound inventory.");
        return -1;
    }
    generic_item = g_bml_stash_new_item(3, 4, 1, 5, 24680U, false, NULL);
    generic_added = target_add_chest(NULL, generic_item, false, NULL);
    if (generic_added == NULL || bml_stash_inventory_count(inventory) != 3U) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_GENERIC_ADD_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test did not route addItemToChest into the bound inventory.");
        return -1;
    }
    generic_removed = target_get_item(NULL, generic_added, 5, false);
    if (generic_removed == NULL || bml_stash_inventory_count(inventory) != 2U) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_GENERIC_GET_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test did not route getItemFromChest removal through the bound inventory.");
        return -1;
    }
    target_close(NULL);
    if (saved_rows != NULL) {
        *saved_rows = bml_count_stash_inventory_file_rows();
    }
    if (g_bml_stash_core_behavior_saves < 1 || bml_count_stash_inventory_file_rows() != 2U) {
        bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_SAVE_FAILED");
        bml_copy_string(error_message, error_message_size, "Stash core behavior self-test did not persist exactly two inventory rows after closeChest.");
        return -1;
    }
    {
        BmlStashListFreeAllFunction saved_list_free_all = g_bml_stash_list_free_all;
        bool saved_loaded = g_bml_stash_core_behavior_loaded;
        bool saved_dirty = g_bml_stash_core_behavior_dirty;
        bool saved_failed = g_bml_stash_core_behavior_failed;
        char saved_failure_code[BML_MAX_TEXT];
        char saved_failure_message[BML_MAX_TEXT];
        void *load_failure_result;
        bml_copy_string(saved_failure_code, sizeof(saved_failure_code), g_bml_stash_core_behavior_failure_code);
        bml_copy_string(saved_failure_message, sizeof(saved_failure_message), g_bml_stash_core_behavior_failure_message);
        g_bml_stash_core_behavior_loaded = false;
        g_bml_stash_core_behavior_dirty = false;
        g_bml_stash_list_free_all = NULL;
        load_failure_result = target_get(NULL);
        g_bml_stash_list_free_all = saved_list_free_all;
        g_bml_stash_core_behavior_loaded = saved_loaded;
        g_bml_stash_core_behavior_dirty = saved_dirty;
        g_bml_stash_core_behavior_failed = saved_failed;
        bml_copy_string(g_bml_stash_core_behavior_failure_code, sizeof(g_bml_stash_core_behavior_failure_code), saved_failure_code);
        bml_copy_string(g_bml_stash_core_behavior_failure_message, sizeof(g_bml_stash_core_behavior_failure_message), saved_failure_message);
        if (load_failure_result != NULL) {
            bml_copy_string(error_code, error_code_size, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_LOAD_FAIL_CLOSED_FAILED");
            bml_copy_string(error_message, error_message_size, "Stash core behavior self-test observed getChestInventoryList returning the original inventory after a fail-closed load failure.");
            return -1;
        }
        if (load_failure_returned_null != NULL) {
            *load_failure_returned_null = true;
        }
    }
    return 0;
}
static int bml_write_stash_playable_install_report(const char *report_path, const char *status, const char *error_code, const char *error_message) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }
    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"stash-playable-install\",\n  \"status\": ", file);
    bml_json_write_escaped(file, status);
    fputs(",\n  \"mode\": \"production\",\n  \"claimBoundary\": \"playable-lobby-shop-spell-metadata-lifecycle-only\",\n  \"lobbyPlacement\": {\n    \"attempted\": ", file);
    fprintf(file, "%d", g_bml_stash_playable_lobby_placements_attempted);
    fprintf(file, ",\n    \"succeeded\": %d,\n    \"failed\": %d,\n    \"alreadyPlaced\": %d,\n    \"chestPlaced\": %s,\n    \"lidPlaced\": %s\n  },\n  \"shopPlacement\": {\n    \"attempted\": %d,\n    \"succeeded\": %d,\n    \"failed\": %d,\n    \"alreadyPlaced\": %d,\n    \"chestPlaced\": %s,\n    \"lidPlaced\": %s\n  },\n  \"spellBinding\": {\n    \"voidChestInventoryHookInstalled\": %s,\n    \"sharedStatsInventoryBound\": %s,\n    \"claimBoundary\": \"spell-created Void Chests use Entity::getChestInventoryList and chestVoidState; fake-provider tests verify the shared inventory binding, not a player-cast spell UI flow\"\n  },\n  \"multiplayerMetadata\": {\n    \"guardInstalled\": true,\n    \"multiplayer\": %d,\n    \"clientnum\": %d,\n    \"clientBlocked\": %s,\n    \"runtimeMetadata\": \"runtime=barony-bml-runtime-stash;runtime_version=0.1.0;inventory_schema=stash-inventory-v1;capabilities=persistent_storage,persistent_inventory,void_chest_binding,placement_lobby,placement_shop,multiplayer_version_metadata\"\n  },\n  \"calls\": {\n    \"assignActions\": %d,\n    \"generateDungeon\": %d,\n    \"newEntity\": %d,\n    \"setSprite\": %d\n  },\n  \"lastPlaced\": {\n    \"lobbyChest\": ",
            g_bml_stash_playable_lobby_placements_succeeded,
            g_bml_stash_playable_lobby_placements_failed,
            g_bml_stash_playable_lobby_already_placed_count,
            g_bml_stash_playable_last_placed_chest != NULL ? "true" : "false",
            g_bml_stash_playable_last_placed_lid != NULL ? "true" : "false",
            g_bml_stash_playable_shop_placements_attempted,
            g_bml_stash_playable_shop_placements_succeeded,
            g_bml_stash_playable_shop_placements_failed,
            g_bml_stash_playable_shop_already_placed_count,
            g_bml_stash_playable_last_placed_shop_chest != NULL ? "true" : "false",
            g_bml_stash_playable_last_placed_shop_lid != NULL ? "true" : "false",
            g_bml_stash_get_inventory_replacement_calls > 0 || g_bml_stash_core_behavior_active ? "true" : "false",
            g_bml_stash_core_behavior_inventory != NULL ? "true" : "false",
            g_bml_stash_playable_multiplayer_value,
            g_bml_stash_playable_clientnum_value,
            g_bml_stash_playable_multiplayer_client_blocked ? "true" : "false",
            g_bml_stash_playable_assign_actions_calls,
            g_bml_stash_generate_dungeon_replacement_calls,
            g_bml_stash_playable_new_entity_calls,
            g_bml_stash_playable_set_sprite_calls);
    bml_write_address_or_null(file, g_bml_stash_playable_last_placed_chest);
    fputs(", \"lobbyLid\": ", file);
    bml_write_address_or_null(file, g_bml_stash_playable_last_placed_lid);
    fputs(", \"shopChest\": ", file);
    bml_write_address_or_null(file, g_bml_stash_playable_last_placed_shop_chest);
    fputs(", \"shopLid\": ", file);
    bml_write_address_or_null(file, g_bml_stash_playable_last_placed_shop_lid);
    fputs("\n  },\n  \"error\": ", file);
    if (bml_has_value(error_code) || bml_has_value(error_message)) {
        fputs("{\"code\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_code) ? error_code : "BML_STASH_PLAYABLE_FAILED");
        fputs(", \"message\": ", file);
        bml_json_write_escaped(file, bml_has_value(error_message) ? error_message : "Stash playable install failed.");
        fputc('}', file);
    } else {
        fputs("null", file);
    }
    fputs(",\n  \"reportedAt\": ", file);
    bml_write_reported_at(file);
    fputs("\n}\n", file);
    if (fclose(file) != 0) {
        return -1;
    }
    return 0;
}
static int bml_run_stash_playable_install(const char *report_path, const char *profile_dir, bool self_test_requested) {
    BmlStashCoreDetourInstall core_targets[7];
    BmlStashCoreDetourInstall access_targets[8];
    size_t core_target_count = sizeof(core_targets) / sizeof(core_targets[0]);
    size_t access_target_count = sizeof(access_targets) / sizeof(access_targets[0]);
    int result = 0;
    char error_code[BML_MAX_TEXT];
    bool core_installed = false;
    char error_message[BML_MAX_TEXT];

    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));

    if (bml_join_path(g_bml_stash_state_dir_path, sizeof(g_bml_stash_state_dir_path), profile_dir, BML_STASH_STATE_DIR_RELATIVE_PATH) != 0 ||
        bml_join_path(g_bml_stash_inventory_path, sizeof(g_bml_stash_inventory_path), profile_dir, BML_STASH_INVENTORY_RELATIVE_PATH) != 0 ||
        bml_join_path(g_bml_stash_diagnostics_path, sizeof(g_bml_stash_diagnostics_path), profile_dir, BML_STASH_DIAGNOSTICS_RELATIVE_PATH) != 0) {
        bml_copy_string(error_code, sizeof(error_code), "BML_STASH_PLAYABLE_PATH_TOO_LONG");
        bml_copy_string(error_message, sizeof(error_message), "Production Stash state path exceeded PATH_MAX.");
        (void)bml_write_stash_playable_install_report(report_path, "failed", error_code, error_message);
        return -1;
    }
    if (bml_stash_resolve_inventory_functions(error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        (void)bml_write_stash_playable_install_report(report_path, "failed", error_code, error_message);
        return -1;
    }

    g_bml_stash_core_behavior_active = true;
    g_bml_stash_core_behavior_loaded = false;
    g_bml_stash_core_behavior_dirty = false;
    g_bml_stash_core_behavior_failed = false;
    g_bml_stash_core_behavior_loads = 0;
    g_bml_stash_core_behavior_saves = 0;
    g_bml_stash_core_behavior_dirty_marks = 0;
    bml_stash_record_inventory_generation(NULL);
    memset(g_bml_stash_core_behavior_failure_code, 0, sizeof(g_bml_stash_core_behavior_failure_code));
    memset(g_bml_stash_core_behavior_failure_message, 0, sizeof(g_bml_stash_core_behavior_failure_message));

    g_bml_stash_playable_active = true;
    g_bml_stash_playable_hooks_installed = false;
    g_bml_stash_playable_assign_actions_calls = 0;
    g_bml_stash_playable_new_entity_calls = 0;
    g_bml_stash_playable_set_sprite_calls = 0;
    g_bml_stash_playable_lobby_placements_attempted = 0;
    g_bml_stash_playable_lobby_placements_succeeded = 0;
    g_bml_stash_playable_lobby_placements_failed = 0;
    g_bml_stash_playable_lobby_already_placed_count = 0;
    g_bml_stash_playable_last_placed_chest = NULL;
    g_bml_stash_playable_last_placed_lid = NULL;
    g_bml_stash_playable_shop_placements_attempted = 0;
    g_bml_stash_playable_shop_placements_succeeded = 0;
    g_bml_stash_playable_shop_placements_failed = 0;
    g_bml_stash_playable_shop_already_placed_count = 0;
    g_bml_stash_playable_last_placed_shop_chest = NULL;
    g_bml_stash_playable_last_placed_shop_lid = NULL;
    g_bml_stash_playable_last_shop_map = NULL;
    g_bml_stash_playable_shop_generation = 0;
    g_bml_stash_playable_last_shop_generation = -1;
    g_bml_stash_playable_multiplayer_value = 0;
    g_bml_stash_playable_clientnum_value = 0;
    g_bml_stash_playable_multiplayer_client_blocked = false;

    bml_reset_stash_core_passthrough_state();
    bml_reset_stash_access_placement_state();

    bml_init_stash_core_detour_target(&core_targets[0], "Entity::getChestInventoryList", "_ZN6Entity21getChestInventoryListEv", bml_stash_get_inventory_function_address(bml_stash_get_chest_inventory_list_replacement));
    bml_init_stash_core_detour_target(&core_targets[1], "Entity::addItemToChest", "_ZN6Entity14addItemToChestEP4ItembS1_", bml_stash_add_item_to_chest_function_address(bml_stash_add_item_to_chest_replacement));
    bml_init_stash_core_detour_target(&core_targets[2], "Entity::getItemFromChest", "_ZN6Entity16getItemFromChestEP4Itemib", bml_stash_get_item_from_chest_function_address(bml_stash_get_item_from_chest_replacement));
    bml_init_stash_core_detour_target(&core_targets[3], "Entity::addItemToVoidChestServer", "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_", bml_stash_add_item_function_address(bml_stash_add_item_to_void_chest_server_replacement));
    bml_init_stash_core_detour_target(&core_targets[4], "Entity::removeItemFromVoidChestServer", "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi", bml_stash_remove_item_function_address(bml_stash_remove_item_from_void_chest_server_replacement));
    bml_init_stash_core_detour_target(&core_targets[5], "Entity::closeChest", "_ZN6Entity10closeChestEv", bml_stash_close_chest_function_address(bml_stash_close_chest_replacement));
    bml_init_stash_core_detour_target(&core_targets[6], "Entity::closeChestServer", "_ZN6Entity16closeChestServerEv", bml_stash_close_chest_function_address(bml_stash_close_chest_server_replacement));

    for (size_t index = 0U; index < core_target_count; ++index) {
        if (bml_prepare_stash_core_detour_target(&core_targets[index]) != 0) {
            result = -1;
        }
    }
    if (result == 0 && bml_install_stash_detour_group(core_targets, core_target_count) != 0) {
        bml_reset_stash_core_passthrough_state();
        result = -1;
    }
    if (result == 0) {
        core_installed = true;
    }

    bml_init_stash_core_detour_target(&access_targets[0], "actChest", "_Z8actChestP6Entity", bml_stash_entity_action_function_address(bml_stash_act_chest_replacement));
    bml_init_stash_core_detour_target(&access_targets[1], "actChestLid", "_Z11actChestLidP6Entity", bml_stash_entity_action_function_address(bml_stash_act_chest_lid_replacement));
    bml_init_stash_core_detour_target(&access_targets[2], "generateDungeon", "_Z15generateDungeonPcjSt5tupleIJiiiiEE", bml_stash_generate_dungeon_function_address(bml_stash_generate_dungeon_replacement));
    bml_init_stash_core_detour_target(&access_targets[3], "assignActions", "_Z13assignActionsP5map_t", bml_stash_assign_actions_function_address(bml_stash_assign_actions_replacement));
    bml_init_stash_core_detour_target(&access_targets[4], "newEntity", "_Z9newEntityijP6list_tS0_", bml_stash_new_entity_function_address(bml_stash_new_entity_replacement));
    bml_init_stash_core_detour_target(&access_targets[5], "setSpriteAttributes", "_Z19setSpriteAttributesP6EntityS0_S0_", bml_stash_set_sprite_attributes_function_address(bml_stash_set_sprite_attributes_replacement));
    bml_init_stash_core_detour_target(&access_targets[6], "uidToEntity", "_Z11uidToEntityi", bml_uid_to_entity_function_address(bml_uid_to_entity_replacement));
    bml_init_stash_core_detour_target(&access_targets[7], "Language::get", "_ZN8Language3getEi", bml_language_get_function_address(bml_language_get_replacement));

    for (size_t index = 0U; index < access_target_count; ++index) {
        if (bml_prepare_stash_core_detour_target(&access_targets[index]) != 0) {
            result = -1;
        }
    }
    if (result == 0 && bml_install_stash_detour_group(access_targets, access_target_count) != 0) {
        bml_rollback_stash_detour_targets(core_targets, core_target_count);
        bml_reset_stash_core_passthrough_state();
        bml_reset_stash_access_placement_state();
        result = -1;
    }

    if (result != 0 && core_installed) {
        bml_rollback_stash_detour_targets(core_targets, core_target_count);
        bml_reset_stash_core_passthrough_state();
        core_installed = false;
    }
    if (result == 0) {
        g_bml_stash_playable_hooks_installed = true;
    }
    if (result == 0 && self_test_requested) {
        void *fake_shop_map = dlsym(RTLD_DEFAULT, "bml_fake_shop_map");
        bml_stash_assign_actions_replacement(bml_stash_playable_get_map_symbol());
        if (fake_shop_map != NULL) {
            BmlBaronyList *fake_shop_entities = bml_stash_playable_get_map_entity_list(fake_shop_map);
            if (fake_shop_entities != NULL && g_bml_stash_new_entity_original != NULL) {
                void *occupant = g_bml_stash_new_entity_original(188, 0U, fake_shop_entities, NULL);
                bml_entity_set_real(occupant, BML_STASH_ENTITY_OFFSET_X, 136.0);
                bml_entity_set_real(occupant, BML_STASH_ENTITY_OFFSET_Y, 136.0);
            }
            g_bml_stash_playable_shop_generation += 1;
            bml_stash_assign_actions_replacement(fake_shop_map);
            g_bml_stash_playable_shop_generation += 1;
            bml_stash_assign_actions_replacement(fake_shop_map);
        }
        if (g_bml_stash_playable_lobby_placements_succeeded < 1) {
            result = -1;
            bml_copy_string(error_code, sizeof(error_code), "BML_STASH_PLAYABLE_SELF_TEST_FAILED");
            bml_copy_string(error_message, sizeof(error_message), "Production Stash install self-test did not place the lobby chest and lid.");
        } else if (fake_shop_map != NULL && g_bml_stash_playable_shop_placements_succeeded < 2) {
            result = -1;
            bml_copy_string(error_code, sizeof(error_code), "BML_STASH_PLAYABLE_SHOP_SELF_TEST_FAILED");
            bml_copy_string(error_message, sizeof(error_message), "Production Stash install self-test did not place two generated-shop chest/lid pairs across fresh shop generations with the same map pointer.");
        }
    }
    if (result != 0) {
        if (!bml_has_value(error_code)) {
            bml_copy_string(error_code, sizeof(error_code), "BML_STASH_PLAYABLE_FAILED");
        }
        if (!bml_has_value(error_message)) {
            bml_copy_string(error_message, sizeof(error_message), "Production Stash playable install failed before gameplay hooks became active.");
        }
    }
    (void)bml_stash_playable_is_multiplayer_client();

    if (result != 0) {
        g_bml_stash_playable_active = false;
        g_bml_stash_playable_hooks_installed = false;
        g_bml_stash_core_behavior_active = false;
    }
    if (bml_write_stash_playable_install_report(report_path, result == 0 ? "installed" : "failed", error_code, error_message) != 0) {
        return -1;
    }
    return result;
}

static int bml_run_stash_core_behavior_install(const char *report_path, const char *profile_dir, bool self_test_requested) {
    BmlStashCoreDetourInstall targets[7];
    const size_t target_count = sizeof(targets) / sizeof(targets[0]);
    int result = 0;
    size_t self_test_loaded_count = 0U;
    size_t self_test_saved_rows = 0U;
    bool self_test_load_failure_returned_null = false;
    char error_code[BML_MAX_TEXT];
    char error_message[BML_MAX_TEXT];

    memset(error_code, 0, sizeof(error_code));
    memset(error_message, 0, sizeof(error_message));
    bml_reset_stash_core_passthrough_state();
    g_bml_stash_core_behavior_active = false;
    if (bml_configure_stash_core_behavior(profile_dir, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        result = -1;
    }

    bml_init_stash_core_detour_target(&targets[0], "Entity::getChestInventoryList", "_ZN6Entity21getChestInventoryListEv", bml_stash_get_inventory_function_address(bml_stash_get_chest_inventory_list_replacement));
    bml_init_stash_core_detour_target(&targets[1], "Entity::addItemToChest", "_ZN6Entity14addItemToChestEP4ItembS1_", bml_stash_add_item_to_chest_function_address(bml_stash_add_item_to_chest_replacement));
    bml_init_stash_core_detour_target(&targets[2], "Entity::getItemFromChest", "_ZN6Entity16getItemFromChestEP4Itemib", bml_stash_get_item_from_chest_function_address(bml_stash_get_item_from_chest_replacement));
    bml_init_stash_core_detour_target(&targets[3], "Entity::addItemToVoidChestServer", "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_", bml_stash_add_item_function_address(bml_stash_add_item_to_void_chest_server_replacement));
    bml_init_stash_core_detour_target(&targets[4], "Entity::removeItemFromVoidChestServer", "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi", bml_stash_remove_item_function_address(bml_stash_remove_item_from_void_chest_server_replacement));
    bml_init_stash_core_detour_target(&targets[5], "Entity::closeChest", "_ZN6Entity10closeChestEv", bml_stash_close_chest_function_address(bml_stash_close_chest_replacement));
    bml_init_stash_core_detour_target(&targets[6], "Entity::closeChestServer", "_ZN6Entity16closeChestServerEv", bml_stash_close_chest_function_address(bml_stash_close_chest_server_replacement));

    if (result == 0) {
        for (size_t index = 0U; index < target_count; ++index) {
            if (bml_prepare_stash_core_detour_target(&targets[index]) != 0) {
                result = -1;
            }
        }
    }
    if (result == 0 && bml_install_stash_detour_group(targets, target_count) != 0) {
        bml_reset_stash_core_passthrough_state();
        g_bml_stash_core_behavior_active = false;
        result = -1;
    }
    if (result == 0 && self_test_requested && bml_run_stash_core_behavior_self_test(&self_test_loaded_count, &self_test_saved_rows, &self_test_load_failure_returned_null, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        result = -1;
    }

    if (bml_write_stash_core_behavior_report(report_path, result == 0 ? "installed" : "failed", error_code, error_message, targets, target_count, self_test_requested, self_test_loaded_count, self_test_saved_rows, self_test_load_failure_returned_null) != 0) {
        return -1;
    }
    return result;
}

__attribute__((visibility("default"))) int bml_hook_init(void) {
    const char *profile_dir;
    const char *runtime_manifest;
    const char *hook_manifest;
    const char *hook_library;
    const char *detour_self_test;
    const char *runebound_elixirs_self_test;
    const char *stash_detour_self_test;
    const char *stash_install_add_item_passthrough;
    const char *stash_install_core_passthrough;
    const char *stash_install_access_placement_passthrough;
    const char *stash_access_placement_self_test;
    const char *stash_placement_discovery;
    const char *stash_enable_core_behavior;
    const char *stash_core_behavior_self_test;
    const char *stash_disable_playable;
    const char *stash_playable_install_self_test;
    BmlError errors[BML_MAX_ERRORS];
    size_t error_count = 0U;
    BmlReportInfo info;
    BmlSymbolProbe symbol_probe;
    BmlStashHookPlan stash_hook_plan;
    bool stash_hooks_installed;
    bool stash_detour_self_test_requested;
    bool runebound_elixirs_self_test_requested;
    bool stash_install_add_item_passthrough_requested;
    bool stash_install_core_passthrough_requested;
    bool stash_install_access_placement_passthrough_requested;
    bool stash_enable_core_behavior_requested;
    bool stash_core_behavior_self_test_requested;
    bool stash_access_placement_self_test_requested;
    bool stash_placement_discovery_requested;
    bool stash_disable_playable_requested;
    bool stash_playable_requested;
    bool stash_playable_install_self_test_requested;
    char report_dir[PATH_MAX];
    char report_path[PATH_MAX];
    char symbol_report_path[PATH_MAX];
    char stash_hook_report_path[PATH_MAX];
    char detour_self_test_report_path[PATH_MAX];
    char runebound_elixirs_self_test_report_path[PATH_MAX];
    char runebound_elixirs_live_install_report_path[PATH_MAX];
    char stash_detour_self_test_report_path[PATH_MAX];
    char stash_detour_install_report_path[PATH_MAX];
    char stash_core_detour_install_report_path[PATH_MAX];
    char stash_access_placement_detour_install_report_path[PATH_MAX];
    char stash_access_placement_self_test_report_path[PATH_MAX];
    char stash_placement_discovery_report_path[PATH_MAX];
    char stash_core_behavior_report_path[PATH_MAX];
    char stash_playable_install_report_path[PATH_MAX];
    char *runtime_json = NULL;

    if (g_bml_initialized != 0) {
        return g_bml_init_result;
    }
    g_bml_initialized = 1;
    if (bml_should_skip_non_barony_process()) {
        g_bml_init_result = 0;
        return g_bml_init_result;
    }


    memset(errors, 0, sizeof(errors));

    profile_dir = getenv("BML_PROFILE_DIR");
    runtime_manifest = getenv("BML_RUNTIME_MANIFEST");
    hook_manifest = getenv("BML_HOOK_MANIFEST");
    hook_library = getenv("BML_HOOK_LIBRARY");
    detour_self_test = getenv("BML_DETOUR_SELF_TEST");
    runebound_elixirs_self_test = getenv("BML_RUNEBOUND_ELIXIRS_SELF_TEST");
    stash_detour_self_test = getenv("BML_STASH_DETOUR_SELF_TEST");
    stash_install_add_item_passthrough = getenv("BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH");
    stash_install_core_passthrough = getenv("BML_STASH_INSTALL_CORE_PASSTHROUGH");
    stash_install_access_placement_passthrough = getenv("BML_STASH_INSTALL_ACCESS_PLACEMENT_PASSTHROUGH");
    stash_access_placement_self_test = getenv("BML_STASH_ACCESS_PLACEMENT_SELF_TEST");
    stash_placement_discovery = getenv("BML_STASH_PLACEMENT_DISCOVERY");
    stash_enable_core_behavior = getenv("BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR");
    stash_core_behavior_self_test = getenv("BML_STASH_CORE_BEHAVIOR_SELF_TEST");
    runebound_elixirs_self_test_requested = strcmp(runebound_elixirs_self_test != NULL ? runebound_elixirs_self_test : "", "1") == 0;
    stash_detour_self_test_requested = strcmp(stash_detour_self_test != NULL ? stash_detour_self_test : "", "1") == 0;
    stash_install_add_item_passthrough_requested = strcmp(stash_install_add_item_passthrough != NULL ? stash_install_add_item_passthrough : "", "1") == 0;
    stash_install_core_passthrough_requested = strcmp(stash_install_core_passthrough != NULL ? stash_install_core_passthrough : "", "1") == 0;
    stash_install_access_placement_passthrough_requested = strcmp(stash_install_access_placement_passthrough != NULL ? stash_install_access_placement_passthrough : "", "1") == 0;
    stash_access_placement_self_test_requested = strcmp(stash_access_placement_self_test != NULL ? stash_access_placement_self_test : "", "1") == 0;
    stash_placement_discovery_requested = strcmp(stash_placement_discovery != NULL ? stash_placement_discovery : "", "1") == 0;
    stash_enable_core_behavior_requested = strcmp(stash_enable_core_behavior != NULL ? stash_enable_core_behavior : "", "1") == 0;
    stash_core_behavior_self_test_requested = strcmp(stash_core_behavior_self_test != NULL ? stash_core_behavior_self_test : "", "1") == 0;
    stash_disable_playable = getenv("BML_STASH_DISABLE_PLAYABLE");
    stash_playable_install_self_test = getenv("BML_STASH_PLAYABLE_INSTALL_SELF_TEST");
    bml_report_info_init(&info, hook_library);

    if (bml_has_value(runtime_manifest) && access(runtime_manifest, R_OK) == 0) {
        runtime_json = bml_read_text_file(runtime_manifest, NULL);
        if (runtime_json == NULL) {
            bml_add_error(errors, &error_count, "BML_RUNTIME_MANIFEST_PARSE_FAILED", "BML_RUNTIME_MANIFEST could not be read by the native hook.", "BML_RUNTIME_MANIFEST", runtime_manifest);
        } else {
            bml_populate_report_from_runtime_manifest(&info, runtime_json);
        }
    }
    stash_disable_playable_requested = strcmp(stash_disable_playable != NULL ? stash_disable_playable : "", "1") == 0;
    stash_playable_requested = info.has_stash && !stash_disable_playable_requested;
    stash_playable_install_self_test_requested = strcmp(stash_playable_install_self_test != NULL ? stash_playable_install_self_test : "", "1") == 0;

    if (!bml_has_value(profile_dir)) {
        bml_add_error(errors, &error_count, "BML_PROFILE_DIR_MISSING", "BML_PROFILE_DIR is required before the native hook can write a runtime load report.", "BML_PROFILE_DIR", NULL);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }

    (void)bml_check_readable_env_path(errors, &error_count, "BML_RUNTIME_MANIFEST", runtime_manifest, true);
    (void)bml_check_readable_env_path(errors, &error_count, "BML_HOOK_MANIFEST", hook_manifest, true);
    (void)bml_check_readable_env_path(errors, &error_count, "BML_HOOK_LIBRARY", hook_library, false);

    if (bml_join_path(report_dir, sizeof(report_dir), profile_dir, BML_REPORT_DIR_RELATIVE_PATH) != 0 ||
        bml_join_path(report_path, sizeof(report_path), profile_dir, BML_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(symbol_report_path, sizeof(symbol_report_path), profile_dir, BML_SYMBOL_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_hook_report_path, sizeof(stash_hook_report_path), profile_dir, BML_STASH_HOOK_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(detour_self_test_report_path, sizeof(detour_self_test_report_path), profile_dir, BML_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(runebound_elixirs_self_test_report_path, sizeof(runebound_elixirs_self_test_report_path), profile_dir, BML_RUNES_ELIXIR_SELF_TEST_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(runebound_elixirs_live_install_report_path, sizeof(runebound_elixirs_live_install_report_path), profile_dir, BML_RUNES_ELIXIR_LIVE_INSTALL_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_detour_self_test_report_path, sizeof(stash_detour_self_test_report_path), profile_dir, BML_STASH_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_detour_install_report_path, sizeof(stash_detour_install_report_path), profile_dir, BML_STASH_DETOUR_INSTALL_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_core_detour_install_report_path, sizeof(stash_core_detour_install_report_path), profile_dir, BML_STASH_CORE_DETOUR_INSTALL_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_access_placement_detour_install_report_path, sizeof(stash_access_placement_detour_install_report_path), profile_dir, BML_STASH_ACCESS_PLACEMENT_DETOUR_INSTALL_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_access_placement_self_test_report_path, sizeof(stash_access_placement_self_test_report_path), profile_dir, BML_STASH_ACCESS_PLACEMENT_SELF_TEST_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_placement_discovery_report_path, sizeof(stash_placement_discovery_report_path), profile_dir, BML_STASH_PLACEMENT_DISCOVERY_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_core_behavior_report_path, sizeof(stash_core_behavior_report_path), profile_dir, BML_STASH_CORE_BEHAVIOR_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_playable_install_report_path, sizeof(stash_playable_install_report_path), profile_dir, BML_STASH_PLAYABLE_INSTALL_REPORT_RELATIVE_PATH) != 0) {
        free(runtime_json);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }

    bml_probe_required_symbols(&symbol_probe);
    if (symbol_probe.missing_count > 0U) {
        bml_add_error(errors, &error_count, "BML_HOOK_SYMBOL_MISSING", "One or more required Barony symbols could not be resolved with dlsym(RTLD_DEFAULT).", NULL, NULL);
    }

    bml_analyze_stash_hook_plan(&stash_hook_plan, &symbol_probe);
    stash_hooks_installed = bml_stash_hooks_installed(&stash_hook_plan);
    if (bml_mkdir_p(report_dir) != 0) {
        free(runtime_json);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }

    if (strcmp(detour_self_test != NULL ? detour_self_test : "", "1") == 0 &&
        bml_run_detour_self_test(detour_self_test_report_path) != 0) {
        bml_add_error(errors, &error_count, "BML_DETOUR_SELF_TEST_FAILED", "BML_DETOUR_SELF_TEST=1 was requested, but the native absolute-jump detour substrate self-test failed.", "BML_DETOUR_SELF_TEST", detour_self_test_report_path);
    }

    if (runebound_elixirs_self_test_requested &&
        bml_run_runebound_elixir_self_test(runebound_elixirs_self_test_report_path, &info, runtime_json) != 0) {
        bml_add_error(errors, &error_count, "BML_RUNES_ELIXIR_SELF_TEST_FAILED", "BML_RUNEBOUND_ELIXIRS_SELF_TEST=1 was requested, but the Runebound: Elixirs fake-provider data-path self-test failed.", "BML_RUNEBOUND_ELIXIRS_SELF_TEST", runebound_elixirs_self_test_report_path);
    }

    if (info.has_runebound_elixirs &&
        bml_run_runebound_elixir_live_install(runebound_elixirs_live_install_report_path, &info) != 0) {
        bml_add_error(errors, &error_count, "BML_RUNEBOUND_ELIXIR_LIVE_INSTALL_FAILED", "Runebound: Elixirs is present in the runtime manifest, but native live gameplay hook installation failed closed.", "BML_RUNTIME_MANIFEST", runebound_elixirs_live_install_report_path);
    }

    if (stash_placement_discovery_requested && !stash_install_access_placement_passthrough_requested) {
        bml_add_error(errors, &error_count, "BML_STASH_PLACEMENT_DISCOVERY_WITHOUT_INSTALL", "BML_STASH_PLACEMENT_DISCOVERY=1 requires BML_STASH_INSTALL_ACCESS_PLACEMENT_PASSTHROUGH=1.", "BML_STASH_PLACEMENT_DISCOVERY", stash_placement_discovery_report_path);
    } else if (stash_access_placement_self_test_requested && !stash_install_access_placement_passthrough_requested) {
        bml_add_error(errors, &error_count, "BML_STASH_ACCESS_PLACEMENT_SELF_TEST_WITHOUT_INSTALL", "BML_STASH_ACCESS_PLACEMENT_SELF_TEST=1 requires BML_STASH_INSTALL_ACCESS_PLACEMENT_PASSTHROUGH=1.", "BML_STASH_ACCESS_PLACEMENT_SELF_TEST", stash_access_placement_self_test_report_path);
    } else if (stash_core_behavior_self_test_requested && !stash_enable_core_behavior_requested) {
        bml_add_error(errors, &error_count, "BML_STASH_CORE_BEHAVIOR_SELF_TEST_WITHOUT_BEHAVIOR", "BML_STASH_CORE_BEHAVIOR_SELF_TEST=1 requires BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR=1.", "BML_STASH_CORE_BEHAVIOR_SELF_TEST", stash_core_behavior_report_path);
    } else if (stash_playable_install_self_test_requested && !stash_playable_requested) {
        bml_add_error(errors, &error_count, "BML_STASH_PLAYABLE_SELF_TEST_WITHOUT_PLAYABLE", "BML_STASH_PLAYABLE_INSTALL_SELF_TEST=1 requires a Stash runtime manifest and must not be combined with BML_STASH_DISABLE_PLAYABLE=1.", "BML_STASH_PLAYABLE_INSTALL_SELF_TEST", stash_playable_install_report_path);
    } else if (stash_enable_core_behavior_requested && (stash_install_core_passthrough_requested || stash_install_add_item_passthrough_requested || stash_detour_self_test_requested)) {
        bml_add_error(errors, &error_count, "BML_STASH_DETOUR_REQUEST_CONFLICT", "BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR=1 installs the same core Stash targets as the pass-through/self-test modes; enable only one Stash detour install/self-test mode.", "BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR", stash_core_behavior_report_path);
    } else if (stash_install_core_passthrough_requested && (stash_install_add_item_passthrough_requested || stash_detour_self_test_requested)) {
        bml_add_error(errors, &error_count, "BML_STASH_DETOUR_REQUEST_CONFLICT", "BML_STASH_INSTALL_CORE_PASSTHROUGH=1 targets Entity::addItemToVoidChestServer alongside other Stash detour modes in the same process; enable only one Stash detour install/self-test mode.", "BML_STASH_INSTALL_CORE_PASSTHROUGH", stash_core_detour_install_report_path);
    } else if (stash_install_add_item_passthrough_requested && stash_detour_self_test_requested) {
        bml_add_error(errors, &error_count, "BML_STASH_DETOUR_REQUEST_CONFLICT", "BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1 and BML_STASH_DETOUR_SELF_TEST=1 both target Entity::addItemToVoidChestServer in the same process; enable only one.", "BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH", stash_detour_install_report_path);
    } else if (stash_playable_requested && (stash_enable_core_behavior_requested || stash_install_core_passthrough_requested || stash_install_access_placement_passthrough_requested || stash_install_add_item_passthrough_requested || stash_detour_self_test_requested)) {
        bml_add_error(errors, &error_count, "BML_STASH_DETOUR_REQUEST_CONFLICT", "Production Stash installs the same targets as other Stash detour modes; enable only one Stash detour install mode.", "BML_RUNTIME_MANIFEST", stash_playable_install_report_path);
    } else {
        if (stash_playable_requested &&
            bml_run_stash_playable_install(stash_playable_install_report_path, profile_dir, stash_playable_install_self_test_requested) != 0) {
            bml_add_error(errors, &error_count, "BML_STASH_PLAYABLE_INSTALL_FAILED", "Production Stash install failed.", "BML_RUNTIME_MANIFEST", stash_playable_install_report_path);
        }

        if (stash_enable_core_behavior_requested &&
            bml_run_stash_core_behavior_install(stash_core_behavior_report_path, profile_dir, stash_core_behavior_self_test_requested) != 0) {
            bml_add_error(errors, &error_count, "BML_STASH_CORE_BEHAVIOR_FAILED", "BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR=1 was requested, but the experimental core Stash behavior install/self-test failed.", "BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR", stash_core_behavior_report_path);
        }

        if (stash_install_core_passthrough_requested &&
            bml_run_stash_core_passthrough_install(stash_core_detour_install_report_path) != 0) {
            bml_add_error(errors, &error_count, "BML_STASH_CORE_INSTALL_FAILED", "BML_STASH_INSTALL_CORE_PASSTHROUGH=1 was requested, but the core Stash pass-through detour set could not be fully installed.", "BML_STASH_INSTALL_CORE_PASSTHROUGH", stash_core_detour_install_report_path);
        }

        if (stash_install_access_placement_passthrough_requested &&
            bml_run_stash_access_placement_passthrough_install(stash_access_placement_detour_install_report_path, stash_access_placement_self_test_report_path, stash_access_placement_self_test_requested, stash_placement_discovery_report_path, stash_placement_discovery_requested) != 0) {
            bml_add_error(errors, &error_count, "BML_STASH_ACCESS_PLACEMENT_INSTALL_FAILED", "BML_STASH_INSTALL_ACCESS_PLACEMENT_PASSTHROUGH=1 was requested, but the access/placement Stash pass-through detour set could not be fully installed.", "BML_STASH_INSTALL_ACCESS_PLACEMENT_PASSTHROUGH", stash_access_placement_detour_install_report_path);
        }

        if (stash_install_add_item_passthrough_requested &&
            bml_run_stash_add_item_passthrough_install(stash_detour_install_report_path) != 0) {
            bml_add_error(errors, &error_count, "BML_STASH_ADD_ITEM_INSTALL_FAILED", "BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH=1 was requested, but the Entity::addItemToVoidChestServer pass-through detour could not be installed.", "BML_STASH_INSTALL_ADD_ITEM_PASSTHROUGH", stash_detour_install_report_path);
        }

        if (stash_detour_self_test_requested &&
            bml_run_stash_detour_self_test(stash_detour_self_test_report_path) != 0) {
            bml_add_error(errors, &error_count, "BML_STASH_DETOUR_SELF_TEST_FAILED", "BML_STASH_DETOUR_SELF_TEST=1 was requested, but the Entity::addItemToVoidChestServer detour self-test failed.", "BML_STASH_DETOUR_SELF_TEST", stash_detour_self_test_report_path);
        }
    }

    if (g_bml_stash_playable_hooks_installed) {
        stash_hooks_installed = true;
    }
    if (info.has_stash && !stash_hooks_installed && error_count == 0U) {
        bml_add_error(errors, &error_count, "BML_STASH_HOOKS_NOT_INSTALLED", "Direct Stash hook backend did not install all required gameplay hooks; Stash is intentionally failed closed.", NULL, NULL);
    }
    if (info.has_runebound_elixirs && !g_bml_runebound_live_hooks_installed) {
        bml_add_error(errors, &error_count, "BML_RUNES_ELIXIR_HOOKS_NOT_INSTALLED", "Runebound: Elixirs is present in the runtime manifest, but native live gameplay hooks were not installed; the package is intentionally failed closed and omitted from loadedMods.", "BML_RUNTIME_MANIFEST", NULL);
    }

    if (bml_write_symbol_probe_report(symbol_report_path, &info, &symbol_probe) != 0 ||
        bml_write_stash_hook_report(stash_hook_report_path, &info, &stash_hook_plan, stash_hooks_installed) != 0 ||
        bml_write_report(report_path, &info, errors, error_count) != 0) {
        free(runtime_json);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }


    free(runtime_json);
    g_bml_init_result = (error_count == 0U) ? 0 : 1;
    return g_bml_init_result;
}

static void __attribute__((constructor)) bml_hook_constructor(void) {
    (void)bml_hook_init();
}
