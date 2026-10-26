# Shared native co-op scenarios; session setup and transport policy belong to the caller
$script:deviceAutomationRunIds = @{}

function Start-DeviceGameAutomation {
    param([string]$Serial, [string]$ScriptName, [hashtable]$Params = @{})

    $scriptPath = Join-Path $REPO_ROOT "android\game_scripts\$ScriptName"
    if (-not (Test-Path $scriptPath)) {
        Write-Status "FAIL: automation script not found: $scriptPath" "Red"
        return $false
    }

    # The shared resolver reads optional JSON properties under normal PowerShell semantics
    $scriptPath = & { Set-StrictMode -Off; Resolve-TestScript -ScriptPath $scriptPath -GameId $Game -Params $Params }

    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "push", $scriptPath, "/data/local/tmp/$ScriptName"
    ) -Seconds 30 | Out-Null
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/$ScriptName", "files/$ScriptName"
    ) -Seconds 10 | Out-Null
    $copied = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "ls", "files/$ScriptName"
    ) -Seconds 5
    if (-not $copied) {
        # A slow directory read must not restage or relaunch an already copied script
        $copied = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "run-as", $PACKAGE, "ls", "files/$ScriptName"
        ) -Seconds 15
    }
    if (-not $copied -or $copied -notmatch [regex]::Escape($ScriptName)) {
        Write-Status "FAIL: could not stage $ScriptName on $Serial" "Red"
        return $false
    }

    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "rm", "-f",
        "files/automation_result.json", "files/automation_log.jsonl"
    ) -Seconds 5 | Out-Null
    $runId = [guid]::NewGuid().ToString('N')
    $script:deviceAutomationRunIds[$Serial] = $runId
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE",
        "--es", "script", $ScriptName, "--es", "run_id", $runId
    ) -Seconds 10 | Out-Null
    return $true
}

function Get-DeviceAutomationResult {
    param([string]$Serial)

    $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/automation_result.json"
    ) -Seconds 5
    if (-not $json -or $json -notmatch '^\s*\{') {
        return $null
    }
    try {
        $result = $json | ConvertFrom-Json
        if (-not (Test-AutomationResultRunId -Result $result -ExpectedRunId $script:deviceAutomationRunIds[$Serial])) {
            return $null
        }
        return $result
    } catch {
        return $null
    }
}

function Write-DeviceAutomationDiagnostics {
    param([string]$Serial)

    # Refresh before Cleanup stops the engine; the last polled snapshot may
    # predate the failed gameplay action
    $snapshot = Get-GameIntrospection -Serial $Serial
    if ($snapshot) {
        $snapshotPath = Join-Path $REPO_ROOT "temp/lan-failure-$Serial.json"
        $snapshot | ConvertTo-Json -Depth 40 | Set-Content -Encoding utf8 -LiteralPath $snapshotPath
        Write-Status "  $Serial failure state: $snapshotPath" 'Yellow'
    }
    $result = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/automation_result.json"
    ) -Seconds 5
    $log = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/automation_log.jsonl"
    ) -Seconds 5
    Write-Status "  $Serial automation result: $result" "Yellow"
    if ($log) {
        Write-Status "  $Serial automation log tail:" "Yellow"
        ($log -split "`n") | Select-Object -Last 20 | ForEach-Object {
            Write-Status "    $_" "Gray"
        }
    }
}

function Invoke-PairedGameAutomation {
    param(
        [string]$PrimarySerial,
        [string]$PrimaryScript,
        [string]$SecondarySerial,
        [string]$SecondaryScript,
        [string]$Description,
        [hashtable]$PrimaryParams = @{},
        [hashtable]$SecondaryParams = @{},
        [int]$TimeoutSec = 60,
        [switch]$IndependentEndgame
    )

    if (-not (Start-DeviceGameAutomation -Serial $SecondarySerial -ScriptName $SecondaryScript -Params $SecondaryParams)) {
        return $false
    }
    Start-Sleep -Milliseconds 250
    if (-not (Start-DeviceGameAutomation -Serial $PrimarySerial -ScriptName $PrimaryScript -Params $PrimaryParams)) {
        return $false
    }

    $script:independentEndgameSeen = $false
    $script:primaryAutomationResult = $null
    $script:secondaryAutomationResult = $null
    $finished = Wait-ForCondition -Description $Description -TimeoutSec $TimeoutSec -PollMs 1000 -Condition {
        $script:primaryAutomationResult = Get-DeviceAutomationResult -Serial $PrimarySerial
        $script:secondaryAutomationResult = Get-DeviceAutomationResult -Serial $SecondarySerial
        $primaryDone = $script:primaryAutomationResult -and
        $script:primaryAutomationResult.result -in @("PASS", "FAIL")
        $secondaryDone = $script:secondaryAutomationResult -and
        $script:secondaryAutomationResult.result -in @("PASS", "FAIL")
        if ($IndependentEndgame -and $primaryDone -and -not $secondaryDone -and -not $script:independentEndgameSeen) {
            $viewer = Get-GameIntrospection -Serial $SecondarySerial
            if ($viewer -and $viewer.coop_endgame.active -and $viewer.coop_endgame.released -and
                ($viewer.screen_advance_kind -in @('movie', 'briefing') -or $viewer.menu.type -eq 'credits') -and
                -not $viewer.coop_briefing.active -and
                @($viewer.multiplayer.players | Where-Object { -not $_.is_me -and $_.connected -eq 0 }).Count -gt 0) {
                $script:independentEndgameSeen = $true
                $evidence = @{ viewer = $SecondarySerial; departed = $PrimarySerial; ending = $viewer.coop_endgame;
                    screen = $viewer.screen_advance_kind; players = $viewer.multiplayer.players; paused = $viewer.time_paused
                }
                $evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 -LiteralPath (Join-Path $REPO_ROOT "temp/coop-endgame-independent-$Game-$PrimarySerial.json")
                Write-Status "Verified $SecondarySerial still viewing after $PrimarySerial left" "Green"
            }
        }
        return $primaryDone -and $secondaryDone
    }
    if (-not $finished -or
        $script:primaryAutomationResult.result -ne "PASS" -or
        $script:secondaryAutomationResult.result -ne "PASS") {
        Write-Status "FAIL: $Description did not pass" "Red"
        Write-DeviceAutomationDiagnostics -Serial $PrimarySerial
        Write-DeviceAutomationDiagnostics -Serial $SecondarySerial
        return $false
    }
    if ($IndependentEndgame -and -not $script:independentEndgameSeen) {
        Write-Status "FAIL: did not observe local ending content after the other participant left" "Red"
        return $false
    }
    return $true
}

function Invoke-CoopDeathScenario {
    foreach ($dyingSerial in @($EMU2, $EMU1)) {
        $survivorSerial = if ($dyingSerial -eq $EMU1) { $EMU2 } else { $EMU1 }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $EMU1 -PrimaryScript "test_coop_death_prepare.jsonc" `
                    -SecondarySerial $EMU2 -SecondaryScript "test_coop_death_prepare.jsonc" `
                    -Description "Record both ships before $dyingSerial dies")) { return $false }
        if (-not (Invoke-PairedGameAutomation -PrimarySerial $dyingSerial -PrimaryScript "test_coop_death.jsonc" `
                    -SecondarySerial $survivorSerial -SecondaryScript "test_coop_death_survivor.jsonc" `
                    -Description "Respawn $dyingSerial while the other player keeps playing")) { return $false }
    }
    return $true
}
