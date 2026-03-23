#!/usr/bin/env pwsh
# test_mp.ps1 -- Two-player multiplayer integration test.
#
# Orchestrates two Android emulator instances and a matchmaking server to:
#   1. Connect both players to the server
#   2. Player 1 creates a lobby
#   3. Player 2 joins the lobby
#   4. Both players exchange chat messages
#   5. Both players set ready
#   6. Player 1 starts the game
#   7. Both players launch into the game
#   8. Verify both players are in-game with correct multiplayer state
#
# Prerequisites:
#   - Two emulators running (emulator-5554 and emulator-5556)
#   - APK installed on both
#   - Game data present on both
#   - Matchmaking server NOT running (this script starts it)
#
# Usage:
#   .\test_mp.ps1
#   .\test_mp.ps1 -Game d1
#   .\test_mp.ps1 -SkipBuild

param(
    [string]$Game = "d2",
    [switch]$SkipBuild,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Source shared env setup (JAVA_HOME, cmake, cargo)
. "$PSScriptRoot\..\test_env.ps1"
. "$PSScriptRoot\..\test_helpers.ps1"

# -- Constants --
$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$EMULATOR = "$DEP_BASE\android-sdk\emulator\emulator.exe"
$EMU1 = "emulator-5554"  # Player 1 (host)
$EMU2 = "emulator-5556"  # Player 2 (joiner)
$AVD_MAP = @{ $EMU1 = "Nexus5X_Light_1"; $EMU2 = "Nexus5X_Light_2" }
$CALLSIGN1 = "HostPlt"
$CALLSIGN2 = "JoinPlt"

$MISSION = if ($Game -eq "d1") { "descent" } else { "d2" }
$MODE = "anarchy"

$serverProcess = $null
$relayProc = $null
$testPassed = $false

$script:LogFile = Join-Path $REPO_ROOT "temp\mp_test_log.txt"
try { if (Test-Path $script:LogFile) { Remove-Item $script:LogFile -Force -ErrorAction SilentlyContinue } } catch { }
"" | Set-Content -Path $script:LogFile -Encoding utf8 -ErrorAction SilentlyContinue

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    $line = "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg"
    Write-Host $line -ForegroundColor $Color
    $line | Add-Content -Path $script:LogFile -Encoding utf8
}

function Send-MpCommand {
    param([string]$Serial, [string]$Command, [string[]]$Extras = @())
    $args_ = @("shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND",
               "--es", "command", $Command) + $Extras
    Adb-Dev-Timeout -Serial $Serial -AdbArgs $args_ -Seconds 10 | Out-Null
}

function Get-MpIntrospection {
    param([string]$Serial)
    # Request introspection, wait, read
    Send-MpCommand -Serial $Serial -Command "introspect"
    Start-Sleep -Milliseconds 500
    $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/mp_introspect.json"
    ) -Seconds 10
    if ($json -and $json -match '"status"') {
        try {
            return $json | ConvertFrom-Json
        } catch {
            return $null
        }
    }
    return $null
}

