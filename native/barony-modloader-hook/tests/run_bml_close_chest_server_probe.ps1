$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$probeSlug = 'windows-close-chest-server-probe'
$registry = if ($env:BML_TEST_REGISTRY) { $env:BML_TEST_REGISTRY } else { Join-Path $repoRoot ('.tmp\' + $probeSlug + '-' + $PID + '-registry.json') }
$profileRoot = if ($env:BML_TEST_PROFILE_ROOT) { $env:BML_TEST_PROFILE_ROOT } else { Join-Path $repoRoot ('.tmp\' + $probeSlug + '-' + $PID) }
$runtimeInfo = Join-Path $repoRoot 'framework\BaronyModLoader\fixtures\runtime-info.installed-hook.windows-noop.json'
$hookLibrary = if ($env:BML_TEST_HOOK_DLL) { $env:BML_TEST_HOOK_DLL } else { Join-Path $repoRoot 'native\barony-modloader-hook\build\bml_hook_win.dll' }
$hookManifest = Join-Path $repoRoot 'native\barony-modloader-hook\manifests\steam-371970-22630456-windows.json'
$launcherHelper = if ($env:BML_TEST_LAUNCHER_HELPER) { $env:BML_TEST_LAUNCHER_HELPER } else { Join-Path $repoRoot 'native\barony-modloader-hook\build\bml_launcher_win.exe' }
$app = Join-Path $repoRoot 'framework\BaronyModLoader\app\barony_mod_loader.py'
$smokePackage = Join-Path $repoRoot 'framework\BaronyModLoader\fixtures\windows-smoke-package'
$steamExe = if ($env:BML_TEST_STEAM_EXE) { $env:BML_TEST_STEAM_EXE } else { 'C:\Program Files (x86)\Steam\steamapps\common\Barony\barony.exe' }
$defaultPython = 'C:\Program Files (x86)\GOG Galaxy\python\python.exe'
$python = if ($env:BML_TEST_PYTHON) { $env:BML_TEST_PYTHON } elseif (Test-Path $defaultPython) { $defaultPython } elseif (Get-Command python -ErrorAction SilentlyContinue) { (Get-Command python).Source } else { 'python' }
$closeChestServerRva = if ($env:BML_TEST_CLOSE_CHEST_SERVER_RVA) { $env:BML_TEST_CLOSE_CHEST_SERVER_RVA } else { '3035632' }

if (-not [Environment]::Is64BitProcess) { Write-Host 'SKIP: 64-bit PowerShell is required for the local install probe.'; exit 0 }
if (-not (Test-Path $hookLibrary)) { Write-Host "SKIP: Hook DLL is missing at $hookLibrary."; exit 0 }
if (-not (Test-Path $launcherHelper)) { Write-Host "SKIP: Launcher helper is missing at $launcherHelper."; exit 0 }
if (-not (Test-Path $steamExe)) { Write-Host "SKIP: Steam Barony executable is missing at $steamExe."; exit 0 }
if (-not (Test-Path $python)) { Write-Host "SKIP: Expected Python host is missing at $python."; exit 0 }
if (-not (Test-Path $app)) { Write-Host "SKIP: barony_mod_loader.py is missing at $app."; exit 0 }

Remove-Item -Recurse -Force $profileRoot -ErrorAction SilentlyContinue
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_INSTALL_GET_ITEM_PASSTHROUGH', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_ADD_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_GET_CHEST_LIST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ITEM_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_ASSIGN_ACTIONS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ENTITY_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_SET_SPRITE_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_INIT_CLASS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_SUMMON_NO_SMOKE_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_VALIDATE_INJECTION_ONLY', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_CLOSE_CHEST_SERVER_PROBE_RVA', $null, 'Process')
Remove-Item -Force $registry -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $profileRoot | Out-Null
$env:BML_CLOSE_CHEST_SERVER_PROBE_RVA = [string]$closeChestServerRva
& $python $app runtime register --registry $registry --id barony-bml-runtime-windows-noop --runtime-info $runtimeInfo --steam-executable $steamExe --hook-library $hookLibrary --launcher-helper $launcherHelper --hook-manifest $hookManifest --steam-build-id 22630456
if ($LASTEXITCODE -ne 0) { throw 'runtime register failed' }
& $python $app profile create $profileRoot --id windows-validation --steam --runtime-info $runtimeInfo
if ($LASTEXITCODE -ne 0) { throw 'profile create failed' }
& $python $app launch $profileRoot --package $smokePackage --registry $registry --runtime barony-bml-runtime-windows-noop
if ($LASTEXITCODE -ne 0) { throw 'launch failed' }
& $python $app runtime report (Join-Path $profileRoot 'BaronyModLoader\reports\runtime-load-report.json')
if ($LASTEXITCODE -ne 0) { throw 'runtime report failed' }

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\reports'
$probe = Get-Content -Raw (Join-Path $reportRoot 'windows-close-chest-server-probe-report.json') | ConvertFrom-Json
$runtime = Get-Content -Raw (Join-Path $reportRoot 'runtime-load-report.json') | ConvertFrom-Json
if ($probe.rva -ne 3035632 -or -not $probe.codeViewMatch -or -not $probe.prologueMatch) {
    throw "closeChestServer install probe identity/prologue validation failed: $($probe | ConvertTo-Json -Compress)"
}
if ($runtime.status -eq 'loaded' -and $probe.installed -and $probe.replacementCalls -eq 0) {
    Write-Host 'OK: Windows closeChestServer install-only probe passed.'
    exit 0
}
if ($runtime.status -eq 'failed' -and ($runtime.errors | ForEach-Object code) -contains 'BML_WINDOWS_CLOSE_CHEST_SERVER_PROBE_INSTALL_FAILED') {
    Write-Host 'OK: Windows closeChestServer install-only probe failed with recorded install diagnostics.'
    exit 0
}
throw "Unexpected closeChestServer probe outcome: probe=$($probe | ConvertTo-Json -Compress) runtime=$($runtime | ConvertTo-Json -Compress)"
