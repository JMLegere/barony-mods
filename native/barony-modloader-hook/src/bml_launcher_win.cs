using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

internal static class BmlLauncherWin
{
    private const uint CREATE_SUSPENDED = 0x00000004;
    private const uint CREATE_UNICODE_ENVIRONMENT = 0x00000400;
    private const uint MEM_COMMIT = 0x1000;
    private const uint MEM_RESERVE = 0x2000;
    private const uint PAGE_READWRITE = 0x04;
    private const uint WAIT_OBJECT_0 = 0x00000000;
    private const uint INFINITE = 0xFFFFFFFF;
    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001;
    private const uint TH32CS_SNAPMODULE = 0x00000008;
    private const uint TH32CS_SNAPMODULE32 = 0x00000010;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct STARTUPINFO
    {
        public uint cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public uint dwX;
        public uint dwY;
        public uint dwXSize;
        public uint dwYSize;
        public uint dwXCountChars;
        public uint dwYCountChars;
        public uint dwFillAttribute;
        public uint dwFlags;
        public ushort wShowWindow;
        public ushort cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct PROCESS_INFORMATION
    {
        public IntPtr hProcess;
        public IntPtr hThread;
        public uint dwProcessId;
        public uint dwThreadId;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct MODULEENTRY32
    {
        public uint dwSize;
        public uint th32ModuleID;
        public uint th32ProcessID;
        public uint GlblcntUsage;
        public uint ProccntUsage;
        public IntPtr modBaseAddr;
        public uint modBaseSize;
        public IntPtr hModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string szModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szExePath;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateProcessW(
        string lpApplicationName,
        string lpCommandLine,
        IntPtr lpProcessAttributes,
        IntPtr lpThreadAttributes,
        bool bInheritHandles,
        uint dwCreationFlags,
        IntPtr lpEnvironment,
        string lpCurrentDirectory,
        ref STARTUPINFO lpStartupInfo,
        out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, UIntPtr dwSize, uint flAllocationType, uint flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, UIntPtr nSize, out UIntPtr lpNumberOfBytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr GetModuleHandleW([MarshalAs(UnmanagedType.LPWStr)] string lpModuleName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, [MarshalAs(UnmanagedType.LPStr)] string lpProcName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true, EntryPoint = "LoadLibraryExW")]
    private static extern IntPtr LoadLibraryForExportLookup(string lpLibFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, UIntPtr dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, out uint lpThreadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetExitCodeThread(IntPtr hThread, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint ResumeThread(IntPtr hThread);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetExitCodeProcess(IntPtr hProcess, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool TerminateProcess(IntPtr hProcess, uint uExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateToolhelp32Snapshot(uint dwFlags, uint th32ProcessID);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool Module32FirstW(IntPtr hSnapshot, ref MODULEENTRY32 lpme);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool Module32NextW(IntPtr hSnapshot, ref MODULEENTRY32 lpme);

    private static int Main(string[] args)
    {
        string target = Environment.GetEnvironmentVariable("BML_TARGET_EXECUTABLE");
        string hook = Environment.GetEnvironmentVariable("BML_HOOK_LIBRARY");
        string profileDir = Environment.GetEnvironmentVariable("BML_PROFILE_DIR");
        string runtimeManifest = Environment.GetEnvironmentVariable("BML_RUNTIME_MANIFEST");
        bool validateInjectionOnly = string.Equals(Environment.GetEnvironmentVariable("BML_VALIDATE_INJECTION_ONLY"), "1", StringComparison.Ordinal);

        try
        {
            if (string.IsNullOrWhiteSpace(target)) throw new InvalidOperationException("BML_TARGET_EXECUTABLE is required.");
            if (string.IsNullOrWhiteSpace(hook)) throw new InvalidOperationException("BML_HOOK_LIBRARY is required.");
            if (!File.Exists(target)) throw new FileNotFoundException("Target executable not found.", target);
            if (!File.Exists(hook)) throw new FileNotFoundException("Hook library not found.", hook);
            string cwd = Path.GetDirectoryName(Path.GetFullPath(target));
            if (string.IsNullOrWhiteSpace(cwd)) cwd = Environment.CurrentDirectory;

            string commandLine = Quote(target) + BuildArgs(args);
            STARTUPINFO startup = new STARTUPINFO();
            startup.cb = (uint)Marshal.SizeOf(typeof(STARTUPINFO));
            PROCESS_INFORMATION process;
            if (!CreateProcessW(target, commandLine, IntPtr.Zero, IntPtr.Zero, false, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, IntPtr.Zero, cwd, ref startup, out process))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateProcessW failed.");
            }

            UIntPtr loadResult;
            bool resumed = false;
            try
            {
                loadResult = InjectLoadLibrary(process.hProcess, process.dwProcessId, hook);
                uint initResult = CallRemoteExport(process.hProcess, loadResult, hook, "bml_hook_init");
                if (initResult != 0) throw new InvalidOperationException("Remote bml_hook_init failed with code " + initResult.ToString(System.Globalization.CultureInfo.InvariantCulture) + ".");
                if (validateInjectionOnly)
                {
                    TerminateProcess(process.hProcess, 0);
                    WriteReport(profileDir, true, target, hook, runtimeManifest, process.dwProcessId, loadResult, null);
                    return 0;
                }
                uint resumeResult = ResumeThread(process.hThread);
                if (resumeResult == 0xFFFFFFFF) throw new Win32Exception(Marshal.GetLastWin32Error(), "ResumeThread failed.");
                resumed = true;
                WaitForSingleObject(process.hProcess, INFINITE);
                uint exitCode;
                if (!GetExitCodeProcess(process.hProcess, out exitCode)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetExitCodeProcess failed.");
                WriteReport(profileDir, true, target, hook, runtimeManifest, process.dwProcessId, loadResult, null);
                return unchecked((int)exitCode);
            }
            catch
            {
                if (!resumed && process.hProcess != IntPtr.Zero)
                {
                    TerminateProcess(process.hProcess, 1);
                }
                throw;
            }
            finally
            {
                if (process.hThread != IntPtr.Zero) CloseHandle(process.hThread);
                if (process.hProcess != IntPtr.Zero) CloseHandle(process.hProcess);
            }
        }
        catch (Exception ex)
        {
            WriteReport(profileDir, false, target, hook, runtimeManifest, 0, UIntPtr.Zero, ex);
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }

    private static UIntPtr InjectLoadLibrary(IntPtr process, uint processId, string hook)
    {
        byte[] dllPath = Encoding.Unicode.GetBytes(hook + "\0");
        IntPtr remote = VirtualAllocEx(process, IntPtr.Zero, (UIntPtr)dllPath.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remote == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "VirtualAllocEx failed.");
        UIntPtr written;
        if (!WriteProcessMemory(process, remote, dllPath, (UIntPtr)dllPath.Length, out written) || written.ToUInt64() != (ulong)dllPath.Length)
        {
            throw new Win32Exception(Marshal.GetLastWin32Error(), "WriteProcessMemory failed.");
        }
        IntPtr kernel32 = GetModuleHandleW("kernel32.dll");
        if (kernel32 == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetModuleHandleW(kernel32.dll) failed.");
        IntPtr loadLibrary = GetProcAddress(kernel32, "LoadLibraryW");
        if (loadLibrary == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetProcAddress(LoadLibraryW) failed.");
        uint threadId;
        IntPtr thread = CreateRemoteThread(process, IntPtr.Zero, UIntPtr.Zero, loadLibrary, remote, 0, out threadId);
        if (thread == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateRemoteThread failed.");
        try
        {
            if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) throw new Win32Exception(Marshal.GetLastWin32Error(), "WaitForSingleObject failed.");
            uint exitCode;
            if (!GetExitCodeThread(thread, out exitCode)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetExitCodeThread failed.");
            if (exitCode == 0) throw new InvalidOperationException("Remote LoadLibraryW returned NULL.");
            return FindRemoteModuleBase(processId, Path.GetFileName(hook));
        }
        finally
        {
            CloseHandle(thread);
        }
    }

    private static UIntPtr FindRemoteModuleBase(uint processId, string moduleName)
    {
        string expectedName = Path.GetFileName(moduleName);
        for (int attempt = 0; attempt < 50; attempt++)
        {
            IntPtr snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
            if (snapshot != IntPtr.Zero && snapshot.ToInt64() != -1)
            {
                try
                {
                    MODULEENTRY32 entry = new MODULEENTRY32();
                    entry.dwSize = (uint)Marshal.SizeOf(typeof(MODULEENTRY32));
                    if (Module32FirstW(snapshot, ref entry))
                    {
                        do
                        {
                            string entryName = Path.GetFileName(entry.szModule ?? string.Empty);
                            string entryPathName = Path.GetFileName(entry.szExePath ?? string.Empty);
                            if (string.Equals(entryName, expectedName, StringComparison.OrdinalIgnoreCase) ||
                                string.Equals(entryPathName, expectedName, StringComparison.OrdinalIgnoreCase))
                            {
                                return (UIntPtr)unchecked((ulong)entry.modBaseAddr.ToInt64());
                            }
                            entry.dwSize = (uint)Marshal.SizeOf(typeof(MODULEENTRY32));
                        }
                        while (Module32NextW(snapshot, ref entry));
                    }
                }
                finally
                {
                    CloseHandle(snapshot);
                }
            }
            System.Threading.Thread.Sleep(20);
        }
        throw new InvalidOperationException("Could not locate remote module base for " + expectedName + ".");
    }


    private static uint CallRemoteExport(IntPtr process, UIntPtr remoteModule, string hook, string exportName)
    {
        IntPtr localModule = LoadLibraryForExportLookup(hook, IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
        if (localModule == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "LoadLibraryExW for local export inspection failed.");
        try
        {
            IntPtr localExport = GetProcAddress(localModule, exportName);
            if (localExport == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetProcAddress(" + exportName + ") failed.");
            long exportRva = localExport.ToInt64() - localModule.ToInt64();
            IntPtr remoteExport = new IntPtr(unchecked((long)remoteModule.ToUInt64()) + exportRva);
            uint threadId;
            IntPtr thread = CreateRemoteThread(process, IntPtr.Zero, UIntPtr.Zero, remoteExport, IntPtr.Zero, 0, out threadId);
            if (thread == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "CreateRemoteThread for " + exportName + " failed.");
            try
            {
                if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) throw new Win32Exception(Marshal.GetLastWin32Error(), "WaitForSingleObject for " + exportName + " failed.");
                uint exitCode;
                if (!GetExitCodeThread(thread, out exitCode)) throw new Win32Exception(Marshal.GetLastWin32Error(), "GetExitCodeThread for " + exportName + " failed.");
                return exitCode;
            }
            finally
            {
                CloseHandle(thread);
            }
        }
        finally
        {
            FreeLibrary(localModule);
        }
    }

    private static string BuildArgs(string[] args)
    {
        if (args == null || args.Length == 0) return string.Empty;
        StringBuilder builder = new StringBuilder();
        foreach (string arg in args)
        {
            builder.Append(' ');
            builder.Append(Quote(arg));
        }
        return builder.ToString();
    }

    private static string Quote(string value)
    {
        if (value == null) return "\"\"";
        StringBuilder builder = new StringBuilder();
        builder.Append('"');
        int backslashes = 0;
        foreach (char ch in value)
        {
            if (ch == '\\')
            {
                backslashes++;
                continue;
            }
            if (ch == '"')
            {
                builder.Append('\\', backslashes * 2 + 1);
                builder.Append('"');
                backslashes = 0;
                continue;
            }
            if (backslashes > 0)
            {
                builder.Append('\\', backslashes);
                backslashes = 0;
            }
            builder.Append(ch);
        }
        if (backslashes > 0)
        {
            builder.Append('\\', backslashes * 2);
        }
        builder.Append('"');
        return builder.ToString();
    }

    private static void WriteReport(string profileDir, bool success, string target, string hook, string runtimeManifest, uint processId, UIntPtr loadResult, Exception error)
    {
        if (string.IsNullOrWhiteSpace(profileDir)) return;
        string reports = Path.Combine(profileDir, "BaronyModLoader", "reports");
        Directory.CreateDirectory(reports);
        string path = Path.Combine(reports, "launcher-injection-report.json");
        string json = "{\n" +
            "  \"schemaVersion\": \"0.1.0\",\n" +
            "  \"status\": \"" + (success ? "loaded" : "failed") + "\",\n" +
            "  \"targetExecutable\": " + JsonString(target) + ",\n" +
            "  \"hookLibrary\": " + JsonString(hook) + ",\n" +
            "  \"runtimeManifest\": " + JsonString(runtimeManifest) + ",\n" +
            "  \"processId\": " + processId.ToString(System.Globalization.CultureInfo.InvariantCulture) + ",\n" +
            "  \"loadLibraryResult\": " + JsonString(loadResult == UIntPtr.Zero ? null : "0x" + loadResult.ToUInt64().ToString("x")) + ",\n" +
            "  \"error\": " + JsonString(error == null ? null : error.Message) + "\n" +
            "}\n";
        File.WriteAllText(path, json, new UTF8Encoding(false));
    }

    private static string JsonString(string value)
    {
        if (value == null) return "null";
        return "\"" + value.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\r", "\\r").Replace("\n", "\\n") + "\"";
    }
}
