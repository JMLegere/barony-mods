#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define BML_REPORT_DIR_RELATIVE_PATH "BaronyModLoader/reports"
#define BML_MAX_ERRORS 8
#define BML_MAX_TEXT 256
#define BML_MAX_MANIFEST_BYTES (1024U * 1024U)

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

static int g_bml_initialized = 0;
static int g_bml_init_result = 1;

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

__attribute__((visibility("default"))) int bml_hook_init(void) {
    const char *profile_dir;
    const char *runtime_manifest;
    const char *hook_manifest;
    const char *hook_library;
    BmlError errors[BML_MAX_ERRORS];
    size_t error_count = 0U;
    BmlReportInfo info;
    char report_dir[PATH_MAX];
    char report_path[PATH_MAX];
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
        bml_join_path(report_path, sizeof(report_path), profile_dir, BML_REPORT_RELATIVE_PATH) != 0) {
        free(runtime_json);
        g_bml_init_result = 1;
        return g_bml_init_result;
    }

    if (bml_mkdir_p(report_dir) != 0 || bml_write_report(report_path, &info, errors, error_count) != 0) {
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
