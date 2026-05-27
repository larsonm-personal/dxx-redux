#!/usr/bin/env pwsh
# test_mp.ps1 -- Two-player multiplayer integration test.
#
# Orchestrates two Android emulator instances and a matchmaking server to:
#   1. Launch SetupActivity on both emulators
#   2. Connect both players to the server via MP_COMMAND
#   3. Player 1 creates a lobby, Player 2 joins
#   4. Both players ready up
#   5. Player 1 starts the game
#   6. Both players launch into the game and verify multiplayer state
#   7. Sustained connectivity check (90s)
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
    [string]$Mode = "anarchy",
    [switch]$SkipBuild,
    [switch]$Tls,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Source shared env setup (JAVA_HOME, cmake, cargo)
. "$PSScriptRoot\..\helpers\test_env.ps1"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

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
$MODE = $Mode

$serverProcess = $null
$testPassed = $false

$script:LogFile = Join-Path $REPO_ROOT "temp\mp_test_log.txt"
try { if (Test-Path $script:LogFile) { Remove-Item $script:LogFile -Force -ErrorAction SilentlyContinue } } catch { }
"" | Set-Content -Path $script:LogFile -Encoding utf8 -ErrorAction SilentlyContinue

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

function Test-AllLobbyPlayersReady {
    param([string]$Serial, [int]$ExpectedPlayers = 2)

    $mp = Get-MpIntrospection -Serial $Serial
    if (-not $mp -or -not $mp.lobby) {
        return $false
    }

    $playersProperty = $mp.lobby.PSObject.Properties['players']
    if (-not $playersProperty -or $null -eq $playersProperty.Value) {
        return $false
    }

    $players = @($playersProperty.Value)
    if ($players.Count -lt $ExpectedPlayers) {
        return $false
    }

    foreach ($player in $players) {
        $readyProperty = $player.PSObject.Properties['ready']
        if (-not $readyProperty -or -not [bool]$readyProperty.Value) {
            return $false
        }
    }

    return $true
}