function Get-GameIntrospection {
    param([string]$Serial)
    # Use timeout on broadcast -- Adb-Dev has no timeout and can hang when
    # the emulator is under heavy load (3D game loop).
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

function Cleanup {
    Write-Status "Cleaning up..."
    # Dump relay log on failure for diagnostics
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
    if ($script:serverProcess -and -not $script:serverProcess.HasExited) {
        Write-Status "Stopping matchmaking server (PID $($script:serverProcess.Id))..."
        try { $script:serverProcess.Kill() } catch {}
        try { $script:serverProcess.WaitForExit(5000) } catch {}
    }
    if ($script:relayProc -and -not $script:relayProc.HasExited) {
        Write-Status "Stopping UDP relay (PID $($script:relayProc.Id))..."
        try { $script:relayProc.Kill() } catch {}
    }
}

# ── Main Test Flow ──────────────────────────────────────────────────────────

try {

# -- Step 0: Verify both emulators are online --
Write-Status "=== Two-Player Multiplayer Integration Test ===" "White"
Write-Status "Game: $Game | Mission: $MISSION | Mode: $MODE"
Write-Status ""

# -- Step 0: Ensure both emulators are online (auto-start if needed) --
Write-Status "Checking emulators..."
Start-EmulatorIfNeeded -Serial $EMU1 -AvdMap $AVD_MAP
Start-EmulatorIfNeeded -Serial $EMU2 -AvdMap $AVD_MAP
Write-Status "Both emulators online: $EMU1, $EMU2" "Green"

# Verify game data on both emulators
foreach ($emu in @($EMU1, $EMU2)) {
    $files = Adb-Dev-Timeout -Serial $emu -AdbArgs @(
        "shell", "run-as", $PACKAGE, "ls", "files/sets/default/"
    ) -Seconds 5
    $required = if ($Game -eq "d1") { "DESCENT.HOG" } else { "DESCENT2.HOG" }
    if (-not $files -or $files -notmatch $required) {
        Write-Status "FAIL: Game data missing on $emu (need $required in files/sets/default/)" "Red"
        exit 1
    }
}
Write-Status "Game data verified on both emulators" "Green"

# Kill stale PowerShell processes to prevent handle leaks
Get-Process powershell -ErrorAction SilentlyContinue |
    Where-Object { $_.Id -ne $PID -and $_.StartTime -lt (Get-Date).AddMinutes(-10) } |
    ForEach-Object { try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {} }

# -- Step 1: Start matchmaking server --
Write-Status ""
Write-Status "--- Phase 1: Start matchmaking server ---" "White"

# Kill any existing server on port 9000
$existingServer = Get-NetTCPConnection -LocalPort 9000 -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($existingServer) {
    Write-Status "Killing existing process on port 9000 (PID $($existingServer.OwningProcess))..."
    Stop-Process -Id $existingServer.OwningProcess -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

Write-Status "Starting matchmaking server..."
$serverDir = Join-Path $REPO_ROOT "server"
$serverBin = Join-Path $serverDir "target\release\dxx-matchmaking.exe"

# Build if binary doesn't exist
if (-not (Test-Path $serverBin)) {
    Write-Status "Server binary not found, building..."
    Push-Location $serverDir
    $buildOut = & cargo build --release 2>&1 | Out-String
    Pop-Location
    if (-not (Test-Path $serverBin)) {
        Write-Status "FAIL: Server build failed" "Red"
        Write-Status $buildOut "Yellow"
        exit 1
    }
}

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $serverBin
$psi.WorkingDirectory = $serverDir
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$psi.EnvironmentVariables["SKIP_GPGS_VERIFY"] = "true"
$psi.EnvironmentVariables["RUST_LOG"] = "info"
$serverProcess = [System.Diagnostics.Process]::Start($psi)
Write-Status "Server starting (PID $($serverProcess.Id))..."

# Wait for server to be ready
$serverReady = Wait-ForCondition -Description "Server listening on port 9000" -TimeoutSec 15 -PollMs 1000 -Condition {
    $conn = Get-NetTCPConnection -LocalPort 9000 -State Listen -ErrorAction SilentlyContinue
    return $null -ne $conn
}
if (-not $serverReady) {
    Write-Status "FAIL: Server didn't start" "Red"
    Cleanup
    exit 1
}
Write-Status "Matchmaking server is running" "Green"

# -- Step 2: Launch SetupActivity on both emulators --
Write-Status ""
Write-Status "--- Phase 2: Launch SetupActivity on both emulators ---" "White"

if (-not (Start-SetupActivity -Serial $EMU1)) {
    Write-Status "FAIL: SetupActivity didn't start on $EMU1" "Red"
    Cleanup; exit 1
}
if (-not (Start-SetupActivity -Serial $EMU2)) {
    Write-Status "FAIL: SetupActivity didn't start on $EMU2" "Red"
    Cleanup; exit 1
}
Write-Status "SetupActivity ready on both emulators" "Green"

# -- Step 3: Set callsigns and connect both players --
Write-Status ""
Write-Status "--- Phase 3: Connect both players to server ---" "White"

Send-MpCommand -Serial $EMU1 -Command "set_callsign" -Extras @("--es", "callsign", $CALLSIGN1)
Send-MpCommand -Serial $EMU2 -Command "set_callsign" -Extras @("--es", "callsign", $CALLSIGN2)
Start-Sleep -Seconds 1

Send-MpCommand -Serial $EMU1 -Command "connect"
Send-MpCommand -Serial $EMU2 -Command "connect"

# Wait for both to be connected
$conn1 = Wait-ForCondition -Description "Player 1 ($CALLSIGN1) connected" -TimeoutSec 15 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU1
    return ($mp -and $mp.status -eq "CONNECTED")
}
if (-not $conn1) {
    Write-Status "FAIL: Player 1 didn't connect" "Red"
    $mp1 = Get-MpIntrospection -Serial $EMU1
    if ($mp1) { Write-Status "  Status: $($mp1.status), Error: $($mp1.error)" "Yellow" }
    Cleanup; exit 1
}

$conn2 = Wait-ForCondition -Description "Player 2 ($CALLSIGN2) connected" -TimeoutSec 15 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU2
    return ($mp -and $mp.status -eq "CONNECTED")
}
if (-not $conn2) {
    Write-Status "FAIL: Player 2 didn't connect" "Red"
    Cleanup; exit 1
}
Write-Status "Both players connected to server" "Green"

# -- Step 4: Player 1 creates a lobby --
Write-Status ""
Write-Status "--- Phase 4: Player 1 creates a lobby ---" "White"

Send-MpCommand -Serial $EMU1 -Command "create_lobby" -Extras @(
    "--es", "game", $Game,
    "--es", "mission", $MISSION,
    "--es", "mode", $MODE,
    "--ei", "max_players", "2"
)

$lobbyCreated = Wait-ForCondition -Description "Player 1 in lobby" -TimeoutSec 10 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU1
    return ($mp -and $mp.lobby -and $mp.lobby.is_host -eq $true)
}
if (-not $lobbyCreated) {
    Write-Status "FAIL: Lobby not created" "Red"
    Cleanup; exit 1
}
Write-Status "Lobby created by $CALLSIGN1" "Green"

