$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$probeSlug = 'windows-init-class-probe'
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
$initClassRva = $env:BML_INIT_CLASS_PROBE_RVA

if (-not [Environment]::Is64BitProcess) { Write-Host 'SKIP: 64-bit PowerShell is required for the initClass probe.'; exit 0 }
if (-not (Test-Path $hookLibrary)) { Write-Host "SKIP: Hook DLL is missing at $hookLibrary."; exit 0 }
if (-not (Test-Path $launcherHelper)) { Write-Host "SKIP: Launcher helper is missing at $launcherHelper."; exit 0 }
if (-not (Test-Path $steamExe)) { Write-Host "SKIP: Steam Barony executable is missing at $steamExe."; exit 0 }
if (-not (Test-Path $python)) { Write-Host "SKIP: Expected Python host is missing at $python."; exit 0 }
if (-not (Test-Path $app)) { Write-Host "SKIP: barony_mod_loader.py is missing at $app."; exit 0 }
if (-not $initClassRva) { Write-Host 'SKIP: BML_INIT_CLASS_PROBE_RVA is not set for the initClass fired-hook probe.'; exit 0 }

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
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_ASSIGN_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_NEW_ENTITY_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_PLACEMENT_DISCOVERY_SET_SPRITE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_STASH_INSTALL_GET_ITEM_PASSTHROUGH', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ITEM_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_ASSIGN_ACTIONS_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_REMOVE_ITEM_VOID_CHEST_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_CLOSE_CHEST_SERVER_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_NEW_ENTITY_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_SET_SPRITE_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_DO_NEW_GAME_PROBE_RVA', $null, 'Process')
[Environment]::SetEnvironmentVariable('BML_INIT_CLASS_PROBE_RVA', [string]$initClassRva, 'Process')
[Environment]::SetEnvironmentVariable('BML_INIT_CLASS_PROBE_EXIT_ON_FIRE', '1', 'Process')
$command = @($plan.command)
$launcher = $command[0]
$launcherArgs = @()
if ($command.Count -gt 1) { $launcherArgs = $command[1..($command.Count - 1)] }
$launcherProcess = Start-Process -FilePath $launcher -ArgumentList $launcherArgs -WorkingDirectory $plan.cwd -PassThru

$reportRoot = Join-Path $profileRoot 'BaronyModLoader\reports'
$probePath = Join-Path $reportRoot 'windows-init-class-probe-report.json'
$screenshotPath = Join-Path $profileRoot 'BaronyModLoader\\reports\\initclass-probe-timeout.png'
$fired = $false
for ($i = 0; $i -lt 30; ++$i) {
    Start-Sleep -Seconds 1
    if (Test-Path $probePath) {
        $probe = Get-Content -Raw $probePath | ConvertFrom-Json
        if ($probe.status -eq 'fired' -and $probe.quickstartCalls -gt 0 -and $probe.lastPlayer -eq 0 -and $probe.lastReturnRva -eq 5032615) {
            $fired = $true
            break
        }
    }
    if ($launcherProcess.HasExited) { break }
}
$launcherAliveAtTimeout = -not $launcherProcess.HasExited
if ($launcherAliveAtTimeout) {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bmp.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Stop-Process -Id $launcherProcess.Id -Force -ErrorAction SilentlyContinue
    Get-Process barony -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
Write-Host ("launcherAliveAtTimeout=" + $launcherAliveAtTimeout)
if ($launcherProcess.HasExited) { Write-Host ("launcherExitCode=" + $launcherProcess.ExitCode) }
if ($launcherAliveAtTimeout) { Write-Host ("timeoutScreenshot=" + $screenshotPath) }
if (-not (Test-Path $probePath)) { throw 'initClass probe report was not written' }
$probe = Get-Content -Raw $probePath | ConvertFrom-Json
if (-not $fired) {
    throw "initClass fired-hook probe did not observe the quickstart call within timeout: $($probe | ConvertTo-Json -Compress)"
}

& $python $app runtime report (Join-Path $profileRoot 'BaronyModLoader\reports\runtime-load-report.json')
if ($LASTEXITCODE -ne 0) { throw 'runtime report failed' }
$runtime = Get-Content -Raw (Join-Path $reportRoot 'runtime-load-report.json') | ConvertFrom-Json
if ($runtime.status -ne 'loaded' -or $runtime.errors.Count -ne 0) {
    throw "Runtime load report failed validation: $($runtime | ConvertTo-Json -Compress)"
}
Write-Host 'OK: Windows initClass quickstart probe passed.'
