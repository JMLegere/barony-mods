$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$hookRoot = Join-Path $repoRoot 'native\barony-modloader-hook'
$profileRoot = Join-Path $repoRoot '.tmp\windows-direct-selftest-profile'
$hookLibrary = Join-Path $hookRoot 'build\bml_hook_win.dll'
$fakeProvider = Join-Path $hookRoot 'build\bml_fake_provider_win.dll'
$hookManifest = Join-Path $hookRoot 'manifests\steam-371970-22630456-windows.json'

if (-not [Environment]::Is64BitProcess) {
    Write-Host 'SKIP: 64-bit PowerShell is required to load bml_hook_win.dll.'
    exit 0
}
if (-not (Test-Path $hookLibrary)) { throw "Missing hook DLL: $hookLibrary" }
if (-not (Test-Path $fakeProvider)) { throw "Missing fake provider DLL: $fakeProvider" }

Remove-Item -Recurse -Force $profileRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $profileRoot 'BaronyModLoader') | Out-Null
$runtimeManifest = Join-Path $profileRoot 'BaronyModLoader\runtime-manifest.json'
Set-Content -Encoding UTF8 $runtimeManifest '{"contract":{"id":"bml-runtime-contract","version":"0.1.0"},"app":{"id":"BaronyModLoader","version":"0.1.0"},"launch":{"profileId":"windows-direct-selftest","runtimeStrategy":"installed-binary-hook","gameVersionString":"v5.0.2","runtime":{"runtimeId":"barony-bml-runtime-windows-noop","runtimeVersion":"0.1.0"}},"mods":[{"id":"jml.windows_smoke","version":"0.1.0","capabilities":[{"id":"runtime_load_smoke","version":"0.1.0","required":true}],"modules":{}}]}'

$env:BML_PROFILE_DIR = $profileRoot
$env:BML_RUNTIME_MANIFEST = $runtimeManifest
$env:BML_HOOK_MANIFEST = $hookManifest
$env:BML_HOOK_LIBRARY = $hookLibrary
$env:BML_WINDOWS_DETOUR_SELF_TEST = '1'
$env:BML_WINDOWS_FAKE_STASH_SELF_TEST = '1'
$env:BML_WINDOWS_FAKE_PROVIDER_DLL = $fakeProvider

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class BmlHookWinDirectSelfTest {
    [DllImport(@"$hookLibrary", CallingConvention=CallingConvention.Winapi)]
    public static extern int bml_hook_init();
}
"@

$rc = [BmlHookWinDirectSelfTest]::bml_hook_init()
if ($rc -ne 0) { throw "bml_hook_init failed with exit code $rc" }

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\reports'
$detour = Get-Content -Raw (Join-Path $reportRoot 'windows-detour-self-test-report.json') | ConvertFrom-Json
$fake = Get-Content -Raw (Join-Path $reportRoot 'windows-fake-stash-detour-report.json') | ConvertFrom-Json
$runtime = Get-Content -Raw (Join-Path $reportRoot 'runtime-load-report.json') | ConvertFrom-Json

if ($detour.status -ne 'installed' -or $detour.before -ne 7 -or $detour.after -ne 11 -or $detour.trampoline -ne 7 -or -not $detour.callRelocated.installed -or $detour.callRelocated.before -ne 7 -or $detour.callRelocated.after -ne 11 -or $detour.callRelocated.trampoline -ne 7 -or -not $detour.ripRelocated.installed -or $detour.ripRelocated.before -ne 7 -or $detour.ripRelocated.after -ne 11 -or $detour.ripRelocated.trampoline -ne 7) {
    throw "Windows detour self-test failed validation: $($detour | ConvertTo-Json -Compress)"
}
if ($fake.status -ne 'installed' -or -not $fake.providerLoaded -or -not $fake.targetResolved -or -not $fake.replacementCallsOriginal -or $fake.before -ne 12884901916 -or $fake.after -ne 12884901929 -or $fake.trampoline -ne 12884901916) {
    throw "Windows fake Stash detour self-test failed validation: $($fake | ConvertTo-Json -Compress)"
}
if ($runtime.status -ne 'loaded' -or $runtime.errors.Count -ne 0) {
    throw "Runtime load report failed validation: $($runtime | ConvertTo-Json -Compress)"
}

Write-Host 'OK: Windows hook direct self-tests passed.'