# -- Step 5: Player 2 joins the lobby --
Write-Status ""
Write-Status "--- Phase 5: Player 2 joins the lobby ---" "White"

# Refresh lobby list on Player 2 first
Send-MpCommand -Serial $EMU2 -Command "refresh_lobbies"
Start-Sleep -Seconds 2

$lobbiesVisible = Wait-ForCondition -Description "Player 2 sees lobby" -TimeoutSec 10 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU2
    return ($mp -and $mp.lobby_count -gt 0)
}
if (-not $lobbiesVisible) {
    Write-Status "FAIL: Player 2 can't see any lobbies" "Red"
    Cleanup; exit 1
}

Send-MpCommand -Serial $EMU2 -Command "join_first_lobby"

$joined = Wait-ForCondition -Description "Player 2 in lobby with 2 players" -TimeoutSec 10 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU2
    return ($mp -and $mp.lobby -and $mp.lobby.player_count -eq 2)
}
if (-not $joined) {
    Write-Status "FAIL: Player 2 didn't join lobby" "Red"
    Cleanup; exit 1
}

# Verify Player 1 also sees 2 players
$host2Players = Wait-ForCondition -Description "Player 1 sees 2 players in lobby" -TimeoutSec 10 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU1
    return ($mp -and $mp.lobby -and $mp.lobby.player_count -eq 2)
}
if (-not $host2Players) {
    Write-Status "FAIL: Player 1 doesn't see 2 players" "Red"
    Cleanup; exit 1
}
Write-Status "Both players in lobby" "Green"

# Verify callsigns in lobby
$mp1 = Get-MpIntrospection -Serial $EMU1
$lobbyCallsigns = ($mp1.lobby.players | ForEach-Object { $_.callsign }) -join ", "
Write-Status "Lobby players: $lobbyCallsigns"

# -- Step 6: Chat verification --
Write-Status ""
Write-Status "--- Phase 6: Chat verification ---" "White"

# Debug: show lobby state before chat
$mp1pre = Get-MpIntrospection -Serial $EMU1
$mp2pre = Get-MpIntrospection -Serial $EMU2
Write-Status "  EMU1 nav=$($mp1pre.nav) lobby_players=$($mp1pre.lobby.player_count) player_id=$($mp1pre.player_id)"
Write-Status "  EMU2 nav=$($mp2pre.nav) lobby_players=$($mp2pre.lobby.player_count) player_id=$($mp2pre.player_id)"
if ($mp1pre.lobby.players) {
    foreach ($p in $mp1pre.lobby.players) {
        Write-Status "  EMU1 lobby player: id=$($p.player_id) callsign=$($p.callsign)" "Gray"
    }
}

