#!/usr/bin/env pwsh
# test_lan.ps1 -- Two-player LAN multiplayer integration test.
#
# Uses the lan_launch MP_COMMAND to bypass the launcher lobby UI entirely,
# launching the game engine directly in host/join mode on two emulators.
# A UDP relay bridges the two emulators' separate SLIRP NATs.
#
# Prerequisites:
#   - Two emulators running (emulator-5554 and emulator-5556)
#   - APK installed on both
#   - Game data present on both
#
# Usage:
#   .\test_lan.ps1
#   .\test_lan.ps1 -Game d1
#   .\test_lan.ps1 -SkipBuild

param(
    [string]$Game = "d2",
    [switch]$SkipBuild,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# -- Constants --
$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$EMU1 = "emulator-5554"  # Player 1 (host)
$EMU2 = "emulator-5556"  # Player 2 (joiner)
$CALLSIGN1 = "LanHost"
$CALLSIGN2 = "LanJoin"

# Mission filenames as used by the engine (not display names)
$MISSION = if ($Game -eq "d1") { "" } else { "d2" }
$MODE = "coop"

$relayProc = $null
$testPassed = $false

$script:LogFile = Join-Path $REPO_ROOT "temp\lan_test_log.txt"
try { if (Test-Path $script:LogFile) { Remove-Item $script:LogFile -Force -ErrorAction SilentlyContinue } } catch { }
"" | Set-Content -Path $script:LogFile -Encoding utf8 -ErrorAction SilentlyContinue

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    $line = "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg"
    Write-Host $line -ForegroundColor $Color
    $line | Add-Content -Path $script:LogFile -Encoding utf8
}

function Adb-Dev {
    param([string]$Serial, [string[]]$AdbArgs)
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $fullArgs = @("-s", $Serial) + $AdbArgs
    $output = & $ADB @fullArgs 2>&1 | Out-String
    $ErrorActionPreference = $prevEAP
    return $output.Trim()
}

