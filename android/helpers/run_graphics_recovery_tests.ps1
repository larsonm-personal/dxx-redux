#!/usr/bin/env pwsh
# Provisioned-emulator faults; restores changed permissions even after an assertion fails
param(
    [switch]$Install,
    [ValidateSet('d1', 'd2')][string]$Game,
    [ValidateSet('stall', 'record_unreadable', 'publication_blocked', 'rebuild_failure', 'menu_abandoned', 'accept_publication_blocked', 'normal_exit', 'repair_interrupted', 'activity_replaced')][string]$Fault,
    [string]$Serial = 'emulator-5554'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
if ($Serial -notmatch '^emulator-[0-9]+$') { throw 'This reset-state fixture is restricted to provisioned emulators' }
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/graphics-recovery-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
$faults = if ($Fault) { @($Fault) } else { @('stall', 'record_unreadable', 'publication_blocked', 'rebuild_failure', 'menu_abandoned', 'accept_publication_blocked', 'normal_exit', 'repair_interrupted', 'activity_replaced') }
$results = [System.Collections.Generic.List[object]]::new()
$permissionFault = ''
$originalPermissionMode = ''

function Invoke-Device {
    param([string[]]$Arguments)
    $output = & $script:ADB -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $output" }
    return ($output -join "`n")
}

function Read-DeviceJson {
    param([string]$Path)
    return (Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'cat', $Path) | ConvertFrom-Json)
}

function Get-GamePid {
    $output = & $script:ADB -s $Serial shell pidof "$($script:PACKAGE):game" 2>$null
    return "$output".Trim()
}

function Wait-RecoveryCondition {
    param([scriptblock]$Condition, [string]$Description, [int]$Seconds = 20)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 150
    }
    throw "Timed out: $Description"
}

function Assert-Accepted {
    param($Expected)
    $record = Read-DeviceJson 'files/graphics_safety.json'
    foreach ($field in $Expected.PSObject.Properties) {
        if ($record.accepted.($field.Name) -ne $field.Value) { throw "Accepted $($field.Name) changed after fault" }
    }
    return $record
}

function Assert-MirroredConfig {
    param($Expected)
    foreach ($path in @('files/descent.cfg', 'files/d1x-redux/descent.cfg', 'files/d2x-redux/descent.cfg')) {
        $text = Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'cat', $path)
        foreach ($field in $Expected.PSObject.Properties) {
            $fieldMatches = [regex]::Matches($text, '(?m)^' + [regex]::Escape($field.Name) + '=(-?[0-9]+)\r?$')
            if ($fieldMatches.Count -ne 1 -or [int]$fieldMatches[0].Groups[1].Value -ne $field.Value) {
                throw "Mirrored accepted value differs: $path $($field.Name)"
            }
        }
    }
}

function Restore-FaultPermissions {
    if ($permissionFault -eq 'record_unreadable') {
        Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'chmod', $originalPermissionMode, 'files/graphics_safety.json') | Out-Null
    } elseif ($permissionFault -in @('publication_blocked', 'accept_publication_blocked')) {
        Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'chmod', $originalPermissionMode, 'files') | Out-Null
    }
}

function Push-RecoveryScript {
    param([string]$Name, [string]$GameId)
    $resolved = Resolve-TestScript -ScriptPath (Join-Path $script:ANDROID_ROOT "game_scripts/$Name") -GameId $GameId
    # Native automation accepts comments, but the formatter's trailing commas require normalization
    $steps = Get-Content $resolved -Raw | ConvertFrom-Json -NoEnumerate
    $source = Join-Path $outputDirectory "$GameId-$Name.json"
    ConvertTo-Json -InputObject $steps -Depth 100 | Set-Content $source -Encoding utf8NoBOM
    Invoke-Device @('push', $source, "/data/local/tmp/$Name") | Out-Null
    Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$Name", "files/$Name") | Out-Null
}