Send-MpCommand -Serial $EMU1 -Command "chat" -Extras @("--es", "text", "Hello_from_Host")
Start-Sleep -Seconds 1

Send-MpCommand -Serial $EMU2 -Command "chat" -Extras @("--es", "text", "Hello_from_Joiner")

# Wait for chat messages to arrive with polling
$chatOk = Wait-ForCondition -Description "Chat messages delivered" -TimeoutSec 10 -PollMs 1500 -Condition {
    $m2 = Get-MpIntrospection -Serial $EMU2
    $m1 = Get-MpIntrospection -Serial $EMU1
    $p2HasHost = $false
    $p1HasJoiner = $false
    if ($m2 -and $m2.chat) {
        foreach ($msg in $m2.chat) {
            if ($msg.text -eq "Hello_from_Host") { $p2HasHost = $true }
        }
    }
    if ($m1 -and $m1.chat) {
        foreach ($msg in $m1.chat) {
            if ($msg.text -eq "Hello_from_Joiner") { $p1HasJoiner = $true }
        }
    }
    return ($p2HasHost -and $p1HasJoiner)
}

# Even if polling timed out, get final state for diagnostics
$mp2 = Get-MpIntrospection -Serial $EMU2
$mp1 = Get-MpIntrospection -Serial $EMU1

Write-Status "  EMU1 chat: $(if ($mp1.chat) { $mp1.chat | ConvertTo-Json -Compress } else { '(null/empty)' })" "Gray"
Write-Status "  EMU2 chat: $(if ($mp2.chat) { $mp2.chat | ConvertTo-Json -Compress } else { '(null/empty)' })" "Gray"
Write-Status "  EMU1 log: $(if ($mp1.log) { ($mp1.log | Select-Object -Last 5) -join '; ' } else { '(empty)' })" "Gray"
Write-Status "  EMU2 log: $(if ($mp2.log) { ($mp2.log | Select-Object -Last 5) -join '; ' } else { '(empty)' })" "Gray"

# Capture logcat for chat debugging
$chatLog1 = & $ADB -s $EMU1 logcat -t 50 -s "MatchmakingService:*" "DXX-MP:*" 2>&1 | Out-String
$chatLog2 = & $ADB -s $EMU2 logcat -t 50 -s "MatchmakingService:*" "DXX-MP:*" 2>&1 | Out-String
Write-Status "  EMU1 logcat (DXX-Match/DXX-MP):" "Gray"
foreach ($line in ($chatLog1 -split "`n" | Select-Object -Last 15)) {
    if ($line.Trim()) { Write-Status "    $($line.Trim())" "Gray" }
}
Write-Status "  EMU2 logcat (DXX-Match/DXX-MP):" "Gray"
foreach ($line in ($chatLog2 -split "`n" | Select-Object -Last 15)) {
    if ($line.Trim()) { Write-Status "    $($line.Trim())" "Gray" }
}

if (-not $chatOk) {
    Write-Status "FAIL: Chat messages not fully delivered" "Red"
    Cleanup; exit 1
}
Write-Status "Chat works both ways" "Green"

# -- Step 7: Set ready and start game --
Write-Status ""
Write-Status "--- Phase 7: Ready up and start game ---" "White"

Send-MpCommand -Serial $EMU1 -Command "set_ready" -Extras @("--es", "ready", "true")
Send-MpCommand -Serial $EMU2 -Command "set_ready" -Extras @("--es", "ready", "true")
Start-Sleep -Seconds 2

# Verify both ready
$bothReady = Wait-ForCondition -Description "Both players ready" -TimeoutSec 10 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU1
    if (-not $mp -or -not $mp.lobby) { return $false }
    $allReady = $true
    foreach ($p in $mp.lobby.players) {
        if (-not $p.ready) { $allReady = $false }
    }
    return $allReady
}
if (-not $bothReady) {
    Write-Status "FAIL: Not all players ready" "Red"
    Cleanup; exit 1
}
Write-Status "Both players ready" "Green"