function Adb-Dev-Timeout {
    param([string]$Serial, [string[]]$AdbArgs, [int]$Seconds = 10)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $ADB
    $allArgs = @("-s", $Serial) + $AdbArgs
    $psi.Arguments = ($allArgs | ForEach-Object {
        if ($_ -match '\s') { "`"$_`"" } else { $_ }
    }) -join ' '
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    try {
        $proc = [System.Diagnostics.Process]::Start($psi)
        $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
        $stderrTask = $proc.StandardError.ReadToEndAsync()
        if (-not $proc.WaitForExit($Seconds * 1000)) {
            try { $proc.Kill() } catch {}
            return $null
        }
        $out = $stdoutTask.GetAwaiter().GetResult()
        if ([string]::IsNullOrEmpty($out)) { return "" }
        return $out.Trim()
    } catch {
        return $null
    }
}

function Test-DeviceOnline {
    param([string]$Serial)
    $devices = & $ADB devices 2>&1 | Out-String
    return $devices -match "$Serial\s+device"
}

function Send-MpCommand {
    param([string]$Serial, [string]$Command, [string[]]$Extras = @())
    $args_ = @("shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND",
               "--es", "command", $Command) + $Extras
    Adb-Dev-Timeout -Serial $Serial -AdbArgs $args_ -Seconds 10 | Out-Null
}

function Get-GameIntrospection {
    param([string]$Serial)
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT"
    ) -Seconds 10 | Out-Null
    Start-Sleep -Milliseconds 800
    $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/introspect.json"
    ) -Seconds 5
    if ($json -and $json -match '"screen_mode"') {
        try {
            return $json | ConvertFrom-Json
        } catch {
            return $null
        }
    }
    return $null
}

function Wait-ForCondition {
    param(
        [string]$Description,
        [scriptblock]$Condition,
        [int]$TimeoutSec = 30,
        [int]$PollMs = 1000
    )
    Write-Status "Waiting: $Description (timeout: ${TimeoutSec}s)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $result = & $Condition
        if ($result) {
            Write-Status "  OK: $Description" "Green"
            return $true
        }
        Start-Sleep -Milliseconds $PollMs
    }
    Write-Status "  TIMEOUT: $Description" "Red"
    return $false
}

function Wait-SetupReady {
    param([string]$Serial, [int]$TimeoutSec = 30)
    Adb-Dev -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "rm", "-f", "files/setup_introspect.json"
    ) | Out-Null
    return Wait-ForCondition -Description "SetupActivity ready on $Serial" -TimeoutSec $TimeoutSec -PollMs 2000 -Condition {
        Adb-Dev -Serial $Serial -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT"
        ) | Out-Null
        Start-Sleep -Milliseconds 500
        $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "run-as", $PACKAGE, "cat", "files/setup_introspect.json"
        ) -Seconds 5
        return ($json -and $json -match '"screen"')
    }
}

function Start-SetupActivity {
    param([string]$Serial)
    Write-Status "Force-stopping app on $Serial..."
    Adb-Dev -Serial $Serial -AdbArgs @("shell", "am", "force-stop", $PACKAGE) | Out-Null
    Start-Sleep -Seconds 2
    Adb-Dev -Serial $Serial -AdbArgs @("logcat", "-c") | Out-Null
    Write-Status "Launching SetupActivity on $Serial..."
    Adb-Dev -Serial $Serial -AdbArgs @(
        "shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY"
    ) | Out-Null
    return Wait-SetupReady -Serial $Serial -TimeoutSec 30
}

function Setup-EmulatorRedir {
    param([int]$ConsolePort, [string]$RedirSpec)
    $token = (Get-Content "$env:USERPROFILE\.emulator_console_auth_token").Trim()
    try {
        $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $ConsolePort)
        $s = $c.GetStream()
        $b = New-Object byte[] 4096
        Start-Sleep -Milliseconds 500
        if ($s.DataAvailable) { $s.Read($b, 0, 4096) | Out-Null }
        $w = [System.Text.Encoding]::ASCII.GetBytes("auth $token`r`n")
        $s.Write($w, 0, $w.Length)
        Start-Sleep -Milliseconds 300
        if ($s.DataAvailable) { $s.Read($b, 0, 4096) | Out-Null }
        $w = [System.Text.Encoding]::ASCII.GetBytes("redir $RedirSpec`r`n")
        $s.Write($w, 0, $w.Length)
        Start-Sleep -Milliseconds 300
        $result = ""
        if ($s.DataAvailable) { $n = $s.Read($b, 0, 4096); $result = [System.Text.Encoding]::ASCII.GetString($b, 0, $n) }
        $c.Close()
        return $result.Trim()
    } catch {
        return "ERROR: $($_.Exception.Message)"
    }
}

function Cleanup {
    Write-Status "Cleaning up..."
    $relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
    if (Test-Path $relayLog) {
        $lines = Get-Content $relayLog -ErrorAction SilentlyContinue
        if ($lines) {
            Write-Status "  Relay log ($($lines.Count) lines):" "Gray"
            foreach ($l in ($lines | Select-Object -Last 20)) {
                Write-Status "    $l" "Gray"
            }
        }
    }
    if ($script:relayProc -and -not $script:relayProc.HasExited) {
        Write-Status "Stopping UDP relay (PID $($script:relayProc.Id))..."
        try { $script:relayProc.Kill() } catch {}
    }
}

# ---- Main Test Flow ----

try {

Write-Status "=== LAN Multiplayer Integration Test ===" "White"
Write-Status "Game: $Game | Mission: $MISSION | Mode: $MODE"
Write-Status ""

# -- Step 0: Verify both emulators are online --
Write-Status "Checking emulators..."
if (-not (Test-DeviceOnline -Serial $EMU1)) {
    Write-Status "FAIL: Emulator $EMU1 is not online" "Red"; exit 1
}
if (-not (Test-DeviceOnline -Serial $EMU2)) {
    Write-Status "FAIL: Emulator $EMU2 is not online" "Red"; exit 1
}
Write-Status "Both emulators online" "Green"

# Verify game data
foreach ($emu in @($EMU1, $EMU2)) {
    $files = Adb-Dev-Timeout -Serial $emu -AdbArgs @(
        "shell", "run-as", $PACKAGE, "ls", "files/sets/default/"
    ) -Seconds 5
    $required = if ($Game -eq "d1") { "descent.hog" } else { "descent2.hog" }
    if (-not $files -or $files -notmatch "(?i)$required") {
        Write-Status "FAIL: Game data missing on $emu (need $required)" "Red"; exit 1
    }
}
Write-Status "Game data verified on both emulators" "Green"

# -- Step 1: Launch SetupActivity on both --
Write-Status ""
Write-Status "--- Phase 1: Launch SetupActivity on both emulators ---" "White"

if (-not (Start-SetupActivity -Serial $EMU1)) {
    Write-Status "FAIL: SetupActivity didn't start on $EMU1" "Red"; Cleanup; exit 1
}
if (-not (Start-SetupActivity -Serial $EMU2)) {
    Write-Status "FAIL: SetupActivity didn't start on $EMU2" "Red"; Cleanup; exit 1
}
Write-Status "SetupActivity ready on both emulators" "Green"

# -- Step 2: Set up UDP relay for cross-emulator engine traffic --
Write-Status ""
Write-Status "--- Phase 2: Set up UDP relay ---" "White"

# EMU1 redir: host:42500 -> EMU1:42424 (relay -> host game inbound)
$r1del = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "del udp:42500"
Write-Status "  EMU1 redir cleanup: $r1del" "Gray"
$r1 = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "add udp:42500:42424"
Write-Status "  EMU1 redir add: $r1" "Gray"

# EMU2: remove stale redir
$r2del = Setup-EmulatorRedir -ConsolePort 5556 -RedirSpec "del udp:42501"
Write-Status "  EMU2 redir cleanup: $r2del" "Gray"

# Start UDP relay
$relayScript = Join-Path $PSScriptRoot "..\udp_relay.py"
if (-not (Test-Path $relayScript)) {
    Write-Status "FAIL: udp_relay.py not found at $relayScript" "Red"; exit 1
}
$relayProc = Start-Process python -ArgumentList "-u",$relayScript -PassThru -NoNewWindow `
    -RedirectStandardOutput (Join-Path $REPO_ROOT "temp\udp_relay.log") `
    -RedirectStandardError (Join-Path $REPO_ROOT "temp\udp_relay_err.log")
$script:relayProc = $relayProc
Write-Status "UDP relay started (PID $($relayProc.Id))" "Green"
Start-Sleep -Seconds 1

# -- Step 3: Launch game on both emulators via lan_launch --
Write-Status ""
Write-Status "--- Phase 3: Launch game via lan_launch ---" "White"

# Start logcat capture BEFORE lan_launch so we catch all MPDIAG messages.
$logcatFile1 = Join-Path $REPO_ROOT "temp\lan_emu1_logcat.txt"
$logcatFile2 = Join-Path $REPO_ROOT "temp\lan_emu2_logcat.txt"
& $ADB -s $EMU1 logcat -c 2>&1 | Out-Null
& $ADB -s $EMU2 logcat -c 2>&1 | Out-Null
$logcatProc1 = Start-Process -FilePath $ADB -ArgumentList "-s",$EMU1,"logcat","-s","DXX-MP:*","dxxredux:*","AndroidRuntime:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile1 -RedirectStandardError (Join-Path $REPO_ROOT "temp\lan_emu1_logcat_err.txt")
$logcatProc2 = Start-Process -FilePath $ADB -ArgumentList "-s",$EMU2,"logcat","-s","DXX-MP:*","dxxredux:*","AndroidRuntime:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile2 -RedirectStandardError (Join-Path $REPO_ROOT "temp\lan_emu2_logcat_err.txt")
Start-Sleep -Seconds 1

# Host (EMU1)
Write-Status "Sending lan_launch host to $EMU1..."
Send-MpCommand -Serial $EMU1 -Command "lan_launch" -Extras @(
    "--es", "game", $Game,
    "--es", "mp_mode", "host",
    "--es", "mission", $MISSION,
    "--es", "mode", $MODE,
    "--ei", "max_players", "2",
    "--ei", "level_num", "1",
    "--ei", "difficulty", "1",
    "--es", "callsign", $CALLSIGN1
)

# Give host time to start the engine and enter multiplayer hosting mode
Start-Sleep -Seconds 8

# Joiner (EMU2) - point at relay address (10.0.2.2 = host loopback from emulator)
# Port 42600 is the UDP relay on the host machine, NOT the engine port.
Write-Status "Sending lan_launch join to $EMU2..."
Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras @(
    "--es", "game", $Game,
    "--es", "mp_mode", "join",
    "--es", "mission", $MISSION,
    "--es", "mode", $MODE,
    "--ei", "max_players", "2",
    "--ei", "level_num", "1",
    "--ei", "difficulty", "1",
    "--es", "callsign", $CALLSIGN2,
    "--es", "host_addr", "10.0.2.2",
    "--ei", "host_port", "42600"
)

# -- Step 4: Verify multiplayer sync via MPDIAG logcat --
#
# The success criterion is that the host's MPDIAG logs show:
#   1) Both players added (add_player ... N_players now 2)
#   2) Level sync completed (send_sync ... sending SYNC to all)
#
# This proves the full LAN networking path works:
#   EMU2 -> LAN proxy -> relay -> EMU1 redir -> host engine
#   and back.
#
# We do NOT require both emulators to survive the level load phase
# (GPU rendering on swiftshader_indirect can crash the emulator).

Write-Status ""
Write-Status "--- Phase 4: Wait for multiplayer sync ---" "White"

$script:pollCount = 0
$syncOk = Wait-ForCondition -Description "MPDIAG sync on host" -TimeoutSec $TimeoutSeconds -PollMs 3000 -Condition {
    $script:pollCount++
    if (-not (Test-Path $logcatFile1)) { return $false }
    $lines = Get-Content $logcatFile1 -ErrorAction SilentlyContinue
    $hasSync = $lines | Where-Object { $_ -match 'send_sync.*sending SYNC to all' }
    $hasTwoPlayers = $lines | Where-Object { $_ -match 'N_players now 2' }
    $lastMpdiag = ($lines | Where-Object { $_ -match 'MPDIAG' } | Select-Object -Last 1)
    if ($lastMpdiag) {
        Write-Status "  [poll $($script:pollCount)] last MPDIAG: $($lastMpdiag.Substring([Math]::Max(0,$lastMpdiag.Length - 80)))" "Gray"
    } else {
        Write-Status "  [poll $($script:pollCount)] no MPDIAG yet" "Gray"
    }
    return ($hasSync -and $hasTwoPlayers)
}

if (-not $syncOk) {
    Write-Status "FAIL: Multiplayer sync never completed on host" "Red"
    if (Test-Path $logcatFile1) {
        Write-Status "  EMU1 MPDIAG lines:" "Gray"
        Get-Content $logcatFile1 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG' } | ForEach-Object { Write-Status "    $_" "Gray" }
    }
    if (Test-Path $logcatFile2) {
        Write-Status "  EMU2 MPDIAG lines:" "Gray"
        Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG' } | ForEach-Object { Write-Status "    $_" "Gray" }
    }
    Cleanup; exit 1
}

Write-Status "Multiplayer sync completed" "Green"

# -- Step 5: Verify results --
Write-Status ""
Write-Status "--- Phase 5: Verify networking ---" "White"

# Check that relay forwarded traffic in both directions
$relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
$relayLines = @()
if (Test-Path $relayLog) {
    $relayLines = Get-Content $relayLog -ErrorAction SilentlyContinue
}
$emu1ToEmu2 = @($relayLines | Where-Object { $_ -match 'EMU1->EMU2' }).Count
$emu2ToEmu1 = @($relayLines | Where-Object { $_ -match 'EMU2->EMU1' }).Count
Write-Status "Relay traffic: EMU1->EMU2: $emu1ToEmu2 packets, EMU2->EMU1: $emu2ToEmu1 packets"

# Check MPDIAG details from host logcat
$hostLines = Get-Content $logcatFile1 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG' }
Write-Status "Host MPDIAG log ($($hostLines.Count) lines):" "Gray"
foreach ($line in $hostLines) {
    Write-Status "  $line" "Gray"
}

$testPassed = $true

if ($emu1ToEmu2 -eq 0 -or $emu2ToEmu1 -eq 0) {
    Write-Status "FAIL: Relay did not forward traffic in both directions" "Red"
    $testPassed = $false
}

# Optionally check in-game state if emulators are still alive
$gi1 = Get-GameIntrospection -Serial $EMU1
$gi2 = Get-GameIntrospection -Serial $EMU2
if ($gi1) {
    Write-Status "EMU1: screen=$($gi1.screen_mode) in_game=$($gi1.in_game) game_mode=$($gi1.game_mode)"
} else {
    Write-Status "EMU1: introspection unavailable (emulator may have crashed during level load)" "Yellow"
}
if ($gi2) {
    Write-Status "EMU2: screen=$($gi2.screen_mode) in_game=$($gi2.in_game) game_mode=$($gi2.game_mode)"
} else {
    Write-Status "EMU2: introspection unavailable (emulator may have crashed during level load)" "Yellow"
}

# Stop logcat capture
try { if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force } } catch {}
try { if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force } } catch {}

Write-Status ""
if ($testPassed) {
    Write-Status "=== LAN MP TEST PASSED ===" "Green"
} else {
    Write-Status "=== LAN MP TEST FAILED ===" "Red"
}

} finally {
    Cleanup
}

exit $(if ($testPassed) { 0 } else { 1 })