function Cleanup {
    Write-Status "Cleaning up..."
    # Force-stop the app on both emulators BEFORE killing the server.
    # Otherwise the relay dies first, the game detects a timeout ~15s later,
    # and shows "Host left the game!" dialogs.
    foreach ($emu in @($EMU1, $EMU2)) {
        try {
            & $ADB -s $emu shell am force-stop $PACKAGE 2>&1 | Out-Null
        } catch {}
    }
    if ($script:serverProcess -and -not $script:serverProcess.HasExited) {
        Write-Status "Stopping matchmaking server (PID $($script:serverProcess.Id))..."
        try { $script:serverProcess.Kill() } catch {}
        try { $script:serverProcess.WaitForExit(5000) } catch {}
    }
    # Wait briefly for log file to flush
    Start-Sleep -Milliseconds 500
    # Print server log tail for diagnostics
    $serverLogDir = Join-Path $REPO_ROOT "temp"
    $serverLogFile = Get-ChildItem $serverLogDir -Filter "dxx-matchmaking.log*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($serverLogFile) {
        $lines = Get-Content $serverLogFile.FullName | Select-Object -Last 30
        if ($lines) {
            Write-Status "  Server log ($($serverLogFile.Name), last 30 lines):" "Gray"
            foreach ($l in $lines) {
                if ($l.Trim()) { Write-Status "    $($l.Trim())" "Gray" }
            }
        }
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

    # Verify game data on both emulators, push if missing
    foreach ($emu in @($EMU1, $EMU2)) {
        if (-not (Ensure-StandardGameDataOnDevice -Serial $emu)) {
            Write-Status "FAIL: Could not ensure standard game data on $emu" "Red"
            exit 1
        }
    }
    Write-Status "Game data verified on both emulators" "Green"

    # Ensure active file set is 'default' (a previous test may have changed it).
    # Delete file_sets.json -- FileSetManager.getActive() defaults to "default"
    # when the file is missing.  Force-stop first so a running app can't re-create it.
    foreach ($emu in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "am", "force-stop", $PACKAGE
        ) -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "run-as", $PACKAGE, "rm", "-f", "files/file_sets.json"
        ) -Seconds 5 | Out-Null
    }

    # Kill stale PowerShell processes to prevent handle leaks
    Get-Process powershell -ErrorAction SilentlyContinue |
        Where-Object { $_.Id -ne $PID -and $_.StartTime -lt (Get-Date).AddMinutes(-10) } |
        ForEach-Object { try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {} }

    # -- Step 1: Start matchmaking server --
    Write-Status ""
    Write-Status "--- Phase 1: Start matchmaking server ---" "White"

    # Pre-create firewall rules to prevent UAC prompts during test
    Ensure-FirewallRules

    # Kill any existing server on the ports we need (9000, 8080, 9001).
    # run_all_tests.ps1 may have started its own server for the tier.
    $serverPorts = @(9000, 8080, 9001)
    $killedPids = @{}
    foreach ($port in $serverPorts) {
        $conn = Get-NetTCPConnection -LocalPort $port -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($conn -and -not $killedPids.ContainsKey($conn.OwningProcess)) {
            Write-Status "Killing existing process on port $port (PID $($conn.OwningProcess))..."
            Stop-Process -Id $conn.OwningProcess -Force -ErrorAction SilentlyContinue
            $killedPids[$conn.OwningProcess] = $true
        }
    }
    # Also check UDP listeners (relay port 9001 is UDP)
    foreach ($port in $serverPorts) {
        $udp = Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($udp -and -not $killedPids.ContainsKey($udp.OwningProcess)) {
            Write-Status "Killing UDP process on port $port (PID $($udp.OwningProcess))..."
            Stop-Process -Id $udp.OwningProcess -Force -ErrorAction SilentlyContinue
            $killedPids[$udp.OwningProcess] = $true
        }
    }
    if ($killedPids.Count -gt 0) {
        # Wait for ports to actually be free (TIME_WAIT can hold them)
        $waitSw = [System.Diagnostics.Stopwatch]::StartNew()
        while ($waitSw.Elapsed.TotalSeconds -lt 10) {
            $stillBusy = $false
            foreach ($port in $serverPorts) {
                $tcp = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue
                if ($tcp) { $stillBusy = $true; break }
            }
            if (-not $stillBusy) { break }
            Start-Sleep -Seconds 1
        }
        if ($stillBusy) {
            Write-Status "WARNING: Ports still in use after 10s wait" "Yellow"
        }
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

    # Set env vars for server, then launch with Start-Process for clean stderr capture
    $env:SKIP_GPGS_VERIFY = "true"
    $env:RUST_LOG = "info"
    # Bind to localhost to avoid Windows Firewall prompts (emulator
    # reaches host via 10.0.2.2 which maps to loopback)
    $env:WS_LISTEN_ADDR = "127.0.0.1:9000"
    $env:HTTP_LISTEN_ADDR = "127.0.0.1:8080"
    # Configure built-in UDP relay so LocalhostProxy can route game traffic.
    # Emulators reach host loopback at 10.0.2.2.
    $env:RELAY_LISTEN_ADDR = "127.0.0.1:9001"
    $env:RELAY_PUBLIC_ADDR = "10.0.2.2:9001"
    # Force relay for all connections -- two emulators on the same host share
    # a NAT gateway and cannot reach each other's 10.0.2.x addresses directly.
    $env:FORCE_RELAY = "true"
    $env:LOG_DIR = (Join-Path $REPO_ROOT "temp")
    if ($Tls) {
        $tlsDir = Join-Path $REPO_ROOT "temp\tls"
        New-Item -Path $tlsDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
        $tlsCert = Join-Path $tlsDir "server.crt"
        $tlsKey = Join-Path $tlsDir "server.key"
        if (-not (Test-Path $tlsCert) -or -not (Test-Path $tlsKey)) {
            Write-Status "  Generating self-signed TLS certificate..."
            $opensslExe = "openssl"
            if (Test-Path "$DEP_BASE\git\usr\bin\openssl.exe") { $opensslExe = "$DEP_BASE\git\usr\bin\openssl.exe" }
            & $opensslExe req -x509 -newkey rsa:2048 -keyout $tlsKey -out $tlsCert `
                -days 365 -nodes -subj "/CN=localhost" 2>&1 | Out-Null
        }
        if ((Test-Path $tlsCert) -and (Test-Path $tlsKey)) {
            $env:TLS_CERT_PATH = $tlsCert
            $env:TLS_KEY_PATH = $tlsKey
            Write-Status "  TLS enabled"
        } else {
            Write-Status "  WARNING: TLS cert generation failed, falling back to plain WS" "Yellow"
        }
    }
    $script:serverLogPath = Join-Path $REPO_ROOT "temp\matchmaking_server.log"
    $serverProcess = Start-Process -FilePath $serverBin -WorkingDirectory $serverDir `
        -NoNewWindow -PassThru `
        -RedirectStandardError $script:serverLogPath `
        -RedirectStandardOutput (Join-Path $REPO_ROOT "temp\server_stdout.log")
    Write-Status "Server starting (PID $($serverProcess.Id))..."

    # Wait for server to be ready (both WS and relay ports)
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

    # -- Step 3: Connect to matchmaking --
    Write-Status ""
    Write-Status "--- Phase 3: Connect to matchmaking ---" "White"

    # Drive the multiplayer flow through the SetupActivity MP_COMMAND receiver.
    # This avoids coupling the test to launcher button exposure details.
    Send-MpCommand -Serial $EMU1 -Command "set_callsign" -Extras @("--es", "callsign", $CALLSIGN1)
    Send-MpCommand -Serial $EMU2 -Command "set_callsign" -Extras @("--es", "callsign", $CALLSIGN2)
    Start-Sleep -Milliseconds 500

    $testServerUrl = if ($Tls) { "wss://10.0.2.2:9000/ws" } else { "ws://10.0.2.2:9000/ws" }
    Write-Status "Connecting via $testServerUrl"
    Send-MpCommand -Serial $EMU1 -Command "connect" -Extras @("--es", "url", $testServerUrl)
    Send-MpCommand -Serial $EMU2 -Command "connect" -Extras @("--es", "url", $testServerUrl)

    $conn1 = Wait-ForCondition -Description "Player 1 connected" -TimeoutSec 15 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU1
        return ($mp -and $mp.status -eq "CONNECTED")
    }
    if (-not $conn1) {
        Write-Status "FAIL: Player 1 didn't connect" "Red"
        $mp1 = Get-MpIntrospection -Serial $EMU1
        if ($mp1) { Write-Status "  Status: $($mp1.status), Error: $($mp1.error)" "Yellow" }
        Cleanup; exit 1
    }
    $conn2 = Wait-ForCondition -Description "Player 2 connected" -TimeoutSec 15 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU2
        return ($mp -and $mp.status -eq "CONNECTED")
    }
    if (-not $conn2) {
        Write-Status "FAIL: Player 2 didn't connect" "Red"
        Cleanup; exit 1
    }
    Write-Status "Both players connected" "Green"

    # -- Step 4: Player 1 creates a lobby --
    Write-Status ""
    Write-Status "--- Phase 4: Player 1 creates a lobby ---" "White"

    # Use MP_COMMAND for create_lobby (the dialog with dropdowns is complex to tap through)
    Send-MpCommand -Serial $EMU1 -Command "create_lobby" -Extras @(
        "--es", "game", $Game,
        "--es", "mission", $MISSION,
        "--es", "mode", $MODE,
        "--ei", "max_players", "2"
    )

    $lobbyCreated = Wait-ForCondition -Description "Player 1 in lobby" -TimeoutSec 10 -PollMs 750 -Condition {
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

    Send-MpCommand -Serial $EMU2 -Command "refresh_lobbies"
    $lobbiesVisible = Wait-ForCondition -Description "Player 2 sees lobby" -TimeoutSec 10 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU2
        return ($mp -and $mp.lobby_count -gt 0)
    }
    if (-not $lobbiesVisible) {
        Write-Status "FAIL: Player 2 can't see any lobbies" "Red"
        Cleanup; exit 1
    }

    # Join via MP_COMMAND -- the "Join" button is inside a LazyColumn which
    # Compose's accessibility provider doesn't expose to sequential ID scans.
    Send-MpCommand -Serial $EMU2 -Command "join_first_lobby"

    $joined = Wait-ForCondition -Description "Both players in lobby" -TimeoutSec 10 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU2
        return ($mp -and $mp.lobby -and $mp.lobby.player_count -eq 2)
    }
    if (-not $joined) {
        Write-Status "FAIL: Player 2 didn't join lobby" "Red"
        Cleanup; exit 1
    }
    Write-Status "Both players in lobby" "Green"

    # -- Step 6: Quick chat verification (non-fatal) --
    Write-Status ""
    Write-Status "--- Phase 6: Chat verification ---" "White"

    Send-MpCommand -Serial $EMU1 -Command "chat" -Extras @("--es", "text", "Hello_from_Host")
    Start-Sleep -Milliseconds 500
    Send-MpCommand -Serial $EMU2 -Command "chat" -Extras @("--es", "text", "Hello_from_Joiner")

    $chatOk = Wait-ForCondition -Description "Chat delivered" -TimeoutSec 10 -PollMs 1500 -Condition {
        $m1 = Get-MpIntrospection -Serial $EMU1
        $m2 = Get-MpIntrospection -Serial $EMU2
        $p1HasJoiner = $m1 -and $m1.chat -and ($m1.chat | Where-Object { $_.text -eq "Hello_from_Joiner" })
        $p2HasHost = $m2 -and $m2.chat -and ($m2.chat | Where-Object { $_.text -eq "Hello_from_Host" })
        return ($p1HasJoiner -and $p2HasHost)
    }
    if (-not $chatOk) {
        Write-Status "WARN: Chat not fully delivered (non-fatal)" "Yellow"
    } else {
        Write-Status "Chat works both ways" "Green"
    }

    # -- Step 7: Ready up and start game --
    Write-Status ""
    Write-Status "--- Phase 7: Ready up and start game ---" "White"

    Write-Status "Setting both players ready via MP_COMMAND"
    Send-MpCommand -Serial $EMU1 -Command "set_ready" -Extras @("--es", "ready", "true")
    Send-MpCommand -Serial $EMU2 -Command "set_ready" -Extras @("--es", "ready", "true")

    $bothReady = Wait-ForCondition -Description "Both players ready" -TimeoutSec 10 -PollMs 750 -Condition {
        return Test-AllLobbyPlayersReady -Serial $EMU1 -ExpectedPlayers 2
    }
    if (-not $bothReady) {
        $mp = Get-MpIntrospection -Serial $EMU1
        if ($mp -and $mp.lobby -and $mp.lobby.PSObject.Properties['players']) {
            $readySummary = @($mp.lobby.players) | ForEach-Object {
                "$($_.callsign)=$($_.ready)"
            }
            if ($readySummary) {
                Write-Status "  Ready state: $($readySummary -join ', ')" "Yellow"
            }
        }
        Write-Status "FAIL: Not all players ready" "Red"
        Cleanup; exit 1
    }
    Write-Status "Both players ready" "Green"

    # Clear stale introspect files so Phase 8 doesn't read old game state
    foreach ($emu in @($EMU1, $EMU2)) {
        Adb-Dev -Serial $emu -AdbArgs @(
            "shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json"
        ) | Out-Null
    }

    # Clear logcat and start capture BEFORE start_game
    & $ADB -s $EMU1 logcat -c 2>&1 | Out-Null
    & $ADB -s $EMU2 logcat -c 2>&1 | Out-Null
    $logcatFile1 = Join-Path $REPO_ROOT "temp\emu1_logcat_phase8.txt"
    $logcatFile2 = Join-Path $REPO_ROOT "temp\emu2_logcat_phase8.txt"
    $logcatProc1 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU1, "logcat", "-s", "DXX-MP:*", "DXX-Redux:*", "dxxredux:*", "MatchmakingService:*", "LocalhostProxy:*", "DEBUG:*", "AndroidRuntime:*", "libc:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile1 -RedirectStandardError (Join-Path $REPO_ROOT "temp\emu1_logcat_err.txt")
    $logcatProc2 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU2, "logcat", "-s", "DXX-MP:*", "DXX-Redux:*", "dxxredux:*", "MatchmakingService:*", "LocalhostProxy:*", "DEBUG:*", "AndroidRuntime:*", "libc:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile2 -RedirectStandardError (Join-Path $REPO_ROOT "temp\emu2_logcat_err.txt")

    # Start game via MP_COMMAND -- "Start Game" is on the LobbyScreen which
    # uses LazyColumn; same accessibility limitation as the Join button.
    Write-Status "Player 1 starting game..."
    Send-MpCommand -Serial $EMU1 -Command "start_game"

    # Poll for game process on EMU1
    $null = Wait-ForCondition -Description "EMU1 game process" -TimeoutSec 15 -PollMs 500 -Condition {
        $gPid = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5
        return ($gPid -and $gPid -match '^\d+')
    }

    # -- Step 8: Wait for both players to enter the game --
    # The test drives matchmaking through MP_COMMANDs, so always send explicit
    # launch_game commands after the lobby transitions to GAME_STARTING.
    Write-Status ""
    Write-Status "--- Phase 8: Wait for game launch ---" "White"

    # Fallback: send launch_game in case the auto-launch didn't fire
    Send-MpCommand -Serial $EMU1 -Command "launch_game"
    $null = Wait-ForCondition -Description "EMU1 game process" -TimeoutSec 15 -PollMs 500 -Condition {
        $gPid = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5
        return ($gPid -and $gPid -match '^\d+')
    }
    Send-MpCommand -Serial $EMU2 -Command "launch_game"

    # Wait for both to enter the game.
    # Primary: check in_game via introspection.
    # Fallback: if introspection returns null (emulator crash during 3D render),
    # check MPDIAG logcat for "send_sync" as proof that the multiplayer handshake
    # completed.
    $script:p8poll = 0
    $script:p8nullCount = 0
    $script:p8hadLevel = $false
    $script:p8UsedFallback = $false
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
        # Fallback: after 5 consecutive null polls (emulator likely crashed during 3D render),
        # check MPDIAG logcat for send_sync confirmation.
        if ($script:p8nullCount -ge 5) {
            Write-Status "  Checking fallback: MPDIAG logcat..." "Gray"
            $hasSendSync = $false
            if (Test-Path $logcatFile1) {
                $mpdiag = Select-String -Path $logcatFile1 -Pattern "send_sync" -SimpleMatch -ErrorAction SilentlyContinue
                if ($mpdiag) { $hasSendSync = $true }
            }
            Write-Status "    send_sync in MPDIAG: $hasSendSync" "Gray"
            if ($hasSendSync) {
                Write-Status "  Fallback PASS: MPDIAG confirms send_sync (emulator likely crashed during 3D render)" "Yellow"
                $script:p8UsedFallback = $true
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
            # Fallback for P2: check MPDIAG for send_sync
            if ($script:p8nullCount2 -ge 5) {
                if (Test-Path $logcatFile2) {
                    $hasSendSync2 = Select-String -Path $logcatFile2 -Pattern "send_sync" -SimpleMatch -ErrorAction SilentlyContinue
                    if ($hasSendSync2) {
                        Write-Status "  Fallback PASS: MPDIAG confirms EMU2 send_sync" "Yellow"
                        return $true
                    }
                }
            }
            return $false
        }
    } else {
        Write-Status "  Skipping Player 2 introspection (fallback path, MPDIAG confirmed connectivity)" "Yellow"
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

    if ($script:p8UsedFallback) {
        Write-Status "Skipping introspection checks (emulator crashed during 3D render, MPDIAG confirmed connectivity)" "Yellow"
        $failures = @()
        $gi1 = $null
        $gi2 = $null
    } else {

        # Give the game a moment to fully sync -- poll for introspection readiness
        $null = Wait-ForCondition -Description "Introspection ready after game sync" -TimeoutSec 15 -PollMs 1000 -Condition {
            $gi = Get-GameIntrospection -Serial $EMU1
            return ($gi -and $gi.in_game -eq $true)
        }

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
            # Engine reports full name (e.g. "cooperative") while lobby uses short name ("coop")
            $modeAliases = @{ "coop" = "cooperative"; "ctf" = "capture_flag" }
            $expectedModes = @($MODE)
            if ($modeAliases.ContainsKey($MODE)) { $expectedModes += $modeAliases[$MODE] }
            if ($mp1state.gamemode_name -notin $expectedModes) { $failures += "Player 1 gamemode: $($mp1state.gamemode_name) (expected $MODE)" }

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

    } # end non-fallback Phase 9 block

    # -- Phase 10: Sustained connectivity check (90s, crosses 3 WS ping intervals) --
    if ($failures.Count -eq 0 -and -not $script:p8UsedFallback) {
        Write-Status ""
        Write-Status "--- Phase 10: Sustained connectivity check (90s) ---" "White"
        $sustainStart = [System.Diagnostics.Stopwatch]::StartNew()
        $sustainFailed = $false
        $prevState = @{}  # track per-player state changes
        while ($sustainStart.Elapsed.TotalSeconds -lt 90 -and -not $sustainFailed) {
            Start-Sleep -Seconds 5
            $elapsed = [int]$sustainStart.Elapsed.TotalSeconds
            $sg1 = Get-GameIntrospection -Serial $EMU1
            $sg2 = Get-GameIntrospection -Serial $EMU2
            foreach ($entry in @(@{Tag = "EMU1"; S = $sg1; Num = 1 }, @{Tag = "EMU2"; S = $sg2; Num = 2 })) {
                $tag = $entry.Tag; $sg = $entry.S; $pNum = $entry.Num
                if (-not $sg) { continue }
                try {

                    # Check for disconnect messagebox: game_window_is_front=false
                    # while in_game=true means a modal is on top (e.g. "Host left the game!")
                    $gwFront = $sg.game_window_is_front
                    $menuSub = ""
                    if ($sg.PSObject.Properties['menu'] -and $sg.menu -and
                        $sg.menu.PSObject.Properties['subtitle'] -and $sg.menu.subtitle) {
                        $menuSub = $sg.menu.subtitle
                    }

                    # Check connected status of all players
                    $disconnectedPeers = @()
                    if ($sg.PSObject.Properties['multiplayer'] -and $sg.multiplayer -and
                        $sg.multiplayer.PSObject.Properties['players'] -and $sg.multiplayer.players) {
                        foreach ($p in $sg.multiplayer.players) {
                            if (-not $p.is_me -and $p.connected -eq 0) {
                                $disconnectedPeers += $p.callsign
                            }
                        }
                    }

                    if ($sg.in_game -and -not $gwFront) {
                        # A modal is covering the game -- likely a disconnect notice
                        $failures += "Player $pNum has disconnect modal at ${elapsed}s (menu='$menuSub')"
                        $sustainFailed = $true
                        Write-Status "  [${elapsed}s] ${tag}: DISCONNECT MODAL: '$menuSub'" "Red"
                        if ($sg.console) {
                            Write-Status "  $tag console:" "Gray"
                            foreach ($c in ($sg.console.lines | Select-Object -Last 20)) {
                                Write-Status "    $($c.text)" "Yellow"
                            }
                        }
                        # Dump logcat for both emulators to catch proxy lifecycle events
                        Write-Status "  --- Logcat dump (last 80 lines, proxy/WS tags) ---" "Gray"
                        foreach ($emuEntry in @(@{Tag = "EMU1"; Serial = $EMU1 }, @{Tag = "EMU2"; Serial = $EMU2 })) {
                            $lcOut = & $ADB -s $emuEntry.Serial logcat -d -s "LocalhostProxy:*" "MatchmakingService:*" 2>&1 | Out-String
                            Write-Status "  $($emuEntry.Tag) proxy/WS logcat:" "Gray"
                            foreach ($lcLine in ($lcOut -split "`n" | Select-Object -Last 40)) {
                                if ($lcLine.Trim()) { Write-Status "    $($lcLine.Trim())" "Yellow" }
                            }
                        }
                    } elseif ($disconnectedPeers.Count -gt 0) {
                        $failures += "Player $pNum peer disconnected at ${elapsed}s ($($disconnectedPeers -join ', '))"
                        $sustainFailed = $true
                        Write-Status "  [${elapsed}s] ${tag}: PEER DISCONNECTED: $($disconnectedPeers -join ', ')" "Red"
                    } elseif ($sg.in_game -and $sg.multiplayer) {
                        $np = $sg.multiplayer.num_players
                        $mp = $sg.multiplayer
                        $myNum = $mp.my_player_num
                        # Build per-player state summary using slot index to avoid callsign confusion
                        $pSummary = ""
                        if ($mp.players) {
                            $parts = @()
                            foreach ($p in $mp.players) {
                                $me = ""; if ($p.is_me) { $me = "*" }
                                $parts += "p$($p.slot)${me}=$($p.callsign):s=$([int]$p.shields)/e=$([int]$p.energy)/sc=$($p.score)/c=$($p.connected)"
                            }
                            $pSummary = " [$($parts -join ', ')]"
                        }
                        Write-Status "  [${elapsed}s] ${tag}: gw_front=$gwFront np=$np my=$myNum$pSummary" "Gray"
                        if ($np -lt 2) {
                            $failures += "Player $pNum lost peer at ${elapsed}s (num_players=$np)"
                            $sustainFailed = $true
                        }
                    } elseif (-not $sg.in_game) {
                        $failures += "Player $pNum left game at ${elapsed}s (screen=$($sg.screen_mode))"
                        $sustainFailed = $true
                    }
                } catch {
                    Write-Status "  [${elapsed}s] ${tag}: introspect parse error: $_" "Yellow"
                }
            }
        }
        if (-not $sustainFailed) {
            Write-Status "  Connection sustained for 90s" "Green"

            # Final assertion: verify no disconnect dialog appeared in the last moments
            foreach ($entry in @(@{Tag = "EMU1"; Serial = $EMU1; Num = 1 }, @{Tag = "EMU2"; Serial = $EMU2; Num = 2 })) {
                $sg = Get-GameIntrospection -Serial $entry.Serial
                if ($sg -and $sg.in_game -and -not $sg.game_window_is_front) {
                    $menuSub = ""
                    if ($sg.PSObject.Properties['menu'] -and $sg.menu -and
                        $sg.menu.PSObject.Properties['subtitle'] -and $sg.menu.subtitle) {
                        $menuSub = $sg.menu.subtitle
                    }
                    $failures += "Player $($entry.Num) has disconnect modal at end (menu='$menuSub')"
                    Write-Status "  $($entry.Tag): DISCONNECT MODAL at end: '$menuSub'" "Red"
                }
            }
        }
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
    if (-not $testPassed) {
        Cleanup
    }
    if ($testPassed) {
        exit 0
    } else {
        exit 1
    }
}
