#!/usr/bin/env pwsh
param([string]$Serial, [ValidatePattern('^[A-Za-z0-9_.-]+\.7z$')][string]$DistributionName = '', [switch]$PumoUiImport, [switch]$PumoUiCold, [switch]$PumoUiIsolation, [switch]$PumoUiDisabledOwner, [switch]$TimeEquipment, [string]$TimeEquipmentPilot)
$ErrorActionPreference = 'Stop'
if ($TimeEquipment -and ($PumoUiImport -or !$DistributionName -or !(Test-Path -LiteralPath $TimeEquipmentPilot -PathType Leaf))) {
    throw 'TimeEquipment requires DistributionName and an engine-written pilot with original Warp Shock level6 unlocked, in its own mode'
}
if ($PumoUiCold -and !$PumoUiImport) { throw 'PumoUiCold requires PumoUiImport' }
if ($PumoUiIsolation -and (!$PumoUiImport -or $PumoUiCold)) { throw 'PumoUiIsolation requires PumoUiImport in its own mode' }
if ($PumoUiDisabledOwner -and (!$PumoUiImport -or $PumoUiCold -or $PumoUiIsolation)) { throw 'PumoUiDisabledOwner requires PumoUiImport in its own mode' }
$saveMode = $PumoUiIsolation -or $PumoUiDisabledOwner
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-addon-import-android'
if ($PumoUiImport) { $output = Join-Path $PSScriptRoot '../../temp/xl-pumo-ui-import-android' }
if ($PumoUiCold) { $output = Join-Path $PSScriptRoot '../../temp/xl-pumo-ui-cold-android' }
if ($PumoUiIsolation) { $output = Join-Path $PSScriptRoot '../../temp/xl-pumo-ui-isolation-android' }
if ($PumoUiDisabledOwner) { $output = Join-Path $PSScriptRoot '../../temp/xl-pumo-ui-disabled-android' }
if ($TimeEquipment) { $output = Join-Path $PSScriptRoot '../../temp/xl-time-equipment-android' }
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
function Set-ImportedOwnerExpectations {
    param([object[]]$Steps)
    foreach ($step in $Steps) {
        if ($step.expect -and $step.expect.PSObject.Properties.Name -contains 'xl_pilot.runtime_active') {
            $owner = if ($step.expect.'xl_pilot.runtime_active') { 'mod/pmines_v11.7z' } else { 'base' }
            $step.expect | Add-Member -NotePropertyName mission_asset_owner -NotePropertyValue $owner -Force
        }
    }
}
$backup = 'files/xl-addon-import-backup'
$paths = @('shared_prefs', 'files/file_sets.json')
if ($PumoUiImport -or $TimeEquipment) {
    $paths += @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d1x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
        'files/descent.cfg', 'files/graphics_safety.json', 'files/.graphics_safety.lock', 'files/d2x-redux/mod_usage.json',
        'files/d2x-redux/.active_mod_paths', 'files/d2x-redux/.active_set_path', 'files/d1x-redux/.active_set_path',
        'files/d2x-redux/.mission_assets', 'files/d2x-redux/.mission_assets.json',
        'files/d2x-redux/missions/d2x-xl.hog', 'files/d2x-redux/missions/d2x.ham')
}
if ($PumoUiImport) {
    if ($DistributionName) { throw 'Pumo UI fixture imports its qualified raw addons separately' }
    $paths += @('files/imported/sets/default',
        'files/d2x-redux/missions/pmines.hog', 'files/d2x-redux/missions/pmines.mn2',
        'files/d2x-redux/mods/pmines')
}
if ($TimeEquipment) {
    if (([string](Invoke-AddonPrivate 'if test -e files/imported/sets/xl-picker-check; then echo yes; fi')).Trim() -eq 'yes') {
        throw 'Reserved disposable gameplay set already exists; inspect it before rerunning'
    }
    $paths += @('files/imported/sets/xl-picker-check', 'files/d2x-redux/missions/wshock20.hog', 'files/d2x-redux/missions/wshock20.mn2')
}
$ready = $false
$hostBackup = $null
$backupHash = $null
$offloaded = $false
$restoreTemporary = $null
$attemptId = [guid]::NewGuid().ToString('N')
$completed = $false
$restored = $false
$failure = $null
$phase = 'protection'
$warmRun = $null
$coldRun = $null
$disabledRuns = @{}
@{ attempt_id = $attemptId; status = 'RUNNING' } | ConvertTo-Json | Set-Content "$output/owner-result.json" -Encoding utf8
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
        # Reuse an identical private safety archive instead of duplicating retail data
        if (!(Test-Path -LiteralPath $hostBackup)) {
            foreach ($prior in @('xl-pumo-ui-import-android', 'xl-pumo-ui-cold-android', 'xl-pumo-ui-isolation-android', 'xl-pumo-ui-disabled-android')) {
                $candidate = Join-Path $PSScriptRoot "../../temp/$prior/profile-$backupHash.tar"
                if (Test-Path -LiteralPath $candidate) { $hostBackup = $candidate; break }
            }
        }
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
    $phase = 'import'
    $suite = if ($PumoUiImport) { 'pumo_ui_import' } else { 'xl_addon_import' }
    $instrument = @('shell', 'am', 'instrument', '-w', '-e', 'suite', $suite)
    if ($DistributionName) { $instrument += @('-e', 'xl_addon_distribution', $DistributionName) }
    if ($TimeEquipment) { $instrument += @('-e', 'xl_time_equipment', 'true') }
    $instrument += "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation"
    $result = Invoke-AddonAdb -Arguments $instrument
    $result | Set-Content "$output/instrumentation.txt" -Encoding utf8
    Write-Output $result
    $expected = if ($TimeEquipment) { 'PASS: Original XL resources and mission imported for gameplay' } elseif ($PumoUiImport) { 'PASS: Original Pumo and raw addons imported' } else { 'PASS: XL addon selective import' }
    if ($result -notmatch $expected -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'XL addon import checks failed'
    }
    if ($DistributionName) {
        foreach ($artifact in @(@('xl-addon-picker-original.png', 'original-render.png'), @('xl-addon-original-inventory.json', 'resource-inventory.json'))) {
            $external = "/sdcard/Android/data/$($script:PACKAGE)/files/$($artifact[0])"
            Invoke-AddonAdb -Arguments @('pull', $external, "$output/$($artifact[1])") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $external) | Out-Null
        }
    }
    if ($TimeEquipment) {
        $phase = 'gameplay'
        $pilotFile = [IO.Path]::GetFileName($TimeEquipmentPilot).ToLowerInvariant()
        if ($pilotFile -notmatch '^[A-Za-z0-9_]{1,8}\.plr$') { throw 'Gameplay pilot must have a portable native callsign' }
        $pilot = [IO.Path]::GetFileNameWithoutExtension($pilotFile)
        $pilotTemporary = "/data/local/tmp/xl-time-pilot-$attemptId.plr"
        Invoke-AddonAdb -Arguments @('push', (Resolve-Path -LiteralPath $TimeEquipmentPilot).Path, $pilotTemporary) | Out-Null
        Invoke-AddonPrivate "mkdir -p files/d2x-redux/Players && cp $pilotTemporary files/d2x-redux/Players/$pilotFile" | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $pilotTemporary) | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'am', 'start', '-n', "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
        if (!(Wait-SetupActivityReady)) { throw 'Launcher did not restart after original import' }
        foreach ($enabled in @($false, $true)) {
            Invoke-AddonAdb -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
                '--es', 'command', 'set_mod_enabled', '--es', 'filename', 'xl-resources', '--ez', 'enabled', $enabled.ToString().ToLowerInvariant()) | Out-Null
            if (!(Wait-SetupCondition -TimeoutSeconds 20 -Predicate {
                param($snapshot)
                $entry = $snapshot.content_entries_by_id.'mod:xl-resources'
                return $snapshot.active_set -eq 'xl-picker-check' -and $null -ne $entry -and $entry.enabled -eq $enabled
            })) { throw "Resource enable toggle did not reach $enabled" }
            Invoke-AddonPrivate 'cat files/setup_introspect.json' | Set-Content "$output/toggle-$enabled.json" -Encoding utf8
        }
        $scenario = Join-Path $PSScriptRoot '../game_scripts/test_xl_time_equipment_runtime.jsonc'
        $steps = @(Get-Content $scenario -Raw | ConvertFrom-Json)
        # Reuse startup, saved-state expectations and the normal bullet-time toggle
        $bullet = -1
        for ($i = 0; $i -lt $steps.Count; $i++) { if ($steps[$i].action -eq 'meta_action' -and $steps[$i].id -eq 1047) { $bullet = $i; break } }
        if ($bullet -lt 0) { throw 'Missing shared time-equipment control phase' }
        $newGame = 0
        while ($newGame -lt $steps.Count -and !($steps[$newGame].action -eq 'select' -and $steps[$newGame].text -eq 'New game')) { $newGame++ }
        if ($newGame -eq $steps.Count) { throw 'Missing shared time-equipment startup' }
        $cold = @($steps[0..($newGame - 1)]) + @(
            @{ action = 'select'; text = 'Load game'; timeout_ms = 15000 },
            @{ action = 'select'; text = 'level 6'; timeout_ms = 15000 }
        ) + @($steps[-1], $steps[$bullet], @{
            action = 'wait_for'; timeout_ms = 60000
            expect = @{ 'xl_pilot.slow_motion.world_speed' = @{ lte = 1 }; 'xl_pilot.slow_motion.player_speed' = @{ lte = 1 };
                'xl_pilot.slow_motion.has_slow_motion' = $true; 'xl_pilot.slow_motion.has_bullet_time' = $true }
        })
        $coldScript = Join-Path $output 'time-equipment-cold.jsonc'
        $cold | ConvertTo-Json -Depth 30 | Set-Content $coldScript -Encoding utf8
        $pids = @{}
        foreach ($run in @(@{ phase = 'warm'; script = $scenario }, @{ phase = 'cold'; script = $coldScript })) {
            $phase = $run.phase
            & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $run.script -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 240 -Params @{ PILOT = $pilot }
            $runtimeExit = $LASTEXITCODE
            Invoke-AddonPrivate 'cat files/automation_result.json' | Set-Content "$output/$phase-result.json" -Encoding utf8
            Invoke-AddonPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/$phase-steps.jsonl" -Encoding utf8
            Invoke-AddonPrivate 'cat files/introspect.json' | Set-Content "$output/$phase-state.json" -Encoding utf8
            $receipt = Get-Content "$output/$phase-result.json" -Raw | ConvertFrom-Json
            if ($runtimeExit -or $receipt.result -ne 'PASS' -or $receipt.steps_completed -ne $receipt.total_steps) { throw "Original Android time-equipment $phase failed" }
            if ($phase -eq 'warm') { $warmRun = $receipt.run_id } else { $coldRun = $receipt.run_id }
            $pids[$phase] = ([string](Invoke-AddonAdb -Arguments @('shell', 'pidof', "$($script:PACKAGE):game"))).Trim()
            if ($pids[$phase] -notmatch '^\d+$') { throw "Missing Android $phase game process" }
            Invoke-AddonAdb -Arguments @('shell', 'screencap', '-p', "/data/local/tmp/xl-time-$phase.png") | Out-Null
            Invoke-AddonAdb -Arguments @('pull', "/data/local/tmp/xl-time-$phase.png", "$output/$phase.png") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', "/data/local/tmp/xl-time-$phase.png") | Out-Null
            Invoke-AddonPrivate 'find files/d2x-redux/Players -type f -name *.sg* -exec sha256sum {} \;' | Set-Content "$output/$phase-saves.sha256" -Encoding utf8
            Invoke-AddonAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        }
        Invoke-AddonPrivate 'cat files/d2x-redux/.mission_assets.json' | Set-Content "$output/runtime-owner.json" -Encoding utf8
        $owner = Get-Content "$output/runtime-owner.json" -Raw | ConvertFrom-Json
        $installation = [string]$owner.installation_mounts[0]
        if (@($owner.installation_mounts).Count -ne 1 -or !$installation.EndsWith('/files/imported/sets/xl-picker-check/.content/mods/xl-resources') -or
            $installation -notmatch '^/data/[A-Za-z0-9_./-]+$') {
            throw 'Native runtime did not use the enabled imported resource owner'
        }
        $inventory = Get-Content "$output/resource-inventory.json" -Raw | ConvertFrom-Json
        $staged = @(Invoke-AddonPrivate "sha256sum $((@($inventory.files) | ForEach-Object { "$installation/$($_.path)" }) -join ' ')" | ForEach-Object { $_ -split '\r?\n' })
        $staged | Set-Content "$output/native-resource-hashes.sha256" -Encoding utf8
        foreach ($resource in $inventory.files) {
            if ($staged -notcontains "$($resource.sha256)  $installation/$($resource.path)") { throw "Native imported resource differs: $($resource.path)" }
        }
        if ($pids.warm -eq $pids.cold -or !$warmRun -or !$coldRun -or $warmRun -eq $coldRun) { throw 'Cold continuation did not use a fresh complete game process' }
        @{ warm_pid = $pids.warm; cold_pid = $pids.cold; fresh_process = $true; resources_verified = @($inventory.files).Count } |
            ConvertTo-Json | Set-Content "$output/cold-proof.json" -Encoding utf8
        if (Compare-Object @(Get-Content "$output/warm-saves.sha256") @(Get-Content "$output/cold-saves.sha256")) {
            throw 'Cold continuation changed the saved checkpoint files'
        }
        Write-Output 'PASS: Original Android time pickups, touch controls, imported owner, energy, save and cold continuation'
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
        if ($saveMode) {
            $phase = 'toggle'
            Invoke-AddonAdb -Arguments @('shell', 'am', 'start', '-n', "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
            if (!(Wait-SetupActivityReady)) { throw 'Launcher was not ready for mission toggle control' }
            foreach ($enabled in @($false, $true)) {
                Invoke-AddonAdb -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
                    '--es', 'command', 'set_mod_enabled', '--es', 'filename', 'pmines_v11.7z', '--ez', 'enabled', $enabled.ToString().ToLowerInvariant()) | Out-Null
                $accepted = Wait-SetupCondition -TimeoutSeconds 20 -Predicate {
                    param($snapshot)
                    $entry = $snapshot.content_entries_by_id.'mod:pmines_v11.7z'
                    return $null -ne $entry -and $entry.enabled -eq $enabled
                }
                Invoke-AddonPrivate 'cat files/setup_introspect.json' | Set-Content "$output/toggle-$enabled.json" -Encoding utf8
                if (!$accepted) { throw "Original 7z mission toggle did not reach enabled=$enabled" }
            }
        }
        $scriptName = if ($saveMode) { 'test_xl_pumo_save_isolation.jsonc' } elseif ($PumoUiCold) { 'test_xl_pumo_opening_gates.jsonc' } else { 'test_xl_pumo_hub_activation.jsonc' }
        $steps = @(Get-Content "$PSScriptRoot/../game_scripts/$scriptName" -Raw | ConvertFrom-Json)
        if ($saveMode) { Set-ImportedOwnerExpectations -Steps $steps }
        if ($PumoUiDisabledOwner) {
            # Stop the existing isolation seed after its first ordinary stock manual save
            $saveRequest = -1
            $seedEnd = -1
            for ($i = 1; $i -lt $steps.Count; $i++) {
                if ($steps[$i].action -eq 'set_debug' -and $steps[$i].field -eq 'android_game_request' -and $steps[$i].value -eq 'save') { $saveRequest = $i }
                if ($saveRequest -ge 0 -and $steps[$i].action -eq 'wait_for' -and $steps[$i].expect.game_window_is_front -eq $true -and $steps[$i].expect.time_paused -eq $false) { $seedEnd = $i; break }
            }
            if ($seedEnd -lt 1) { throw 'Cannot find the stock manual-save boundary in the Pumo isolation seed' }
            $steps = @($steps[0..$seedEnd])
        }
        $steps[0]._info._owner = 'test_xl_addon_import'
        $runtimeScript = Join-Path $output 'ui-owned-hub.jsonc'
        $steps | ConvertTo-Json -Depth 30 | Set-Content $runtimeScript -Encoding utf8
        $phase = 'warm'
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $runtimeScript -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 600
        $runtimeExit = $LASTEXITCODE
        Invoke-AddonPrivate 'cat files/automation_result.json' | Set-Content "$output/runtime-result.json" -Encoding utf8
        Invoke-AddonPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/runtime-steps.jsonl" -Encoding utf8
        Invoke-AddonPrivate 'cat files/introspect.json' | Set-Content "$output/runtime-state.json" -Encoding utf8
        $runtime = Get-Content "$output/runtime-result.json" -Raw | ConvertFrom-Json
        if ($runtimeExit -ne 0 -or $runtime.result -ne 'PASS' -or $runtime.steps_completed -ne ($steps.Count - 1)) { throw 'Imported original Pumo did not pass ordinary activation' }
        $warmRun = $runtime.run_id
        $state = Get-Content "$output/runtime-state.json" -Raw | ConvertFrom-Json
        # mounted_mods describes legacy session mounts, not the mission catalog's private view
        $expectedOwner = if ($saveMode) { 'base' } else { 'mod/pmines_v11.7z' }
        if ($state.mission_asset_owner -ne $expectedOwner -or $state.mission_asset_generation -lt 1) {
            throw 'UI runtime did not retain imported mission ownership'
        }
        Invoke-AddonAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/pumo-ui-runtime.png') | Out-Null
        Invoke-AddonAdb -Arguments @('pull', '/data/local/tmp/pumo-ui-runtime.png', "$output/runtime-render.png") | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/pumo-ui-runtime.png') | Out-Null
        if ($PumoUiDisabledOwner) {
            $stockPath = 'files/d2x-redux/Players/save_sets/single/player/d2'
            $stockBefore = @(Invoke-AddonPrivate "find $stockPath -type f -exec sha256sum {} \;") | Sort-Object
            if ($stockBefore.Count -lt 1) { throw 'No stock save payload to protect' }
            $stockBefore | Set-Content "$output/stock-before.sha256" -Encoding utf8
            foreach ($case in @('negative', 'recovery')) {
                $phase = "disabled-$case"
                $probeName = if ($case -eq 'negative') { 'test_xl_pumo_disabled_owner.jsonc' } else { 'test_xl_pumo_disabled_owner_recovery.jsonc' }
                $probeScript = Join-Path $PSScriptRoot "../game_scripts/$probeName"
                $probeSteps = @(Get-Content $probeScript -Raw | ConvertFrom-Json)
                & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $probeScript -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 301
                $probeExit = $LASTEXITCODE
                foreach ($artifact in @('automation_result.json', 'automation_log.jsonl', 'introspect.json')) {
                    Invoke-AddonPrivate "cat files/$artifact" | Set-Content "$output/$case-$artifact" -Encoding utf8
                }
                Invoke-AddonAdb -Arguments @('shell', 'screencap', '-p', "/data/local/tmp/disabled-$case.png") | Out-Null
                Invoke-AddonAdb -Arguments @('pull', "/data/local/tmp/disabled-$case.png", "$output/$case-render.png") | Out-Null
                Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', "/data/local/tmp/disabled-$case.png") | Out-Null
                $stockAfter = @(Invoke-AddonPrivate "find $stockPath -type f -exec sha256sum {} \;") | Sort-Object
                $stockAfter | Set-Content "$output/stock-after-$case.sha256" -Encoding utf8
                if (Compare-Object @($stockBefore) @($stockAfter)) { throw "Stock save payload changed during $case, before restoration" }
                $probeResult = Get-Content "$output/$case-automation_result.json" -Raw | ConvertFrom-Json
                if ($probeExit -ne 0 -or $probeResult.result -ne 'PASS' -or $probeResult.steps_completed -ne ($probeSteps.Count - 1) -or !$probeResult.run_id -or $probeResult.run_id -eq $warmRun -or $probeResult.run_id -in @($disabledRuns.Values)) {
                    throw "Disabled-owner $case did not pass a fresh complete run"
                }
                $disabledRuns[$case] = $probeResult.run_id
            }
            Write-Output 'PASS: Disabled imported Pumo save rejected, stock bank preserved, and ordinary re-enable/resume recovered'
        }
        if ($PumoUiCold -or $PumoUiIsolation) {
            $warmPid = ([string](Invoke-AddonAdb -Arguments @('shell', 'pidof', "$($script:PACKAGE):game"))).Trim()
            Invoke-AddonAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/warm-logcat.txt" -Encoding utf8
            $coldName = if ($PumoUiIsolation) { 'test_xl_pumo_save_isolation_cold.jsonc' } else { 'test_xl_pumo_opening_gates_cold.jsonc' }
            $coldSteps = @(Get-Content "$PSScriptRoot/../game_scripts/$coldName" -Raw | ConvertFrom-Json)
            if ($PumoUiIsolation) { Set-ImportedOwnerExpectations -Steps $coldSteps }
            $coldSteps[0]._info._owner = 'test_xl_addon_import'
            $coldScript = Join-Path $output 'ui-owned-cold.jsonc'
            $coldSteps | ConvertTo-Json -Depth 30 | Set-Content $coldScript -Encoding utf8
            # The normal launcher/menu path must activate the imported owner from the save
            $phase = 'cold'
            & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $coldScript -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 300
            $coldExit = $LASTEXITCODE
            Invoke-AddonPrivate 'cat files/automation_result.json' | Set-Content "$output/cold-result.json" -Encoding utf8
            Invoke-AddonPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/cold-steps.jsonl" -Encoding utf8
            Invoke-AddonPrivate 'cat files/introspect.json' | Set-Content "$output/cold-state.json" -Encoding utf8
            $cold = Get-Content "$output/cold-result.json" -Raw | ConvertFrom-Json
            $coldState = Get-Content "$output/cold-state.json" -Raw | ConvertFrom-Json
            $coldPid = ([string](Invoke-AddonAdb -Arguments @('shell', 'pidof', "$($script:PACKAGE):game"))).Trim()
            if ($coldExit -ne 0 -or $cold.result -ne 'PASS' -or $cold.steps_completed -ne ($coldSteps.Count - 1)) { throw 'Imported original Pumo cold menu load failed' }
            $coldRun = $cold.run_id
            if ($warmPid -notmatch '^\d+$' -or $coldPid -notmatch '^\d+$' -or $warmPid -eq $coldPid) { throw 'Imported Pumo cold load did not use a fresh game process' }
            if ($coldState.mission_asset_owner -ne $expectedOwner -or $coldState.mission_asset_generation -lt 1) { throw 'Imported Pumo cold load lost its expected owner' }
            @{ warm_pid = $warmPid; cold_pid = $coldPid; owner = $coldState.mission_asset_owner; fresh_process = $true } |
                ConvertTo-Json | Set-Content "$output/cold-proof.json" -Encoding utf8
            Invoke-AddonAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/pumo-ui-cold-runtime.png') | Out-Null
            Invoke-AddonAdb -Arguments @('pull', '/data/local/tmp/pumo-ui-cold-runtime.png', "$output/cold-render.png") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/pumo-ui-cold-runtime.png') | Out-Null
            if ($PumoUiIsolation) {
                Write-Output 'PASS: UI-imported original Pumo and stock warm/cold save isolation'
            } else {
                Write-Output 'PASS: UI-imported original Pumo opening gates and fresh-process save restoration'
            }
        } elseif (!$PumoUiDisabledOwner) {
            Write-Output 'PASS: UI-imported original Pumo normal firing, hub transition and save restoration'
        }
    } else {
        if (!$TimeEquipment) {
            $render = "/sdcard/Android/data/$($script:PACKAGE)/files/xl-addon-picker.png"
            Invoke-AddonAdb -Arguments @('pull', $render, "$output/render.png") | Out-Null
            Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $render) | Out-Null
        }
    }
    $completed = $true
    $phase = 'complete'
} catch {
    $failure = $_.Exception.Message
    throw
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
            $restored = $true
            Write-Output 'PASS: Addon UI profile restoration hashes match'
        }
        if ($captureFailure) { throw $captureFailure }
    } catch {
        $failure = "${failure} Restoration/capture: $($_.Exception.Message)"
        throw
    } finally {
        @{
            attempt_id = $attemptId
            status = if ($completed -and $restored -and !$failure) { 'PASS' } else { 'FAIL' }
            phase = $phase
            failure = $failure
            profile_hashes_restored = $restored
            warm_run_id = $warmRun
            cold_run_id = $coldRun
            disabled_negative_run_id = $disabledRuns.negative
            disabled_recovery_run_id = $disabledRuns.recovery
        } | ConvertTo-Json | Set-Content "$output/owner-result.json" -Encoding utf8
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
