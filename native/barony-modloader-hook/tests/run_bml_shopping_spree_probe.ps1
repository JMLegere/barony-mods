$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$probeSlug = 'windows-shopping-spree-probe'
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
$assignRva = $env:BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA
$newEntityRva = $env:BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA
$setSpriteRva = $env:BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA

if (-not [Environment]::Is64BitProcess) { Write-Host 'SKIP: 64-bit PowerShell is required for the Shopping Spree probe.'; exit 0 }
if (-not (Test-Path $hookLibrary)) { Write-Host "SKIP: Hook DLL is missing at $hookLibrary."; exit 0 }
if (-not (Test-Path $launcherHelper)) { Write-Host "SKIP: Launcher helper is missing at $launcherHelper."; exit 0 }
if (-not (Test-Path $steamExe)) { Write-Host "SKIP: Steam Barony executable is missing at $steamExe."; exit 0 }
if (-not (Test-Path $python)) { Write-Host "SKIP: Expected Python host is missing at $python."; exit 0 }
if (-not (Test-Path $app)) { Write-Host "SKIP: barony_mod_loader.py is missing at $app."; exit 0 }
if (-not $assignRva -or -not $newEntityRva -or -not $setSpriteRva) { Write-Host 'SKIP: Shopping Spree probe requires explicit placement RVAs.'; exit 0 }

Remove-Item -Recurse -Force $profileRoot -ErrorAction SilentlyContinue
Remove-Item -Force $registry -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $profileRoot | Out-Null

& $python $app runtime register --registry $registry --id barony-bml-runtime-windows-noop --runtime-info $runtimeInfo --steam-executable $steamExe --hook-library $hookLibrary --launcher-helper $launcherHelper --hook-manifest $hookManifest --steam-build-id 22630456
if ($LASTEXITCODE -ne 0) { throw 'runtime register failed' }
& $python $app profile create $profileRoot --id windows-validation --steam --runtime-info $runtimeInfo
if ($LASTEXITCODE -ne 0) { throw 'profile create failed' }

$dryRunJson = & $python $app launch $profileRoot --package $smokePackage --registry $registry --runtime barony-bml-runtime-windows-noop --dry-run -- -windowed -size=1280x720 -nosound
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
[Environment]::SetEnvironmentVariable('BML_INIT_CLASS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_CLOSE_CHEST_SERVER_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY', '1', 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA', [string]$assignRva, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA', [string]$newEntityRva, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA', [string]$setSpriteRva, 'Process')
$command = @($plan.command)
$launcher = $command[0]
$launcherArgs = @()
if ($command.Count -gt 1) { $launcherArgs = $command[1..($command.Count - 1)] }
$proc = Start-Process -FilePath $launcher -ArgumentList $launcherArgs -WorkingDirectory $plan.cwd -PassThru
Start-Sleep -Seconds 15
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 2
[System.Windows.Forms.SendKeys]::SendWait('{DOWN 4}')
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 2
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 2
[System.Windows.Forms.SendKeys]::SendWait('{DOWN 2}')
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 2
[System.Windows.Forms.SendKeys]::SendWait('{RIGHT 2}')
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait('{ENTER}')
Start-Sleep -Seconds 60
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue; Get-Process barony -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue }
$report = Join-Path $profileRoot 'BaronyModLoader\reports\stash-placement-discovery-report.json'
if (Test-Path $report) { Get-Content $report }
