/*
 * Buildable fail-closed Windows hook payload for BaronyModLoader.
 *
 * This DLL is intentionally not a playable runtime. It exists so the Windows
 * adapter artifact contract can be built and smoke-tested with winegcc while
 * remaining safe: no detours are installed, no process memory is patched, and
 * every callable status reports unsupported until a real Windows Barony runtime
 * has live verification evidence.
 */

#if !defined(_WIN32)
#error "bml_windows_adapter_stub.c must be built with winegcc or a Windows compiler."
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#define BML_WINDOWS_CONTRACT_ID "bml-runtime-contract"
#define BML_WINDOWS_CONTRACT_VERSION "0.1.0"
#define BML_WINDOWS_RUNTIME_ID "barony-bml-native-hook"
#define BML_WINDOWS_RUNTIME_VERSION "0.1.0"
#define BML_WINDOWS_PLATFORM_ID "windows-x86_64"
#define BML_WINDOWS_ADAPTER_ID "windows-createprocess-loadlibrary"
#define BML_WINDOWS_DLL_ARTIFACT "barony_bml.dll"
#define BML_WINDOWS_STATUS_UNSUPPORTED 42
#define BML_WINDOWS_SELF_TEST_REPORT_ENV "BML_WINDOWS_SELF_TEST_REPORT"
#define BML_WINDOWS_SELF_TEST_REPORT_DEFAULT "build\\windows-adapter-self-test-report.json"

#if defined(__linux__)
#define BML_WINDOWS_EXPORT __attribute__((visibility("default")))
#else
#define BML_WINDOWS_EXPORT __declspec(dllexport)
#endif

static const char *bml_windows_status_text(void) {
    return "unsupported_fail_closed";
}

static const char *bml_windows_status_reason(void) {
    return "Windows Barony launch/injection and gameplay hooks are disabled until a real Windows Barony runtime is live-verified.";
}

static void bml_windows_resolve_report_path(char *buffer, DWORD buffer_size) {
    DWORD written = GetEnvironmentVariableA(BML_WINDOWS_SELF_TEST_REPORT_ENV, buffer, buffer_size);

    if (written == 0 || written >= buffer_size) {
        snprintf(buffer, buffer_size, "%s", BML_WINDOWS_SELF_TEST_REPORT_DEFAULT);
    }
}

static int bml_windows_write_self_test_report(void) {
    char report_path[MAX_PATH * 4];
    time_t now = time(NULL);
    FILE *report = NULL;

    bml_windows_resolve_report_path(report_path, (DWORD)sizeof(report_path));
    report = fopen(report_path, "w");
    if (report == NULL) {
        return 1;
    }

    fprintf(report, "{\n");
    fprintf(report, "  \"contractId\": \"%s\",\n", BML_WINDOWS_CONTRACT_ID);
    fprintf(report, "  \"contractVersion\": \"%s\",\n", BML_WINDOWS_CONTRACT_VERSION);
    fprintf(report, "  \"runtimeId\": \"%s\",\n", BML_WINDOWS_RUNTIME_ID);
    fprintf(report, "  \"runtimeVersion\": \"%s\",\n", BML_WINDOWS_RUNTIME_VERSION);
    fprintf(report, "  \"platformId\": \"%s\",\n", BML_WINDOWS_PLATFORM_ID);
    fprintf(report, "  \"adapterId\": \"%s\",\n", BML_WINDOWS_ADAPTER_ID);
    fprintf(report, "  \"artifact\": \"%s\",\n", BML_WINDOWS_DLL_ARTIFACT);
    fprintf(report, "  \"status\": \"%s\",\n", bml_windows_status_text());
    fprintf(report, "  \"playable\": false,\n");
    fprintf(report, "  \"selfTestStatusCode\": %d,\n", BML_WINDOWS_STATUS_UNSUPPORTED);
    fprintf(report, "  \"timestampUnix\": %lld,\n", (long long)now);
    fprintf(report, "  \"reason\": \"%s\"\n", bml_windows_status_reason());
    fprintf(report, "}\n");

    if (fclose(report) != 0) {
        return 1;
    }

    return 0;
}

BML_WINDOWS_EXPORT int bml_windows_adapter_status(void) {
    return BML_WINDOWS_STATUS_UNSUPPORTED;
}

BML_WINDOWS_EXPORT int bml_windows_hook_self_test(void) {
    if (bml_windows_write_self_test_report() != 0) {
        return 1;
    }

    OutputDebugStringA("BaronyModLoader Windows hook self-test reported unsupported_fail_closed.\n");
    return BML_WINDOWS_STATUS_UNSUPPORTED;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance;
    (void)reason;
    (void)reserved;
    return TRUE;
}
