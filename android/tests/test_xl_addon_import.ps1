#!/usr/bin/env pwsh
param([string]$Serial, [ValidatePattern('^[A-Za-z0-9_.-]+\.7z$')][string]$DistributionName = '', [switch]$PumoUiImport, [switch]$PumoUiCold)
$ErrorActionPreference = 'Stop'
if ($PumoUiCold -and !$PumoUiImport) { throw 'PumoUiCold requires PumoUiImport' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-addon-import-android'
if ($PumoUiImport) { $output = Join-Path $PSScriptRoot '../../temp/xl-pumo-ui-import-android' }
if ($PumoUiCold) { $output = Join-Path $PSScriptRoot '../../temp/xl-pumo-ui-cold-android' }
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
function Invoke-AddonAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $seconds = if ($PumoUiImport) { 420 } else { 180 }
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds $seconds -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Addon import adb failed: $result" }
    return $result
}
function Invoke-AddonPrivate {
    param([string]$Command)
    Invoke-AddonAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-addon-import-backup'
$paths = @('shared_prefs', 'files/file_sets.json')
if ($PumoUiImport) {
    if ($DistributionName) { throw 'Pumo UI fixture imports its qualified raw addons separately' }
    $paths += @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
        'files/descent.cfg', 'files/graphics_safety.json', 'files/.graphics_safety.lock', 'files/d2x-redux/mod_usage.json',
        'files/d2x-redux/.active_mod_paths', 'files/d2x-redux/.active_set_path', 'files/d1x-redux/.active_set_path',
        'files/d2x-redux/.mission_assets', 'files/d2x-redux/.mission_assets.json',
        'files/imported/sets/default',
        'files/d2x-redux/missions/pmines.hog', 'files/d2x-redux/missions/pmines.mn2',
        'files/d2x-redux/missions/d2x-xl.hog', 'files/d2x-redux/missions/d2x.ham', 'files/d2x-redux/mods/pmines')
}
$ready = $false
$hostBackup = $null
$backupHash = $null
$offloaded = $false
$restoreTemporary = $null
try {
    Invoke-AddonAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-AddonPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-AddonPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count) { Invoke-AddonPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null }
    $before = if ($existing.Count) { @(Invoke-AddonPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object } else { @() }
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    if ($PumoUiImport) {
        # Keep the verified protection copy off the small disposable app-data volume
        $backupHash = (([string](Invoke-AddonPrivate "sha256sum $backup/state.tar")) -split '\s+')[0]
        if ($backupHash -notmatch '^[0-9a-f]{64}$') { throw 'Invalid profile backup digest' }
        $hostBackup = Join-Path $output "profile-$backupHash.tar"
        if (!(Test-Path -LiteralPath $hostBackup)) {
            # Stream binary bytes directly; run-as cannot write shell-owned external storage
            $transfer = [System.Diagnostics.Process]::new()
            $transfer.StartInfo.FileName = $script:ADB
            foreach ($argument in @('-s', $Serial, 'exec-out', 'run-as', $script:PACKAGE, 'cat', "$backup/state.tar")) {
                $transfer.StartInfo.ArgumentList.Add($argument)
            }
            $transfer.StartInfo.UseShellExecute = $false
            $transfer.StartInfo.RedirectStandardOutput = $true
            $transfer.StartInfo.RedirectStandardError = $true
            $destination = [System.IO.File]::Create($hostBackup)
            try {
                $transfer.Start() | Out-Null
                $errors = $transfer.StandardError.ReadToEndAsync()
                $copy = $transfer.StandardOutput.BaseStream.CopyToAsync($destination)
                if (!$copy.Wait(420000)) { $transfer.Kill(); throw 'Profile backup transfer timed out; device backup retained' }
                $copy.GetAwaiter().GetResult() | Out-Null
                $transfer.WaitForExit()
                if ($transfer.ExitCode -ne 0) { throw "Profile backup transfer failed: $($errors.GetAwaiter().GetResult())" }
            } finally { $destination.Dispose(); $transfer.Dispose() }
        }
        if ((Get-FileHash -LiteralPath $hostBackup -Algorithm SHA256).Hash -ine $backupHash) { throw 'Host profile backup differs; device backup retained' }
        $offloaded = $true
        Invoke-AddonPrivate "rm $backup/state.tar" | Out-Null
        # Remove only backed-up legacy staging so it cannot satisfy the UI launch
        Invoke-AddonPrivate 'rm -rf files/d2x-redux/mods/pmines' | Out-Null
        Invoke-AddonPrivate 'rm -f files/d2x-redux/missions/pmines.hog files/d2x-redux/missions/pmines.mn2 files/d2x-redux/missions/d2x-xl.hog files/d2x-redux/missions/d2x.ham' | Out-Null
    }
    Invoke-AddonAdb -Arguments @('logcat', '-c') | Out-Null
    $suite = if ($PumoUiImport) { 'pumo_ui_import' } else { 'xl_addon_import' }
    $instrument = @('shell', 'am', 'instrument', '-w', '-e', 'suite', $suite)
    if ($DistributionName) { $instrument += @('-e', 'xl_addon_distribution', $DistributionName) }
    $instrument += "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation"
    $result = Invoke-AddonAdb -Arguments $instrument
    $result | Set-Content "$output/instrumentation.txt" -Encoding utf8
    Write-Output $result
    $expected = if ($PumoUiImport) { 'PASS: Original Pumo and raw addons imported' } else { 'PASS: XL addon selective import' }
    if ($result -notmatch $expected -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'XL addon import checks failed'
    }
    if ($PumoUiImport) {
        foreach ($name in @('pumo-ui-import-proof.json', 'pumo-ui-import.png')) {
            $source = "/sdcard/Android/data/$($script:PACKAGE)/files/$name"
            Invoke-AddonAdb -Arguments @('pull', $source, "$output/$name") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $source) | Out-Null
        }
        $inputHash = (([string](Invoke-AddonPrivate 'sha256sum files/saf_provider_source/pmines_v11.7z')) -split '\s+')[0]
        if ($inputHash -ne '82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf') { throw 'Pumo picker input changed; preserving it' }
        # The original host archive is retained; the imported private archive is independent
        Invoke-AddonPrivate 'rm files/saf_provider_source/pmines_v11.7z' | Out-Null
        $scriptName = if ($PumoUiCold) { 'test_xl_pumo_opening_gates.jsonc' } else { 'test_xl_pumo_hub_activation.jsonc' }
        $steps = @(Get-Content "$PSScriptRoot/../game_scripts/$scriptName" -Raw | ConvertFrom-Json)
        $steps[0]._info._owner = 'test_xl_addon_import'
        $runtimeScript = Join-Path $output 'ui-owned-hub.jsonc'
        $steps | ConvertTo-Json -Depth 30 | Set-Content $runtimeScript -Encoding utf8
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $runtimeScript -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 600
        $runtimeExit = $LASTEXITCODE
        Invoke-AddonPrivate 'cat files/automation_result.json' | Set-Content "$output/runtime-result.json" -Encoding utf8
        Invoke-AddonPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/runtime-steps.jsonl" -Encoding utf8
        Invoke-AddonPrivate 'cat files/introspect.json' | Set-Content "$output/runtime-state.json" -Encoding utf8
        $runtime = Get-Content "$output/runtime-result.json" -Raw | ConvertFrom-Json
        if ($runtimeExit -ne 0 -or $runtime.result -ne 'PASS' -or $runtime.steps_completed -ne ($steps.Count - 1)) { throw 'Imported original Pumo did not pass ordinary activation' }
        $state = Get-Content "$output/runtime-state.json" -Raw | ConvertFrom-Json
        # mounted_mods describes legacy session mounts, not the mission catalog's private view
        if ($state.mission_asset_owner -ne 'mod/pmines_v11.7z' -or $state.mission_asset_generation -lt 1) {
            throw 'UI runtime did not retain imported mission ownership'
        }
        Invoke-AddonAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/pumo-ui-runtime.png') | Out-Null
        Invoke-AddonAdb -Arguments @('pull', '/data/local/tmp/pumo-ui-runtime.png', "$output/runtime-render.png") | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/pumo-ui-runtime.png') | Out-Null
        if ($PumoUiCold) {
            $warmPid = ([string](Invoke-AddonAdb -Arguments @('shell', 'pidof', "$($script:PACKAGE):game"))).Trim()
            Invoke-AddonAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/warm-logcat.txt" -Encoding utf8
            $coldSteps = @(Get-Content "$PSScriptRoot/../game_scripts/test_xl_pumo_opening_gates_cold.jsonc" -Raw | ConvertFrom-Json)
            $coldSteps[0]._info._owner = 'test_xl_addon_import'
            $coldScript = Join-Path $output 'ui-owned-cold.jsonc'
            $coldSteps | ConvertTo-Json -Depth 30 | Set-Content $coldScript -Encoding utf8
            # The normal launcher/menu path must activate the imported owner from the save
            & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $coldScript -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 300
            $coldExit = $LASTEXITCODE
            Invoke-AddonPrivate 'cat files/automation_result.json' | Set-Content "$output/cold-result.json" -Encoding utf8
            Invoke-AddonPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/cold-steps.jsonl" -Encoding utf8
            Invoke-AddonPrivate 'cat files/introspect.json' | Set-Content "$output/cold-state.json" -Encoding utf8
            $cold = Get-Content "$output/cold-result.json" -Raw | ConvertFrom-Json
            $coldState = Get-Content "$output/cold-state.json" -Raw | ConvertFrom-Json
            $coldPid = ([string](Invoke-AddonAdb -Arguments @('shell', 'pidof', "$($script:PACKAGE):game"))).Trim()
            if ($coldExit -ne 0 -or $cold.result -ne 'PASS' -or $cold.steps_completed -ne ($coldSteps.Count - 1)) { throw 'Imported original Pumo cold menu load failed' }
            if ($warmPid -notmatch '^\d+$' -or $coldPid -notmatch '^\d+$' -or $warmPid -eq $coldPid) { throw 'Imported Pumo cold load did not use a fresh game process' }
            if ($coldState.mission_asset_owner -ne 'mod/pmines_v11.7z' -or $coldState.mission_asset_generation -lt 1) { throw 'Imported Pumo cold load lost its catalog owner' }
            @{ warm_pid = $warmPid; cold_pid = $coldPid; owner = $coldState.mission_asset_owner; fresh_process = $true } |
                ConvertTo-Json | Set-Content "$output/cold-proof.json" -Encoding utf8
            Invoke-AddonAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/pumo-ui-cold-runtime.png') | Out-Null
            Invoke-AddonAdb -Arguments @('pull', '/data/local/tmp/pumo-ui-cold-runtime.png', "$output/cold-render.png") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/pumo-ui-cold-runtime.png') | Out-Null
            Write-Output 'PASS: UI-imported original Pumo opening gates and fresh-process save restoration'
        } else {
            Write-Output 'PASS: UI-imported original Pumo normal firing, hub transition and save restoration'
        }
    } else {
        $render = "/sdcard/Android/data/$($script:PACKAGE)/files/xl-addon-picker.png"
        Invoke-AddonAdb -Arguments @('pull', $render, "$output/render.png") | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $render) | Out-Null
        if ($DistributionName) {
            $originalRender = "/sdcard/Android/data/$($script:PACKAGE)/files/xl-addon-picker-original.png"
            Invoke-AddonAdb -Arguments @('pull', $originalRender, "$output/original-render.png") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $originalRender) | Out-Null
        }
    }
} finally {
    try {
        $captureFailure = $null
        try {
            Invoke-AddonAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
        } catch { $captureFailure = $_ }
        Invoke-AddonAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            foreach ($path in $paths) { Invoke-AddonPrivate "rm -rf $path" | Out-Null }
            if ($offloaded) {
                if ((Get-FileHash -LiteralPath $hostBackup -Algorithm SHA256).Hash -ine $backupHash) { throw 'Host profile backup changed; preserving recovery state' }
                $restoreTemporary = "/data/local/tmp/xl-addon-restore-$([guid]::NewGuid().ToString('N')).tar"
                Invoke-AddonAdb -Arguments @('push', $hostBackup, $restoreTemporary) | Out-Null
                Invoke-AddonPrivate "tar -xf $restoreTemporary" | Out-Null
            } elseif ($existing.Count) { Invoke-AddonPrivate "tar -xf $backup/state.tar" | Out-Null }
            $after = if ($existing.Count) { @(Invoke-AddonPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object } else { @() }
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object @($before) @($after)) { throw "Addon import profile restoration differs; backup retained at $backup" }
            Invoke-AddonPrivate "rm -rf $backup" | Out-Null
            if ($restoreTemporary) { Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $restoreTemporary) | Out-Null }
            Write-Output 'PASS: Addon UI profile restoration hashes match'
        }
        if ($captureFailure) { throw $captureFailure }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
