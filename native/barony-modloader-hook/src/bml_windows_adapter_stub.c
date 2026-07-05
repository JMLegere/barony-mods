/*
 * Windows adapter scaffold for BaronyModLoader.
 *
 * This file is intentionally excluded from the Linux Makefile target and is not
 * a playable Windows runtime. It records the native boundary that a future
 * Windows implementation must satisfy before the runtime registry can advertise
 * windows-x86_64 support.
 *
 * Intended artifact contract:
 *   - bml-win-launcher.exe launches the installed Barony executable
 *     (barony.exe) suspended or otherwise controllably via CreateProcessW.
 *   - barony_bml.dll is the Windows hook payload loaded into that process.
 *   - The launcher injects barony_bml.dll with LoadLibraryW using remote-process
 *     memory and thread APIs.
 *   - In-process detours use page-protection and instruction-cache APIs instead
 *     of POSIX mmap/mprotect assumptions.
 *
 * Required Win32 API surface for the future implementation:
 *   - CreateProcessW
 *   - LoadLibraryW
 *   - VirtualAllocEx
 *   - WriteProcessMemory
 *   - CreateRemoteThread
 *   - VirtualProtect
 *   - FlushInstructionCache
 *
 * Fail-closed rule:
 *   Do not add this file to the default Linux SOURCES, ship barony_bml.dll, or
 *   register a playable Windows adapter until a real Windows build manifest,
 *   launcher/DLL artifacts, and live Windows runtime evidence exist.
 */

#if defined(_WIN32) && !defined(BML_WINDOWS_ADAPTER_IMPLEMENTED)
#error "Windows adapter is scaffold-only: implement bml-win-launcher.exe + barony_bml.dll and verify on Windows before building."
#endif

void bml_windows_adapter_scaffold_only(void) {
    /* Deliberately empty: this is a contract marker, not an injection runtime. */
}
