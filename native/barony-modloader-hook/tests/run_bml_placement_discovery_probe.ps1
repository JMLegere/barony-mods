$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$probeSlug = 'windows-placement-discovery-probe'
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
$quickstartClass = if ($env:BML_TEST_QUICKSTART) { $env:BML_TEST_QUICKSTART } else { 'barbarian' }
$assignRva = $env:BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA
$newEntityRva = $env:BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA
$setSpriteRva = $env:BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA
$mapName = $env:BML_TEST_MAP
$doNewGameRva = $env:BML_TEST_DO_NEW_GAME_RVA
$forceShoppingSpree = $env:BML_TEST_FORCE_SHOPPING_SPREE

if (-not $assignRva -or -not $newEntityRva -or -not $setSpriteRva) { Write-Host 'SKIP: Placement discovery probe requires explicit assign/newEntity/setSprite RVAs.'; exit 0 }
if ($forceShoppingSpree -and -not $doNewGameRva) { Write-Host 'SKIP: Shopping Spree placement probe requires BML_TEST_DO_NEW_GAME_RVA.'; exit 0 }
if (-not [Environment]::Is64BitProcess) { Write-Host 'SKIP: 64-bit PowerShell is required for placement discovery.'; exit 0 }
if (-not (Test-Path $hookLibrary)) { Write-Host "SKIP: Hook DLL is missing at $hookLibrary."; exit 0 }
if (-not (Test-Path $launcherHelper)) { Write-Host "SKIP: Launcher helper is missing at $launcherHelper."; exit 0 }
if (-not (Test-Path $steamExe)) { Write-Host "SKIP: Steam Barony executable is missing at $steamExe."; exit 0 }
if (-not (Test-Path $python)) { Write-Host "SKIP: Expected Python host is missing at $python."; exit 0 }
if (-not (Test-Path $app)) { Write-Host "SKIP: barony_mod_loader.py is missing at $app."; exit 0 }

Remove-Item -Recurse -Force $profileRoot -ErrorAction SilentlyContinue
Remove-Item -Force $registry -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $profileRoot | Out-Null

& $python $app runtime register --registry $registry --id barony-bml-runtime-windows-noop --runtime-info $runtimeInfo --steam-executable $steamExe --hook-library $hookLibrary --launcher-helper $launcherHelper --hook-manifest $hookManifest --steam-build-id 22630456
if ($LASTEXITCODE -ne 0) { throw 'runtime register failed' }
& $python $app profile create $profileRoot --id windows-validation --steam --runtime-info $runtimeInfo
if ($LASTEXITCODE -ne 0) { throw 'profile create failed' }

$dryRunJson = & $python $app launch $profileRoot --package $smokePackage --registry $registry --runtime barony-bml-runtime-windows-noop --dry-run -- -windowed -size=640x480 -nosound "-quickstart=$quickstartClass"
if ($LASTEXITCODE -ne 0) { throw 'launch dry-run failed' }
$plan = $dryRunJson | ConvertFrom-Json
foreach ($property in $plan.environment.PSObject.Properties) {
    [Environment]::SetEnvironmentVariable($property.Name, [string]$property.Value, 'Process')
}
[Environment]::SetEnvironmentVariable('BML_VALIDATE_INJECTION_ONLY', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_INSTALL_GET_ITEM_PASSTHROUGH', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ITEM_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_ASSIGN_ACTIONS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ENTITY_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_SET_SPRITE_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_CLOSE_CHEST_SERVER_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY', '1', 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA', [string]$assignRva, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA', [string]$newEntityRva, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA', [string]$setSpriteRva, 'Process')
if ($forceShoppingSpree) {
    [Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_RVA', [string]$doNewGameRva, 'Process')
    [Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_EXIT_ON_FIRE', '0', 'Process')
    [Environment]::SetEnvironmentVariable('BML_FORCE_SHOPPING_SPREE', '1', 'Process')
}
$launchArgs = @('-windowed', '-size=640x480', '-nosound', "-quickstart=$quickstartClass")
if ($mapName) { $launchArgs += "-map=$mapName" }
$dryRunJson = & $python $app launch $profileRoot --package $smokePackage --registry $registry --runtime barony-bml-runtime-windows-noop --dry-run -- @launchArgs
if ($LASTEXITCODE -ne 0) { throw 'launch dry-run failed' }
$plan = $dryRunJson | ConvertFrom-Json
$command = @($plan.command)
$launcher = $command[0]
$launcherArgs = @()
if ($command.Count -gt 1) { $launcherArgs = $command[1..($command.Count - 1)] }
$launcherProcess = Start-Process -FilePath $launcher -ArgumentList $launcherArgs -WorkingDirectory $plan.cwd -PassThru

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\reports'
$probePath = Join-Path $reportRoot 'stash-placement-discovery-report.json'
function Test-PlacementCorrelation($probe) {
    if ($probe.status -ne 'observed') { return $false }
    if ($probe.summary.assignActionsCalls -le 0 -or $probe.summary.assignActionsNewEntityDelta -le 0) { return $false }
    if ($probe.assignActions.newEntitySetSpriteMatches -gt 0) { return $true }
    return $false
}
$observed = $false
for ($i = 0; $i -lt 30; ++$i) {
    Start-Sleep -Seconds 1
    if (Test-Path $probePath) {
        $probe = Get-Content -Raw $probePath | ConvertFrom-Json
        if (Test-PlacementCorrelation $probe) {
            $observed = $true
            break
        }
    }
    if ($launcherProcess.HasExited) { break }
}
if (-not $launcherProcess.HasExited) {
    Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue
    Get-Process barony -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
if (-not (Test-Path $probePath)) { throw 'placement discovery report was not written' }
$probe = Get-Content -Raw $probePath | ConvertFrom-Json
if (-not (Test-PlacementCorrelation $probe)) {
    throw "placement discovery did not observe scoped placement within timeout: $($probe | ConvertTo-Json -Compress)"
}

& $python $app runtime report (Join-Path $profileRoot 'BaronyModLoader\reports\runtime-load-report.json')
if ($LASTEXITCODE -ne 0) { throw 'runtime report failed' }
$runtime = Get-Content -Raw (Join-Path $reportRoot 'runtime-load-report.json') | ConvertFrom-Json
if ($runtime.status -ne 'loaded' -or $runtime.errors.Count -ne 0) {
    throw "Runtime load report failed validation: $($runtime | ConvertTo-Json -Compress)"
}
Write-Host 'OK: Windows placement discovery observed scoped placement.'
