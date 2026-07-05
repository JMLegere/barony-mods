$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$probeSlug = 'windows-stash-core-behavior'
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

if (-not [Environment]::Is64BitProcess) { Write-Host 'SKIP: 64-bit PowerShell is required for the local install probe.'; exit 0 }
if (-not (Test-Path $hookLibrary)) { Write-Host "SKIP: Hook DLL is missing at $hookLibrary."; exit 0 }
if (-not (Test-Path $launcherHelper)) { Write-Host "SKIP: Launcher helper is missing at $launcherHelper."; exit 0 }
if (-not (Test-Path $steamExe)) { Write-Host "SKIP: Steam Barony executable is missing at $steamExe."; exit 0 }
if (-not (Test-Path $python)) { Write-Host "SKIP: Expected Python host is missing at $python."; exit 0 }
if (-not (Test-Path $app)) { Write-Host "SKIP: barony_mod_loader.py is missing at $app."; exit 0 }

Remove-Item -Recurse -Force $profileRoot -ErrorAction SilentlyContinue
Remove-Item -Force $registry -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $profileRoot | Out-Null
$clear = @(
  'BML_VALIDATE_INJECTION_ONLY',
  'BML_STASH_INSTALL_GET_ITEM_PASSTHROUGH',
  'BML_ADD_ITEM_VOID_CHEST_PROBE_RVA',
  'BML_GET_CHEST_LIST_PROBE_RVA',
  'BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA',
  'BML_CLOSE_CHEST_SERVER_PROBE_RVA',
  'BML_NEW_ITEM_PROBE_RVA',
  'BML_ASSIGN_ACTIONS_PROBE_RVA',
  'BML_NEW_ENTITY_PROBE_RVA',
  'BML_SET_SPRITE_PROBE_RVA',
  'BML_DO_NEW_GAME_PROBE_RVA',
  'BML_INIT_CLASS_PROBE_RVA',
  'BML_SUMMON_NO_SMOKE_PROBE_RVA',
  'BML_STASH_PLACEMENT_DISCOVERY',
  'BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA',
  'BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA',
  'BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA',
  'BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR'
)
foreach ($name in $clear) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
$env:BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR = '1'

& $python $app runtime register --registry $registry --id barony-bml-runtime-windows-noop --runtime-info $runtimeInfo --steam-executable $steamExe --hook-library $hookLibrary --launcher-helper $launcherHelper --hook-manifest $hookManifest --steam-build-id 22630456
if ($LASTEXITCODE -ne 0) { throw 'runtime register failed' }
& $python $app profile create $profileRoot --id windows-validation --steam --runtime-info $runtimeInfo
if ($LASTEXITCODE -ne 0) { throw 'profile create failed' }
[Environment]::SetEnvironmentVariable('BML_VALIDATE_INJECTION_ONLY', '1', 'Process')
$env:BML_VALIDATE_INJECTION_ONLY = '1'
[Environment]::SetEnvironmentVariable('BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR', '1', 'Process')
$env:BML_STASH_ENABLE_EXPERIMENTAL_CORE_BEHAVIOR = '1'
& $python $app launch $profileRoot --package $smokePackage --registry $registry --runtime barony-bml-runtime-windows-noop
if ($LASTEXITCODE -ne 0) { throw 'launch failed' }

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\\reports'
$coreReportPath = Join-Path $reportRoot 'stash-core-behavior-report.json'
if (-not (Test-Path $coreReportPath)) { throw 'stash-core-behavior-report.json was not written' }
& $python $app runtime report (Join-Path $reportRoot 'runtime-load-report.json')
if ($LASTEXITCODE -ne 0) { throw 'runtime report failed' }

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\\reports'
if (-not (Test-Path $coreReportPath)) { throw 'stash-core-behavior-report.json was not written' }
$core = Get-Content -Raw $coreReportPath | ConvertFrom-Json
$runtime = Get-Content -Raw (Join-Path $reportRoot 'runtime-load-report.json') | ConvertFrom-Json
if ($core.status -ne 'installed') {
  throw "Windows stash core behavior install failed: $($core | ConvertTo-Json -Compress) runtime=$($runtime | ConvertTo-Json -Compress)"
}
if ($core.targets.Count -ne 7) { throw "Expected 7 stash core targets, saw $($core.targets.Count)" }
if (($core.targets | Where-Object { -not $_.installed }).Count -ne 0) { throw "Not all stash core targets installed: $($core.targets | ConvertTo-Json -Compress)" }
if ($runtime.status -ne 'loaded' -or $runtime.errors.Count -ne 0) {
  throw "Runtime load report failed validation: $($runtime | ConvertTo-Json -Compress)"
}
Write-Host 'OK: Windows stash core behavior install probe passed.'
