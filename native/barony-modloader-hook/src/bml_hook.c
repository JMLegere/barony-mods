#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <errno.h>
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
#define BML_MAX_ERRORS 12
#define BML_MAX_TEXT 256
#define BML_MAX_MANIFEST_BYTES (1024U * 1024U)
#define BML_MAX_REQUIRED_SYMBOLS 32
#define BML_DETOUR_PATCH_BYTES 14U
#define BML_DETOUR_MAX_COPY_BYTES 32U
#define BML_DETOUR_MAX_INSTRUCTIONS 32U
#define BML_DETOUR_MAX_RELOCATED_BYTES ((BML_DETOUR_MAX_COPY_BYTES * 8U) + BML_DETOUR_PATCH_BYTES)

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
} BmlDetourInstall;

typedef struct BmlPatchInstruction {
    size_t source_offset;
    size_t source_length;
    size_t relocated_offset;
    size_t relocated_length;
} BmlPatchInstruction;

static int g_bml_initialized = 0;
static int g_bml_init_result = 1;

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
    {"shoparea", "shoparea", "data"},
    {"TileEntityList", "TileEntityList", "data"}
};

_Static_assert((sizeof(BML_REQUIRED_SYMBOLS) / sizeof(BML_REQUIRED_SYMBOLS[0])) <= BML_MAX_REQUIRED_SYMBOLS, "BML symbol probe result capacity is too small");

static const char *const BML_STASH_VOID_CHEST_BINDING_TARGETS[] = {
    "_Z8actChestP6Entity",
    "_Z11actChestLidP6Entity",
    "_ZN6Entity21getChestInventoryListEv",
    "_ZN6Entity24addItemToVoidChestServerEiP4ItembS1_",
    "_ZN6Entity29removeItemFromVoidChestServerEiP4Itemi"
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
    "map",
    "map_rng",
    "map_server_rng",
    "TileEntityList"
};

