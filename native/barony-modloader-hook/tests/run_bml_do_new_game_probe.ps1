$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$probeSlug = 'windows-do-new-game-probe'
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
$forceShoppingSpree = $env:BML_TEST_FORCE_SHOPPING_SPREE
$doNewGameRva = $env:BML_TEST_DO_NEW_GAME_RVA
$summonNoSmokeRva = $env:BML_TEST_SUMMON_NO_SMOKE_RVA
if ($summonNoSmokeRva -and -not $forceShoppingSpree) { throw 'summonMonsterNoSmoke probe requires BML_TEST_FORCE_SHOPPING_SPREE=1' }

if (-not [Environment]::Is64BitProcess) { Write-Host 'SKIP: 64-bit PowerShell is required for the doNewGame probe.'; exit 0 }
if (-not (Test-Path $hookLibrary)) { Write-Host "SKIP: Hook DLL is missing at $hookLibrary."; exit 0 }
if (-not (Test-Path $launcherHelper)) { Write-Host "SKIP: Launcher helper is missing at $launcherHelper."; exit 0 }
if (-not (Test-Path $steamExe)) { Write-Host "SKIP: Steam Barony executable is missing at $steamExe."; exit 0 }
if (-not (Test-Path $python)) { Write-Host "SKIP: Expected Python host is missing at $python."; exit 0 }
if (-not (Test-Path $app)) { Write-Host "SKIP: barony_mod_loader.py is missing at $app."; exit 0 }
if (-not $doNewGameRva) { Write-Host 'SKIP: BML_TEST_DO_NEW_GAME_RVA is not set for the doNewGame fired-hook probe.'; exit 0 }

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
[Environment]::SetEnvironmentVariable('BML_ADD_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_GET_CHEST_LIST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_CLOSE_CHEST_SERVER_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ITEM_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_ASSIGN_ACTIONS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ENTITY_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_SET_SPRITE_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_INIT_CLASS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_SUMMON_NO_SMOKE_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_RVA', [string]$doNewGameRva, 'Process')
$doNewGameExitOnFire = if ($summonNoSmokeRva) { '0' } else { '1' }
[Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_EXIT_ON_FIRE', $doNewGameExitOnFire, 'Process')
if ($forceShoppingSpree) { [Environment]::SetEnvironmentVariable('BML_FORCE_SHOPPING_SPREE', '1', 'Process') }
if ($summonNoSmokeRva) {
    [Environment]::SetEnvironmentVariable('BML_SUMMON_NO_SMOKE_PROBE_RVA', [string]$summonNoSmokeRva, 'Process')
    [Environment]::SetEnvironmentVariable('BML_SUMMON_NO_SMOKE_PROBE_EXIT_ON_FIRE', '1', 'Process')
}
$command = @($plan.command)
$launcher = $command[0]
$launcherArgs = @()
if ($command.Count -gt 1) { $launcherArgs = $command[1..($command.Count - 1)] }
$launcherProcess = Start-Process -FilePath $launcher -ArgumentList $launcherArgs -WorkingDirectory $plan.cwd -PassThru

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\\reports'
$probePath = Join-Path $reportRoot 'windows-do-new-game-probe-report.json'
$summonProbePath = Join-Path $reportRoot 'windows-summon-monster-probe-report.json'
$fired = $false
for ($i = 0; $i -lt 30; ++$i) {
    Start-Sleep -Seconds 1
    if (Test-Path $probePath) {
        $probe = Get-Content -Raw $probePath | ConvertFrom-Json
        if (-not $summonNoSmokeRva) {
            if ($probe.status -eq 'fired' -and $probe.replacementCalls -eq 1 -and -not $probe.lastMakeHighscore) {
                $fired = $true
                break
            }
        }
    }
    if ($summonNoSmokeRva -and (Test-Path $summonProbePath)) {
        $summonProbe = Get-Content -Raw $summonProbePath | ConvertFrom-Json
        if ($summonProbe.status -eq 'fired' -and $summonProbe.shopkeeperCalls -gt 0 -and $summonProbe.lastCreature -eq 20 -and $summonProbe.lastForceLocation -and $summonProbe.lastReturnRva -eq 5418932) {
            $fired = $true
            break
        }
    }
    if ($launcherProcess.HasExited) { break }
}
if (-not $launcherProcess.HasExited) {
    Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue
    Get-Process barony -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
if (-not (Test-Path $probePath)) { throw 'doNewGame probe report was not written' }
if ($summonNoSmokeRva -and -not (Test-Path $summonProbePath)) { throw 'summonMonsterNoSmoke probe report was not written' }
if (-not $fired) {
    $probe = Get-Content -Raw $probePath | ConvertFrom-Json
    if ($summonNoSmokeRva -and (Test-Path $summonProbePath)) {
        $summonProbe = Get-Content -Raw $summonProbePath | ConvertFrom-Json
        throw "summonMonsterNoSmoke fired-hook probe did not observe the Shopping Spree shopkeeper summon: doNewGame=$($probe | ConvertTo-Json -Compress) summon=$($summonProbe | ConvertTo-Json -Compress)"
    }
    throw "doNewGame fired-hook probe did not fire within timeout: $($probe | ConvertTo-Json -Compress)"
}

& $python $app runtime report (Join-Path $profileRoot 'BaronyModLoader\\reports\\runtime-load-report.json')
if ($LASTEXITCODE -ne 0) { throw 'runtime report failed' }

$probe = Get-Content -Raw $probePath | ConvertFrom-Json
$runtime = Get-Content -Raw (Join-Path $reportRoot 'runtime-load-report.json') | ConvertFrom-Json
if (-not $summonNoSmokeRva) {
    if ($probe.status -ne 'fired' -or $probe.replacementCalls -ne 1 -or -not $probe.installed -or $probe.lastMakeHighscore) {
        throw "doNewGame fired-hook probe validation failed: $($probe | ConvertTo-Json -Compress)"
    }
    if ($forceShoppingSpree) {
        if ($probe.shoppingSpreeState.currentMode -ne 4 -or -not $probe.shoppingSpreeState.inUse -or $probe.shoppingSpreeState.eventType -ne 2 -or $probe.shoppingSpreeState.loadingsavegame -ne 0 -or $probe.shoppingSpreeState.loadinglobbykey -ne 0) {
            throw "Shopping Spree state injection failed: $($probe | ConvertTo-Json -Compress)"
        }
    }
} else {
    $summonProbe = Get-Content -Raw $summonProbePath | ConvertFrom-Json
    if ($summonProbe.status -ne 'fired' -or $summonProbe.shopkeeperCalls -lt 1 -or $summonProbe.lastCreature -ne 20 -or -not $summonProbe.lastForceLocation -or $summonProbe.lastReturnRva -ne 5418932) {
        throw "summonMonsterNoSmoke fired-hook probe validation failed: $($summonProbe | ConvertTo-Json -Compress)"
    }
}
if ($runtime.status -ne 'loaded' -or $runtime.errors.Count -ne 0) {
    throw "Runtime load report failed validation: $($runtime | ConvertTo-Json -Compress)"
}
if ($runtime.loadedMods.Count -ne 1 -or $runtime.loadedMods[0].id -ne 'jml.windows_smoke') {
    throw "Unexpected loaded mods: $($runtime | ConvertTo-Json -Compress)"
}

if ($summonNoSmokeRva) {
    Write-Host 'OK: Windows Shopping Spree summonMonsterNoSmoke probe passed.'
} else {
    Write-Host 'OK: Windows doNewGame fired-hook probe passed.'
}