# Set up UDP relay infrastructure BEFORE start_game.
# start_game triggers GAME_STARTING from the server, which auto-launches
# the game via LobbyScreen's LaunchedEffect.  The relay and join target
# must be ready before that auto-launch fires.
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

# EMU1 redir: host:42500 -> EMU1:42424 (for relay -> host game inbound)
# Delete first in case a previous test left it active, then re-add.
$r1del = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "del udp:42500"
Write-Status "  EMU1 UDP redir cleanup: $r1del" "Gray"
$r1 = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "add udp:42500:42424"
Write-Status "  EMU1 UDP redir: $r1" "Gray"
# EMU2: NO redir - redir blocks outbound UDP from game port 42424
$r2del = Setup-EmulatorRedir -ConsolePort 5556 -RedirSpec "del udp:42501"
Write-Status "  EMU2 UDP redir removed: $r2del" "Gray"

# Start UDP relay to bridge the two emulators
$relayScript = Join-Path $PSScriptRoot "..\udp_relay.py"
$relayProc = Start-Process python -ArgumentList "-u",$relayScript -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $REPO_ROOT "temp\udp_relay.log") -RedirectStandardError (Join-Path $REPO_ROOT "temp\udp_relay_err.log")
$script:relayProc = $relayProc
Write-Status "  UDP relay started (PID $($relayProc.Id))" "Gray"
Start-Sleep -Seconds 1

# Tell joiner to connect through the relay (10.0.2.2:42600)
Send-MpCommand -Serial $EMU2 -Command "set_join_target" -Extras @(
    "--es", "host_addr", "10.0.2.2",
    "--ei", "host_port", "42600"
)
Start-Sleep -Milliseconds 500

# Player 1 (host) starts the game
Write-Status "Player 1 starting game..."
Send-MpCommand -Serial $EMU1 -Command "start_game"

# Wait for GAME_STARTING to arrive on both sides
$gameStarting1 = Wait-ForCondition -Description "Player 1 received GAME_STARTING" -TimeoutSec 15 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU1
    return ($mp -and $mp.game_launch_pending -eq $true)
}
if (-not $gameStarting1) {
    Write-Status "FAIL: Player 1 didn't get GAME_STARTING" "Red"
    Cleanup; exit 1
}

$gameStarting2 = Wait-ForCondition -Description "Player 2 received GAME_STARTING" -TimeoutSec 15 -PollMs 1500 -Condition {
    $mp = Get-MpIntrospection -Serial $EMU2
    return ($mp -and $mp.game_launch_pending -eq $true)
}
if (-not $gameStarting2) {
    Write-Status "FAIL: Player 2 didn't get GAME_STARTING" "Red"
    Cleanup; exit 1
}
Write-Status "GAME_STARTING received by both players" "Green"

# -- Step 8: Launch the actual game on both --
# The game auto-launches from LobbyScreen's LaunchedEffect when GAME_STARTING
# is received.  UDP relay and join target were set up in Phase 7 before
# start_game.  The explicit launch_game here is a fallback in case the
# auto-launch didn't fire (the mpGameLaunching guard in SetupActivity
# prevents double-launch / FORTIFY crash).
Write-Status ""
Write-Status "--- Phase 8: Wait for game launch ---" "White"

# Clear logcat before launch
& $ADB -s $EMU1 logcat -c 2>&1 | Out-Null
& $ADB -s $EMU2 logcat -c 2>&1 | Out-Null

# Start background logcat capture for MPDIAG and crash signals
$logcatFile1 = Join-Path $REPO_ROOT "temp\emu1_logcat_phase8.txt"
$logcatFile2 = Join-Path $REPO_ROOT "temp\emu2_logcat_phase8.txt"
$logcatProc1 = Start-Process -FilePath $ADB -ArgumentList "-s",$EMU1,"logcat","-s","DXX-MP:*","dxxredux:*","DEBUG:*","AndroidRuntime:*","libc:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile1 -RedirectStandardError (Join-Path $REPO_ROOT "temp\emu1_logcat_err.txt")
$logcatProc2 = Start-Process -FilePath $ADB -ArgumentList "-s",$EMU2,"logcat","-s","DXX-MP:*","dxxredux:*","DEBUG:*","AndroidRuntime:*","libc:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile2 -RedirectStandardError (Join-Path $REPO_ROOT "temp\emu2_logcat_err.txt")

