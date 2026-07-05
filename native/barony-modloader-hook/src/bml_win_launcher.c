/*
 * Fail-closed Windows launcher adapter for BaronyModLoader.
 *
 * The future production adapter will own the CreateProcessW + LoadLibraryW
 * path for barony.exe. This buildable artifact intentionally does not launch,
 * inject into, or modify a game process. Only --self-test is allowed to load the
 * sibling barony_bml.dll in-process and verify that the DLL reports the same
 * unsupported/fail-closed status.
 */

#if !defined(_WIN32)
#error "bml_win_launcher.c must be built with winegcc or a Windows compiler."
#endif

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define BML_WINDOWS_DLL_NAME L"barony_bml.dll"
#define BML_WINDOWS_STATUS_UNSUPPORTED 42

typedef int (__cdecl *BmlWindowsHookSelfTest)(void);
typedef int (__cdecl *BmlWindowsAdapterStatus)(void);

static void bml_windows_print_last_error(const char *operation) {
    DWORD error_code = GetLastError();
    fprintf(stderr, "bml-win-launcher: %s failed with Win32 error %lu\n", operation, (unsigned long)error_code);
}

static int bml_windows_get_sibling_dll_path(wchar_t *dll_path, DWORD dll_path_count) {
    DWORD written = GetModuleFileNameW(NULL, dll_path, dll_path_count);
    wchar_t *slash = NULL;
    wchar_t *forward_slash = NULL;
    size_t base_length = 0;
    size_t dll_name_length = wcslen(BML_WINDOWS_DLL_NAME);

    if (written == 0 || written >= dll_path_count) {
        bml_windows_print_last_error("GetModuleFileNameW");
        return 1;
    }

    slash = wcsrchr(dll_path, L'\\');
    forward_slash = wcsrchr(dll_path, L'/');
    if (slash == NULL || (forward_slash != NULL && forward_slash > slash)) {
        slash = forward_slash;
    }

    if (slash != NULL) {
        slash[1] = L'\0';
    } else {
        dll_path[0] = L'\0';
    }

    base_length = wcslen(dll_path);
    if (base_length + dll_name_length >= dll_path_count) {
        fprintf(stderr, "bml-win-launcher: sibling DLL path is too long\n");
        return 1;
    }

    wcscat(dll_path, BML_WINDOWS_DLL_NAME);
    return 0;
}

static int bml_windows_run_self_test(void) {
    wchar_t dll_path[MAX_PATH];
    HMODULE hook_dll = NULL;
    BmlWindowsAdapterStatus status = NULL;
    BmlWindowsHookSelfTest self_test = NULL;
    int status_code = 0;

    if (bml_windows_get_sibling_dll_path(dll_path, (DWORD)(sizeof(dll_path) / sizeof(dll_path[0]))) != 0) {
        return 1;
    }

    hook_dll = LoadLibraryW(dll_path);
    if (hook_dll == NULL) {
        /*
         * winegcc emits Winelib DLL payloads as .dll.so files and its launcher
         * script exposes the build directory through WINEDLLPATH. A bare module
         * load keeps the self-test working there while preserving the real
         * Windows sibling-path attempt above.
         */
        hook_dll = LoadLibraryW(BML_WINDOWS_DLL_NAME);
    }
    if (hook_dll == NULL) {
        bml_windows_print_last_error("LoadLibraryW(barony_bml.dll)");
        return 1;
    }

    status = (BmlWindowsAdapterStatus)(void *)GetProcAddress(hook_dll, "bml_windows_adapter_status");
    if (status == NULL) {
        bml_windows_print_last_error("GetProcAddress(bml_windows_adapter_status)");
        FreeLibrary(hook_dll);
        return 1;
    }

    status_code = status();
    if (status_code != BML_WINDOWS_STATUS_UNSUPPORTED) {
        fprintf(stderr, "bml-win-launcher: DLL reported unexpected adapter status %d\n", status_code);
        FreeLibrary(hook_dll);
        return 1;
    }

    self_test = (BmlWindowsHookSelfTest)(void *)GetProcAddress(hook_dll, "bml_windows_hook_self_test");
    if (self_test == NULL) {
        bml_windows_print_last_error("GetProcAddress(bml_windows_hook_self_test)");
        FreeLibrary(hook_dll);
        return 1;
    }

    status_code = self_test();
    FreeLibrary(hook_dll);

    if (status_code != BML_WINDOWS_STATUS_UNSUPPORTED) {
        fprintf(stderr, "bml-win-launcher: DLL self-test reported unexpected status %d\n", status_code);
        return 1;
    }

    puts("bml-win-launcher: self-test passed; Windows adapter remains unsupported_fail_closed.");
    return 0;
}

static int bml_windows_fail_closed(void) {
    fputs("bml-win-launcher: Windows launch/injection is disabled. ", stderr);
    fputs("This artifact is a buildable fail-closed adapter only; ", stderr);
    fputs("barony.exe CreateProcessW + LoadLibraryW injection will remain unavailable until live Windows Barony verification exists.\n", stderr);
    return 2;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        return bml_windows_run_self_test();
    }

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("usage: bml-win-launcher.exe --self-test");
        puts("real Barony launch/injection intentionally fails closed until verified on Windows");
        return 0;
    }

    return bml_windows_fail_closed();
}
