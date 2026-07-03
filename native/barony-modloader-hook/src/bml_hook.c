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
#define BML_MAX_ERRORS 12
#define BML_MAX_TEXT 256
#define BML_MAX_MANIFEST_BYTES (1024U * 1024U)
#define BML_MAX_REQUIRED_SYMBOLS 32
#define BML_DETOUR_PATCH_BYTES 14U
#define BML_DETOUR_MAX_COPY_BYTES 32U

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


static bool bml_modrm_is_register_only(unsigned char modrm) {
    return (modrm & 0xc0U) == 0xc0U;
}

static bool bml_modrm_uses_rip_relative(unsigned char modrm) {
    return (modrm & 0xc7U) == 0x05U;
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

    if (op == 0xc2U || op == 0xc3U || op == 0xcaU || op == 0xcbU) {
        *out_code = "BML_DETOUR_EARLY_RETURN_UNSUPPORTED";
        *out_message = "Detour target returns before the absolute-jump patch window can be reserved.";
        return -1;
    }

    if (op == 0xe8U || op == 0xe9U || op == 0xebU || bml_byte_is_short_relative_branch(op) ||
        (op == 0x0fU && offset + 1U < limit && code[offset + 1U] >= 0x80U && code[offset + 1U] <= 0x8fU)) {
        *out_code = "BML_DETOUR_RELATIVE_CONTROL_FLOW_UNSUPPORTED";
        *out_message = "Detour target prologue contains relative control flow that this substrate does not relocate.";
        return -1;
    }

    if (op >= 0x40U && op <= 0x4fU) {
        unsigned char next;
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

    if (op == 0x89U || op == 0x8bU) {
        if (offset + 2U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported register mov instruction.";
            return -1;
        }
        if (!bml_modrm_is_register_only(code[offset + 1U])) {
            *out_code = bml_modrm_uses_rip_relative(code[offset + 1U]) ? "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" : "BML_DETOUR_MEMORY_OPERAND_UNSUPPORTED";
            *out_message = "Detour target prologue uses memory addressing that this substrate does not relocate.";
            return -1;
        }
        *out_length = 2U;
        return 0;
    }

    if (op == 0x83U) {
        unsigned char modrm;
        unsigned char reg_opcode;
        if (offset + 3U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended in the middle of a supported add/sub immediate instruction.";
            return -1;
        }
        modrm = code[offset + 1U];
        reg_opcode = (unsigned char)((modrm >> 3U) & 0x07U);
        if (!bml_modrm_is_register_only(modrm) || (reg_opcode != 0U && reg_opcode != 5U)) {
            *out_code = bml_modrm_uses_rip_relative(modrm) ? "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" : "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
            *out_message = "Detour target prologue uses an unsupported immediate arithmetic form.";
            return -1;
        }
        *out_length = 3U;
        return 0;
    }

    if (op == 0x48U) {
        unsigned char next;
        unsigned char modrm;
        unsigned char reg_opcode;
        if (offset + 2U > limit) {
            *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
            *out_message = "Detour target prologue ended after a REX.W prefix.";
            return -1;
        }
        next = code[offset + 1U];
        if (next >= 0xb8U && next <= 0xbfU) {
            if (offset + 10U > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = "Detour target prologue ended in the middle of a supported movabs immediate instruction.";
                return -1;
            }
            *out_length = 10U;
            return 0;
        }
        if (next == 0x89U || next == 0x8bU) {
            if (offset + 3U > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = "Detour target prologue ended in the middle of a supported REX.W register mov instruction.";
                return -1;
            }
            modrm = code[offset + 2U];
            if (!bml_modrm_is_register_only(modrm)) {
                *out_code = bml_modrm_uses_rip_relative(modrm) ? "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" : "BML_DETOUR_MEMORY_OPERAND_UNSUPPORTED";
                *out_message = "Detour target prologue uses REX.W memory addressing that this substrate does not relocate.";
                return -1;
            }
            *out_length = 3U;
            return 0;
        }
        if (next == 0x83U) {
            if (offset + 4U > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = "Detour target prologue ended in the middle of a supported REX.W add/sub immediate instruction.";
                return -1;
            }
            modrm = code[offset + 2U];
            reg_opcode = (unsigned char)((modrm >> 3U) & 0x07U);
            if (!bml_modrm_is_register_only(modrm) || (reg_opcode != 0U && reg_opcode != 5U)) {
                *out_code = bml_modrm_uses_rip_relative(modrm) ? "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" : "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
                *out_message = "Detour target prologue uses an unsupported REX.W immediate arithmetic form.";
                return -1;
            }
            *out_length = 4U;
            return 0;
        }
        if (next == 0x81U) {
            if (offset + 7U > limit) {
                *out_code = "BML_DETOUR_TRUNCATED_INSTRUCTION";
                *out_message = "Detour target prologue ended in the middle of a supported REX.W add/sub imm32 instruction.";
                return -1;
            }
            modrm = code[offset + 2U];
            reg_opcode = (unsigned char)((modrm >> 3U) & 0x07U);
            if (!bml_modrm_is_register_only(modrm) || (reg_opcode != 0U && reg_opcode != 5U)) {
                *out_code = bml_modrm_uses_rip_relative(modrm) ? "BML_DETOUR_RIP_RELATIVE_RELOCATION_REQUIRED" : "BML_DETOUR_UNSUPPORTED_INSTRUCTION";
                *out_message = "Detour target prologue uses an unsupported REX.W imm32 arithmetic form.";
                return -1;
            }
            *out_length = 7U;
            return 0;
        }
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
    trampoline_length = patch_size + BML_DETOUR_PATCH_BYTES;
    trampoline = mmap(NULL, trampoline_length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (trampoline == MAP_FAILED) {
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_TRAMPOLINE_ALLOC_FAILED");
        bml_copy_string(error_message, error_message_size, "Executable trampoline allocation failed.");
        return -1;
    }

    memcpy(trampoline, original, patch_size);
    bml_write_abs_jump(trampoline + patch_size, (const unsigned char *)target + patch_size);
    __builtin___clear_cache((char *)trampoline, (char *)trampoline + trampoline_length);

    if (mprotect(trampoline, trampoline_length, PROT_READ | PROT_EXEC) != 0) {
        (void)munmap(trampoline, trampoline_length);
        bml_copy_string(error_code, error_code_size, "BML_DETOUR_TRAMPOLINE_PROTECT_FAILED");
        bml_copy_string(error_message, error_message_size, "Executable trampoline could not be made read-only/executable after construction.");
        return -1;
    }

    if (bml_page_span_for_patch(target, patch_size, &page_start, &page_span) != 0 ||
        mprotect((void *)page_start, page_span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        (void)munmap(trampoline, trampoline_length);
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
        (void)munmap(trampoline, trampoline_length);
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

__attribute__((visibility("default"))) int bml_hook_init(void) {
    const char *profile_dir;
    const char *runtime_manifest;
    const char *hook_manifest;
    const char *hook_library;
    const char *detour_self_test;
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
        bml_join_path(detour_self_test_report_path, sizeof(detour_self_test_report_path), profile_dir, BML_DETOUR_SELF_TEST_REPORT_RELATIVE_PATH) != 0) {
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