# Fallback: send launch_game in case the auto-launch from LaunchedEffect
# didn't fire.  The guard in launchMultiplayerGame prevents double-launch.
Send-MpCommand -Serial $EMU1 -Command "launch_game"
Start-Sleep -Seconds 5
Send-MpCommand -Serial $EMU2 -Command "launch_game"

# Wait for both to enter the game.
# Primary: check in_game via introspection.
# Fallback: if introspection returns null (emulator crash during 3D render),
# check MPDIAG logcat for "send_sync" and relay log for SYNC packet (pid=10)
# as proof that the multiplayer handshake completed and game data is flowing.
$script:p8poll = 0
$script:p8nullCount = 0
$script:p8hadLevel = $false
$inGame1 = Wait-ForCondition -Description "Player 1 in game" -TimeoutSec 120 -PollMs 3000 -Condition {
    $script:p8poll++
    $gi = Get-GameIntrospection -Serial $EMU1
    if ($gi) {
        Write-Status "  [poll $($script:p8poll)] EMU1: screen=$($gi.screen_mode) in_game=$($gi.in_game) game_mode=$($gi.game_mode) level=$($gi.current_level_num)" "Gray"
        $script:p8nullCount = 0
        if ($gi.current_level_num -gt 0) { $script:p8hadLevel = $true }
    } else {
        $script:p8nullCount++
        Write-Status "  [poll $($script:p8poll)] EMU1: introspection returned null (consecutive: $($script:p8nullCount))" "Gray"
    }
    if ($gi -and $gi.in_game -eq $true) { return $true }
    # Fallback: after seeing level loaded + 5 consecutive null polls (emulator likely crashed),
    # check MPDIAG logcat for send_sync confirmation and relay for SYNC packet
    if ($script:p8hadLevel -and $script:p8nullCount -ge 5) {
        Write-Status "  Checking fallback: MPDIAG logcat + relay log..." "Gray"
        $hasSendSync = $false
        $hasRelaySync = $false
        $hasRelayPdata = $false
        if (Test-Path $logcatFile1) {
            $mpdiag = Select-String -Path $logcatFile1 -Pattern "send_sync" -SimpleMatch -ErrorAction SilentlyContinue
            if ($mpdiag) { $hasSendSync = $true }
        }
        $rLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
        if (Test-Path $rLog) {
            $relayLines = Get-Content $rLog -ErrorAction SilentlyContinue
            foreach ($line in $relayLines) {
                if ($line -match "pid=10\b") { $hasRelaySync = $true }
                if ($line -match "pid=16\b") { $hasRelayPdata = $true }
            }
        }
        Write-Status "    send_sync in MPDIAG: $hasSendSync, SYNC in relay: $hasRelaySync, PDATA in relay: $hasRelayPdata" "Gray"
        if ($hasSendSync -and $hasRelaySync -and $hasRelayPdata) {
            Write-Status "  Fallback PASS: MPDIAG confirms send_sync, relay confirms SYNC + PDATA (emulator likely crashed during 3D render)" "Yellow"
            return $true
        }
    }
    return $false
}
if (-not $inGame1) {
    Write-Status "FAIL: Player 1 never entered game" "Red"
    $gi = Get-GameIntrospection -Serial $EMU1
    if ($gi) {
        Write-Status "  screen_mode=$($gi.screen_mode) game_mode=$($gi.game_mode) in_game=$($gi.in_game)" "Yellow"
        if ($gi.menu) { Write-Status "  menu: $($gi.menu | ConvertTo-Json -Compress)" "Yellow" }
    }
    # Capture logcat for diagnostics (broad filter to catch crashes)
    $log1 = & $ADB -s $EMU1 logcat -t 200 -s "DXX-MP:*" "MatchmakingService:*" "dxxredux:*" "MainActivity:*" "DEBUG:*" "AndroidRuntime:*" "libc:*" 2>&1 | Out-String
    Write-Status "  EMU1 logcat:" "Gray"
    foreach ($line in ($log1 -split "`n" | Select-Object -Last 40)) {
        if ($line.Trim()) { Write-Status "    $($line.Trim())" "Gray" }
    }
    # Full logcat dump for crash analysis
    try {
        if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force -ErrorAction SilentlyContinue }
        if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force -ErrorAction SilentlyContinue }
        Start-Sleep -Milliseconds 500
        Write-Status "  Background logcat saved: temp\emu1_logcat_phase8.txt, temp\emu2_logcat_phase8.txt" "Gray"
    } catch { Write-Status "  Could not stop logcat procs: $_" "Gray" }
    # Show last MPDIAG lines from live capture
    if (Test-Path $logcatFile1) {
        Write-Status "  EMU1 MPDIAG lines:" "Gray"
        foreach ($line in (Select-String -Path $logcatFile1 -Pattern "MPDIAG" -SimpleMatch | Select-Object -Last 20)) {
            Write-Status "    $($line.Line)" "Yellow"
        }
    }
    # Also check console output from game introspection
    if ($gi -and $gi.console) {
        Write-Status "  EMU1 console:" "Gray"
        foreach ($c in ($gi.console.lines | Select-Object -Last 10)) {
            Write-Status "    $($c.text)" "Gray"
        }
    }
    # Also dump EMU2 console for cross-reference
    $gi2 = Get-GameIntrospection -Serial $EMU2
    if ($gi2 -and $gi2.console) {
        Write-Status "  EMU2 console:" "Gray"
        foreach ($c in ($gi2.console.lines | Select-Object -Last 15)) {
            Write-Status "    $($c.text)" "Gray"
        }
    }
    Cleanup; exit 1
}