try {
    foreach ($gameId in $games) {
        foreach ($faultName in $faults) {
            $caseName = "$gameId-$faultName"
            Write-Output "Graphics recovery case: $caseName"
            $runner = (Join-Path $PSScriptRoot 'run_test.ps1').Replace("'", "''")
            $fixture = (Resolve-TestScript -ScriptPath (Join-Path $script:ANDROID_ROOT 'game_scripts/test_graphics_recovery_fixture.jsonc') -GameId $gameId).Replace("'", "''")
            $command = "& '$runner' -ScriptName '$fixture' -Game $gameId -LeaveRunning -TimeoutSeconds 180"
            if ($Install) { $command += ' -Install'; $Install = $false }
            & pwsh -NoProfile -NonInteractive -Command $command 2>&1 |
                Tee-Object -FilePath (Join-Path $outputDirectory "$caseName-fixture.txt") | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "Fixture failed: $caseName" }
            $originalPid = Get-GamePid
            if (-not $originalPid) { throw 'Fixture left no game process' }
            $expected = (Read-DeviceJson 'files/graphics_safety.json').accepted
            if ($expected.TexFilt -ne 1) { throw 'Fixture must have explicitly accepted filtering enabled' }
            Assert-MirroredConfig $expected
            $trigger = switch ($faultName) {
                'stall' { 'test_graphics_stall_trigger.jsonc' }
                'rebuild_failure' { 'test_graphics_rebuild_trigger.jsonc' }
                'menu_abandoned' { 'test_graphics_menu_abandon_trigger.jsonc' }
                'normal_exit' { 'test_graphics_normal_exit_trigger.jsonc' }
                'repair_interrupted' { 'test_graphics_repair_interrupt_trigger.jsonc' }
                default { 'test_graphics_storage_trigger.jsonc' }
            }
            Push-RecoveryScript $trigger $gameId
            $restartScript = if ($faultName -eq 'normal_exit') { 'test_graphics_staged_restart.jsonc' } else { 'test_graphics_recovery_restart.jsonc' }
            Push-RecoveryScript $restartScript $gameId
            if ($faultName -eq 'accept_publication_blocked') { Push-RecoveryScript 'test_graphics_accept_trigger.jsonc' $gameId }
            Invoke-Device @('logcat', '-c') | Out-Null
            if ($faultName -eq 'publication_blocked') {
                $originalPermissionMode = (Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'stat', '-c', '%a', 'files')).Trim()
                if ($originalPermissionMode -notmatch '^[0-7]{3,4}$') { throw 'Invalid original directory permissions' }
                $permissionFault = $faultName
                Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'chmod', '500', 'files') | Out-Null
            }
            $timer = [Diagnostics.Stopwatch]::StartNew()
            $restoreTimer = $null
            Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $trigger) | Out-Null
            if ($faultName -notin @('publication_blocked', 'rebuild_failure', 'menu_abandoned', 'normal_exit')) {
                Wait-RecoveryCondition { (Read-DeviceJson 'files/graphics_safety.json').attempt.phase -eq 3 } 'Armed challenge'
            }
            if ($faultName -eq 'activity_replaced') {
                $beforeReplacement = Assert-Accepted $expected
                $beforeReplacement | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory "$caseName-before-replacement.json") -Encoding utf8NoBOM
                Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.GAME_COMMAND', '--es', 'command', 'recreate_activity') | Out-Null
            }
            if ($faultName -eq 'accept_publication_blocked') {
                $originalPermissionMode = (Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'stat', '-c', '%a', 'files')).Trim()
                if ($originalPermissionMode -notmatch '^[0-7]{3,4}$') { throw 'Invalid original directory permissions' }
                $permissionFault = $faultName
                Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'chmod', '500', 'files') | Out-Null
                Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', 'test_graphics_accept_trigger.jsonc') | Out-Null
            }
            if ($faultName -in @('menu_abandoned', 'normal_exit')) {
                Wait-RecoveryCondition {
                    $record = Read-DeviceJson 'files/graphics_safety.json'
                    $record.attempt.phase -eq 1 -and $record.attempt.candidate.TexFilt -eq 2
                } 'Unvalidated native-menu attempt'
                Start-Sleep -Seconds 6
                $beforeKill = Assert-Accepted $expected
                if ($beforeKill.attempt.phase -ne 1 -or $beforeKill.attempt.deadline_ms -ne 0) {
                    throw 'Native-menu attempt became eligible before the menu closed'
                }
                $beforeKill | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory "$caseName-before-kill.json") -Encoding utf8NoBOM
                if ($faultName -eq 'menu_abandoned') { Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'kill', '-9', $originalPid) | Out-Null }
            }
            if ($faultName -eq 'repair_interrupted') {
                Wait-RecoveryCondition { (Read-DeviceJson 'files/graphics_safety.json').attempt.phase -eq 4 } 'Durable interrupted-repair marker' 10
                $partial = Assert-Accepted $expected
                $partial | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory "$caseName-partial-record.json") -Encoding utf8NoBOM
                foreach ($variant in @('root', 'd1', 'd2')) {
                    $path = if ($variant -eq 'root') { 'files/descent.cfg' } else { "files/${variant}x-redux/descent.cfg" }
                    $text = Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'cat', $path)
                    $text | Set-Content (Join-Path $outputDirectory "$caseName-partial-$variant.cfg") -Encoding utf8NoBOM
                    $filter = if ($variant -eq 'root') { 1 } else { 2 }
                    if ($text -notmatch "(?m)^TexFilt=$filter\r?$") { throw "Repair was not interrupted between config publications: $variant" }
                }
                if ((Get-GamePid) -ne $originalPid) { throw 'Game closed before partial-publication evidence was captured' }
                Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'kill', '-9', $originalPid) | Out-Null
            }
            if ($faultName -eq 'record_unreadable') {
                $originalPermissionMode = (Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'stat', '-c', '%a', 'files/graphics_safety.json')).Trim()
                if ($originalPermissionMode -notmatch '^[0-7]{3,4}$') { throw 'Invalid original record permissions' }
                $permissionFault = $faultName
                Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'chmod', '000', 'files/graphics_safety.json') | Out-Null
            }
            if ($faultName -in @('stall', 'rebuild_failure')) {
                Wait-RecoveryCondition { (Read-DeviceJson 'files/graphics_safety.json').attempt.phase -eq 4 } 'Kotlin timeout rejected the stalled trial' 10
                if ($faultName -eq 'rebuild_failure') { $restoreTimer = [Diagnostics.Stopwatch]::StartNew() }
                Wait-RecoveryCondition {
                    try { Assert-MirroredConfig $expected; $true } catch { $false }
                } 'Accepted config publication after the durable restore marker' 2
                if ((Get-GamePid) -ne $originalPid) { throw 'Could not verify durable rollback while the stalled process was still alive' }
            }
            try {
                Wait-RecoveryCondition { (Get-GamePid) -ne $originalPid } 'Bounded game-process recovery' 15
            } catch {
                Invoke-Device @('logcat', '-d') | Set-Content (Join-Path $outputDirectory "$caseName-failure-logcat.txt") -Encoding utf8NoBOM
                throw
            }
            $elapsed = [int]$timer.ElapsedMilliseconds
            $restoreElapsed = if ($restoreTimer) { [int]$restoreTimer.ElapsedMilliseconds } else { $null }
            $logs = Invoke-Device @('logcat', '-d')
            $logs | Set-Content (Join-Path $outputDirectory "$caseName-logcat.txt") -Encoding utf8NoBOM
            # Resolution rollback uses the ten-second context-rebuild watchdog in graphicsRestoreTimeoutMs
            if ($faultName -eq 'rebuild_failure' -and ($restoreElapsed -lt 8800 -or $restoreElapsed -gt 11000)) {
                throw "Repeated rebuilds did not honor the ten-second restore watchdog: $restoreElapsed ms"
            }
            if ($faultName -eq 'stall' -and ($elapsed -lt 6500 -or $elapsed -gt 11500)) {
                throw "Stall recovery did not honor the five-second deadline plus three-second watchdog: $elapsed ms"
            }
            Restore-FaultPermissions
            $permissionFault = ''
            $after = Assert-Accepted $expected
            $restartExpected = $expected | ConvertTo-Json | ConvertFrom-Json
            if ($faultName -eq 'normal_exit') {
                if ($null -ne $after.attempt) { throw 'Normal exit did not clear attempt ownership' }
                $restartExpected.TexFilt = 2
                Assert-MirroredConfig $restartExpected
            }
            $after | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory "$caseName-after.json") -Encoding utf8NoBOM
            if ($faultName -eq 'activity_replaced') {
                if ($logs -notmatch 'automation recreate activity=' -or $logs -notmatch 'changing_config=true' -or
                    $logs -notmatch 'restored=true') { throw 'Missing actual Activity replacement lifecycle evidence' }
                if ($logs -match 'startGame\(\) called while game already running') { throw 'Replacement Activity attempted a second native startup' }
                if ($elapsed -gt 5000) { throw "Activity replacement recovery exceeded five seconds: $elapsed ms" }
                $decision = [regex]::Match($logs, 'Graphics trial decision id=[0-9]+ accept=0 reason=background result=2 now_ms=([0-9]+) deadline_ms=([0-9]+)')
                if (-not $decision.Success -or [long]$decision.Groups[1].Value -ge [long]$decision.Groups[2].Value) {
                    throw 'Activity replacement did not durably cancel the armed trial before its deadline'
                }
                Assert-MirroredConfig $expected
            }
            if ($faultName -eq 'stall') {
                if ($logs -notmatch 'armed render-thread stall') { throw 'No evidence the render thread stalled after arming' }
                Assert-MirroredConfig $expected
            }
            if ($faultName -eq 'accept_publication_blocked') {
                $decision = [regex]::Match($logs, 'Graphics trial decision id=[0-9]+ accept=1 reason=ok result=-1 now_ms=([0-9]+) deadline_ms=([0-9]+)')
                if (-not $decision.Success -or [long]$decision.Groups[1].Value -ge [long]$decision.Groups[2].Value) {
                    throw 'No evidence of a failed OK publication before the challenge deadline'
                }
            }
            if ($faultName -eq 'repair_interrupted' -and $logs -notmatch 'Graphics repair fault: paused after first publication tex_filt=1 duration_ms=10000') {
                throw 'No evidence of an actual pause between config publications'
            }
            if ($faultName -eq 'rebuild_failure') {
                foreach ($dimensions in @('width=800 height=600', 'width=640 height=480')) {
                    if ($logs -notmatch ('EGL initialization failed after old context destruction ' + $dimensions)) {
                        throw "No evidence of actual EGL initialization failure: $dimensions"
                    }
                }
                Assert-MirroredConfig $expected
            }
            if ($faultName -notin @('menu_abandoned', 'normal_exit', 'repair_interrupted')) {
                Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT') | Out-Null
                Wait-RecoveryCondition {
                    Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT') | Out-Null
                    $setup = Read-DeviceJson 'files/setup_introspect.json'
                    $setup.displayed_launch_error -match 'Graphics settings could not be restored|Could not save or recover graphics settings|The game display was recreated during a graphics change'
                } 'Surfaced graphics recovery message'
                if ($faultName -eq 'rebuild_failure') {
                    $setup = Read-DeviceJson 'files/setup_introspect.json'
                    if ($setup.displayed_launch_error -notmatch 'Graphics settings could not be restored in time') {
                        throw 'Accepted-mode rebuild failure did not surface the restore-watchdog message'
                    }
                }
                if ($faultName -eq 'activity_replaced') {
                    $setup = Read-DeviceJson 'files/setup_introspect.json'
                    if ($setup.displayed_launch_error -notmatch 'The game display was recreated during a graphics change') {
                        throw 'Activity replacement did not surface its recovery message'
                    }
                }
            }
            Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/introspect.json', 'files/automation_result.json') | Out-Null
            Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', 'launch', '--es', 'game', $gameId) | Out-Null
            Wait-RecoveryCondition { $fresh = Get-GamePid; $fresh -and $fresh -ne $originalPid } 'Fresh game process'
            Wait-RecoveryCondition {
                Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT') | Out-Null
                try { Read-DeviceJson 'files/introspect.json' | Out-Null; $true } catch { $false }
            } 'Fresh engine responds to introspection'
            if ($faultName -ne 'normal_exit') {
                Wait-RecoveryCondition { (Read-DeviceJson 'files/graphics_safety.json').attempt -eq $null } 'Abandoned-attempt repair before rendering'
            }
            Assert-Accepted $expected | Out-Null
            Assert-MirroredConfig $restartExpected
            Invoke-Device @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
            Invoke-Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $restartScript) | Out-Null
            Wait-RecoveryCondition {
                try {
                    $result = Read-DeviceJson 'files/automation_result.json'
                    if ($result.result -eq 'FAIL') { throw "Restart automation failed: $($result.reason)" }
                    $result.result -eq 'PASS'
                } catch {
                    if ($_ -match 'Restart automation failed') { throw }
                    $false
                }
            } 'Fresh accepted gameplay without a prompt' 45
            Assert-Accepted $expected | Out-Null
            Assert-MirroredConfig $expected
            $results.Add([ordered]@{ game = $gameId; fault = $faultName; recovery_ms = $elapsed; restore_ms = $restoreElapsed; passed = $true })
            $results | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory 'results.json') -Encoding utf8NoBOM
            Write-Output "PASS: $caseName, recovery $elapsed ms, accepted tuple and mirrored configs retained"
        }
    }
    $results | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory 'results.json') -Encoding utf8NoBOM
    Write-Output "Graphics recovery tests passed; output: $outputDirectory"
} catch {
    $failure = $_
    # Preserve restart failures too, before the outer runner releases the emulator
    try {
        Adb-Dev-Timeout -Serial $Serial -Seconds 15 -AdbArgs @('logcat', '-d') |
            Set-Content (Join-Path $outputDirectory "$caseName-failure-logcat.txt") -Encoding utf8NoBOM
        foreach ($name in @('introspect.json', 'setup_introspect.json', 'graphics_safety.json')) {
            try {
                Read-DeviceJson "files/$name" | ConvertTo-Json -Depth 60 |
                    Set-Content (Join-Path $outputDirectory "$caseName-failure-$name") -Encoding utf8NoBOM
            } catch { Write-Warning "Could not capture ${name}: $($_.Exception.Message)" }
        }
    } catch { Write-Warning "Could not capture recovery failure: $($_.Exception.Message)" }
    throw $failure
} finally {
    Restore-FaultPermissions
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