static const char *const BML_STASH_SHOP_PLACEMENT_TARGETS[] = {
    "_Z15generateDungeonPcjSt5tupleIJiiiiEE",
    "_Z13assignActionsP5map_t",
    "_Z9newEntityijP6list_tS0_",
    "_Z19setSpriteAttributesP6EntityS0_S0_",
    "map",
    "map_rng",
    "map_server_rng",
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
    info->has_stash = bml_runtime_manifest_has_mod(manifest_json, "jml.stash");
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
    return op == 0x31U || op == 0x39U || op == 0x3bU || op == 0x85U || op == 0x89U || op == 0x8bU || op == 0x8dU;
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
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_RELATIVE_CONTROL_FLOW_UNSUPPORTED");
        bml_copy_string(error_message, error_message_size, "Detour target prologue branches before the copied patch window; this conservative relocator does not support that shape.");
        return -1;
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
    if (info->has_stash && error_count == 0U) {
        fputs("\n    {\n      \"id\": \"jml.stash\",\n      \"version\": ", file);
        bml_json_write_escaped(file, info->stash_version);
        fputs(",\n      \"status\": \"loaded\",\n      \"capabilities\": [\n        \"persistent_storage\",\n        \"persistent_inventory\",\n        \"void_chest_binding\",\n        \"placement_lobby\",\n        \"placement_shop\",\n        \"multiplayer_version_metadata\"\n      ],\n      \"modules\": [\n        \"persistentStorage\",\n        \"persistentInventories\",\n        \"voidChestBindings\",\n        \"placements\",\n        \"multiplayer\"\n      ]\n    }\n  ", file);
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

typedef void *(*BmlStashAddItemToVoidChestServerFunction)(void *, int, void *, bool, void *);
_Static_assert(sizeof(BmlStashAddItemToVoidChestServerFunction) == sizeof(void *), "BML Linux x86_64 Stash detour self-test expects function pointers to fit in void pointers");

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

static BmlStashAddItemToVoidChestServerFunction g_bml_stash_add_item_original = NULL;
static int g_bml_stash_add_item_replacement_calls = 0;
static void *g_bml_stash_add_item_original_result = NULL;
static void *g_bml_stash_add_item_replacement_result = NULL;

static void *bml_stash_add_item_to_void_chest_server_replacement(void *entity, int player, void *item, bool force_new_stack, void *picked_up_stack) {
    ++g_bml_stash_add_item_replacement_calls;
    if (g_bml_stash_add_item_original != NULL) {
        g_bml_stash_add_item_original_result = g_bml_stash_add_item_original(entity, player, item, force_new_stack, picked_up_stack);
    }
    g_bml_stash_add_item_replacement_result = g_bml_stash_add_item_original_result;
    return g_bml_stash_add_item_replacement_result;
}

static int bml_write_stash_detour_self_test_report(const char *report_path, const char *status, const char *error_code, const char *error_message, const BmlDetourInstall *install, void *direct_result, void *original_result, int replacement_calls) {
    FILE *file = fopen(report_path, "wb");
    if (file == NULL) {
        return -1;
    }

    fputs("{\n  \"schemaVersion\": \"0.1.0\",\n  \"test\": \"stash-add-item-detour-self-test\",\n  \"status\": ", file);
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
        (void)bml_write_stash_detour_self_test_report(report_path, "failed", error_code, error_message, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    if (bml_install_absolute_jump_detour(target_address, bml_stash_add_item_function_address(replacement_function), &install, error_code, sizeof(error_code), error_message, sizeof(error_message)) != 0) {
        (void)bml_write_stash_detour_self_test_report(report_path, "failed", error_code, error_message, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    g_bml_stash_add_item_original = bml_stash_add_item_function_from_address(install.trampoline);
    direct_result = target_function(NULL, 0, NULL, false, NULL);

    if (g_bml_stash_add_item_replacement_calls != 1 || (uintptr_t)g_bml_stash_add_item_original_result != 42U || direct_result != g_bml_stash_add_item_original_result) {
        bml_copy_string(error_code, sizeof(error_code), "BML_STASH_DETOUR_SELF_TEST_ASSERTION_FAILED");
        bml_copy_string(error_message, sizeof(error_message), "Stash target detour self-test did not observe replacement invocation and original trampoline call-through with the expected fixture result.");
        (void)bml_write_stash_detour_self_test_report(report_path, "failed", error_code, error_message, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls);
        return -1;
    }

    if (bml_write_stash_detour_self_test_report(report_path, "loaded", NULL, NULL, &install, direct_result, g_bml_stash_add_item_original_result, g_bml_stash_add_item_replacement_calls) != 0) {
        return -1;
    }

    return 0;
}

__attribute__((visibility("default"))) int bml_hook_init(void) {
    const char *profile_dir;
    const char *runtime_manifest;
    const char *hook_manifest;
    const char *hook_library;
    const char *detour_self_test;
    const char *stash_detour_self_test;
    BmlError errors[BML_MAX_ERRORS];
    size_t error_count = 0U;
    BmlReportInfo info;
    BmlSymbolProbe symbol_probe;
    BmlStashHookPlan stash_hook_plan;
    bool stash_hooks_installed;
    char report_dir[PATH_MAX];
    char report_path[PATH_MAX];
    char symbol_report_path[PATH_MAX];
    char stash_hook_report_path[PATH_MAX];
    char detour_self_test_report_path[PATH_MAX];
    char stash_detour_self_test_report_path[PATH_MAX];
    char *runtime_json = NULL;

    if (g_bml_initialized != 0) {
        return g_bml_init_result;
    }
    g_bml_initialized = 1;

    memset(errors, 0, sizeof(errors));

    profile_dir = getenv("BML_PROFILE_DIR");
    runtime_manifest = getenv("BML_RUNTIME_MANIFEST");
    hook_manifest = getenv("BML_HOOK_MANIFEST");
    hook_library = getenv("BML_HOOK_LIBRARY");
    detour_self_test = getenv("BML_DETOUR_SELF_TEST");
    stash_detour_self_test = getenv("BML_STASH_DETOUR_SELF_TEST");

    bml_report_info_init(&info, hook_library);

    if (!bml_has_value(profile_dir)) {
        bml_add_error(errors, &error_count, "BML_PROFILE_DIR_MISSING", "BML_PROFILE_DIR is required before the native hook can write a runtime load report.", "BML_PROFILE_DIR", NULL);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }

    (void)bml_check_readable_env_path(errors, &error_count, "BML_RUNTIME_MANIFEST", runtime_manifest, true);
    (void)bml_check_readable_env_path(errors, &error_count, "BML_HOOK_MANIFEST", hook_manifest, true);
    (void)bml_check_readable_env_path(errors, &error_count, "BML_HOOK_LIBRARY", hook_library, false);

    if (bml_has_value(runtime_manifest) && access(runtime_manifest, R_OK) == 0) {
        runtime_json = bml_read_text_file(runtime_manifest, NULL);
        if (runtime_json == NULL) {
            bml_add_error(errors, &error_count, "BML_RUNTIME_MANIFEST_PARSE_FAILED", "BML_RUNTIME_MANIFEST could not be read by the native hook.", "BML_RUNTIME_MANIFEST", runtime_manifest);
        } else {
            bml_populate_report_from_runtime_manifest(&info, runtime_json);
        }
    }

    if (bml_join_path(report_dir, sizeof(report_dir), profile_dir, BML_REPORT_DIR_RELATIVE_PATH) != 0 ||
        bml_join_path(report_path, sizeof(report_path), profile_dir, BML_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(symbol_report_path, sizeof(symbol_report_path), profile_dir, BML_SYMBOL_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_hook_report_path, sizeof(stash_hook_report_path), profile_dir, BML_STASH_HOOK_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(detour_self_test_report_path, sizeof(detour_self_test_report_path), profile_dir, BML_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH) != 0 ||
        bml_join_path(stash_detour_self_test_report_path, sizeof(stash_detour_self_test_report_path), profile_dir, BML_STASH_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH) != 0) {
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
    if (info.has_stash && !stash_hooks_installed) {
        bml_add_error(errors, &error_count, "BML_STASH_HOOKS_NOT_INSTALLED", "Direct Stash hook backend did not install all required gameplay hooks; Stash is intentionally failed closed.", NULL, NULL);
    }

    if (bml_mkdir_p(report_dir) != 0) {
        free(runtime_json);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }

    if (strcmp(detour_self_test != NULL ? detour_self_test : "", "1") == 0 &&
        bml_run_detour_self_test(detour_self_test_report_path) != 0) {
        bml_add_error(errors, &error_count, "BML_DETOUR_SELF_TEST_FAILED", "BML_DETOUR_SELF_TEST=1 was requested, but the native absolute-jump detour substrate self-test failed.", "BML_DETOUR_SELF_TEST", detour_self_test_report_path);
    }

    if (strcmp(stash_detour_self_test != NULL ? stash_detour_self_test : "", "1") == 0 &&
        bml_run_stash_detour_self_test(stash_detour_self_test_report_path) != 0) {
        bml_add_error(errors, &error_count, "BML_STASH_DETOUR_SELF_TEST_FAILED", "BML_STASH_DETOUR_SELF_TEST=1 was requested, but the Entity::addItemToVoidChestServer detour self-test failed.", "BML_STASH_DETOUR_SELF_TEST", stash_detour_self_test_report_path);
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