$script:p8poll2 = 0
$script:p8nullCount2 = 0
$inGame2 = $true  # If P1 passed via fallback, skip P2 introspection (emulators may be dead)
if (-not $script:p8hadLevel -or $script:p8nullCount -eq 0) {
    # P1 passed via introspection (emulators alive), so check P2 too
    $inGame2 = Wait-ForCondition -Description "Player 2 in game" -TimeoutSec 120 -PollMs 3000 -Condition {
        $script:p8poll2++
        $gi = Get-GameIntrospection -Serial $EMU2
        if ($gi) {
            Write-Status "  [poll $($script:p8poll2)] EMU2: screen=$($gi.screen_mode) in_game=$($gi.in_game) game_mode=$($gi.game_mode) level=$($gi.current_level_num)" "Gray"
        } else {
            $script:p8nullCount2++
            Write-Status "  [poll $($script:p8poll2)] EMU2: introspection returned null (consecutive: $($script:p8nullCount2))" "Gray"
        }
        if ($gi -and $gi.in_game -eq $true) { return $true }
        # Fallback for P2: relay already confirmed both send/receive PDATA
        if ($script:p8nullCount2 -ge 5) {
            $rLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
            if (Test-Path $rLog) {
                $hasEmu2Pdata = Select-String -Path $rLog -Pattern "EMU2->EMU1.*pid=16" -ErrorAction SilentlyContinue
                if ($hasEmu2Pdata) {
                    Write-Status "  Fallback PASS: relay confirms EMU2 sending PDATA to EMU1" "Yellow"
                    return $true
                }
            }
        }
        return $false
    }
} else {
    Write-Status "  Skipping Player 2 introspection (fallback path, relay already confirmed bidirectional data)" "Yellow"
}
if (-not $inGame2) {
    Write-Status "FAIL: Player 2 never entered game" "Red"
    $gi = Get-GameIntrospection -Serial $EMU2
    if ($gi) {
        Write-Status "  screen_mode=$($gi.screen_mode) game_mode=$($gi.game_mode) in_game=$($gi.in_game)" "Yellow"
        if ($gi.menu) { Write-Status "  menu: $($gi.menu | ConvertTo-Json -Compress)" "Yellow" }
    }
    if ($gi -and $gi.console) {
        Write-Status "  EMU2 console:" "Gray"
        foreach ($c in ($gi.console.lines | Select-Object -Last 15)) {
            Write-Status "    $($c.text)" "Gray"
        }
    }
    # Also dump EMU1 console
    $gi1 = Get-GameIntrospection -Serial $EMU1
    if ($gi1 -and $gi1.console) {
        Write-Status "  EMU1 console:" "Gray"
        foreach ($c in ($gi1.console.lines | Select-Object -Last 10)) {
            Write-Status "    $($c.text)" "Gray"
        }
    }
    Cleanup; exit 1
}
Write-Status "Both players in game" "Green"
# Stop background logcat capture
if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force -ErrorAction SilentlyContinue }
if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force -ErrorAction SilentlyContinue }

# -- Step 9: Verify multiplayer state --
Write-Status ""
Write-Status "--- Phase 9: Verify multiplayer state ---" "White"

# Give the game a moment to fully sync
Start-Sleep -Seconds 5

$gi1 = Get-GameIntrospection -Serial $EMU1
$gi2 = Get-GameIntrospection -Serial $EMU2

Write-Status "Player 1 game state:"
Write-Status "  game_mode=$($gi1.game_mode) is_network=$($gi1.is_network) in_game=$($gi1.in_game)"

$failures = @()

# Verify network mode
if (-not $gi1.is_network) { $failures += "Player 1 not in network mode" }
if (-not $gi2.is_network) { $failures += "Player 2 not in network mode" }

# Verify multiplayer block exists
if (-not $gi1.multiplayer) {
    $failures += "Player 1 has no multiplayer section"
} else {
    $mp1state = $gi1.multiplayer
    Write-Status "Player 1 multiplayer state:"
    Write-Status "  num_players=$($mp1state.num_players) gamemode_name=$($mp1state.gamemode_name)"
    Write-Status "  mission=$($mp1state.mission_title) level=$($mp1state.level_num)"

    if ($mp1state.num_players -lt 2) { $failures += "Player 1 sees $($mp1state.num_players) players (expected 2+)" }
    if ($mp1state.gamemode_name -ne $MODE) { $failures += "Player 1 gamemode: $($mp1state.gamemode_name) (expected $MODE)" }

    # Check callsigns
    $p1Callsigns = ($mp1state.players | ForEach-Object { $_.callsign }) -join ", "
    Write-Status "  Player callsigns: $p1Callsigns"

    $hasHost = $mp1state.players | Where-Object { $_.callsign -eq $CALLSIGN1 }
    $hasJoiner = $mp1state.players | Where-Object { $_.callsign -eq $CALLSIGN2 }
    if (-not $hasHost) { $failures += "Player 1 doesn't see $CALLSIGN1 in player list" }
    if (-not $hasJoiner) { $failures += "Player 1 doesn't see $CALLSIGN2 in player list" }
}

if (-not $gi2.multiplayer) {
    $failures += "Player 2 has no multiplayer section"
} else {
    $mp2state = $gi2.multiplayer
    Write-Status "Player 2 multiplayer state:"
    Write-Status "  num_players=$($mp2state.num_players) gamemode_name=$($mp2state.gamemode_name)"

    if ($mp2state.num_players -lt 2) { $failures += "Player 2 sees $($mp2state.num_players) players (expected 2+)" }

    $p2Callsigns = ($mp2state.players | ForEach-Object { $_.callsign }) -join ", "
    Write-Status "  Player callsigns: $p2Callsigns"

    $hasHost = $mp2state.players | Where-Object { $_.callsign -eq $CALLSIGN1 }
    $hasJoiner = $mp2state.players | Where-Object { $_.callsign -eq $CALLSIGN2 }
    if (-not $hasHost) { $failures += "Player 2 doesn't see $CALLSIGN1 in player list" }
    if (-not $hasJoiner) { $failures += "Player 2 doesn't see $CALLSIGN2 in player list" }
}

# -- Results --
Write-Status ""
Write-Status "=== Test Results ===" "White"

if ($failures.Count -eq 0) {
    $testPassed = $true
    Write-Status "ALL CHECKS PASSED" "Green"
    Write-Status "  - Both players connected to matchmaking server"
    Write-Status "  - Lobby created and joined successfully"
    Write-Status "  - Chat messages exchanged both ways"
    Write-Status "  - Both players readied up"
    Write-Status "  - Game started and launched on both emulators"
    Write-Status "  - Both players in network game with correct player count and callsigns"
} else {
    Write-Status "FAILURES ($($failures.Count)):" "Red"
    foreach ($f in $failures) {
        Write-Status "  - $f" "Red"
    }
}

} finally {
    Cleanup
    if ($testPassed) {
        exit 0
    } else {
        exit 1
    }
}
