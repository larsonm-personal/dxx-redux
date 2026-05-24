#!/usr/bin/env pwsh
# test_lan.ps1 -- Two-player LAN multiplayer integration test.
#
# Uses the lan_launch MP_COMMAND to bypass the launcher lobby UI entirely,
# launching the game engine directly in host/join mode on two emulators.
#
# Modes:
#   -DirectLan  (default) Uses emulator 36.5+ shared Wi-Fi. The joiner
#               connects to the host's wlan0 IP directly. No relay needed.
#   -UseRelay   Legacy mode: a UDP relay bridges two emulators' separate
#               SLIRP NATs (pre-36.5 emulators).
#
# Prerequisites:
#   - Two emulators running (emulator-5554 and emulator-5556)
#   - APK installed on both
#   - Game data present on both
#
# Usage:
#   .\test_lan.ps1
#   .\test_lan.ps1 -Game d1
#   .\test_lan.ps1 -UseRelay
#   .\test_lan.ps1 -SkipBuild

param(
    [string]$Game = "d2",
    [switch]$SkipBuild,
    [switch]$UseRelay,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

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

function Cleanup {
    Write-Status "Cleaning up..."
    $relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
    if ($UseRelay -and (Test-Path $relayLog)) {
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

function Get-IntroMultiplayer {
    param($Intro)

    if (-not $Intro) {
        return $null
    }

    $prop = $Intro.PSObject.Properties['multiplayer']
    if ($null -eq $prop) {
        return $null
    }

    return $prop.Value
}

function Get-IntroNumConnected {
    param($Intro)

    $mp = Get-IntroMultiplayer -Intro $Intro
    if (-not $mp) {
        return $null
    }

    $prop = $mp.PSObject.Properties['num_connected']
    if ($null -eq $prop) {
        return $null
    }

    return [int]$prop.Value
}

# ---- Main Test Flow ----

try {

    Write-Status "=== LAN Multiplayer Integration Test ===" "White"
    Write-Status "Game: $Game | Mission: $MISSION | Mode: $MODE"
    Write-Status ""

    # -- Step 0: Ensure both emulators are online (auto-start if needed) --
    Write-Status "Checking emulators..."
    Start-EmulatorIfNeeded -Serial $EMU1 -AvdMap $AVD_MAP
    Start-EmulatorIfNeeded -Serial $EMU2 -AvdMap $AVD_MAP
    Write-Status "Both emulators online" "Green"

    # Verify game data
    foreach ($emu in @($EMU1, $EMU2)) {
        $files = Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "run-as", $PACKAGE, "ls", "$($script:DEFAULT_SET_DIR)/"
        ) -Seconds 5
        $required = if ($Game -eq "d1") { "descent.hog" } else { "descent2.hog" }
        if (-not $files -or $files -notmatch "(?i)$required") {
            Write-Status "Game data missing on $emu, pushing..." "Yellow"
            Install-AppAndData -Serial $emu
            $files = Adb-Dev-Timeout -Serial $emu -AdbArgs @(
                "shell", "run-as", $PACKAGE, "ls", "$($script:DEFAULT_SET_DIR)/"
            ) -Seconds 5
            if (-not $files -or $files -notmatch "(?i)$required") {
                Write-Status "FAIL: Game data still missing on $emu after push (need $required)" "Red"; exit 1
            }
        }
    }
    Write-Status "Game data verified on both emulators" "Green"

    foreach ($emu in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "am", "force-stop", $PACKAGE
        ) -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            "shell", "run-as", $PACKAGE, "rm", "-f", "files/file_sets.json"
        ) -Seconds 5 | Out-Null
    }

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

    # -- Step 2: Set up networking (relay or direct LAN) --
    Write-Status ""
    if ($UseRelay) {
        Write-Status "--- Phase 2: Set up UDP relay (legacy mode) ---" "White"

        # Pre-create firewall rules to prevent UAC prompts during test
        Ensure-FirewallRules

        # EMU1 redir: host:42500 -> EMU1:42424 (relay -> host game inbound)
        $r1del = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "del udp:42500"
        Write-Status "  EMU1 redir cleanup: $r1del" "Gray"
        $r1 = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "add udp:42500:42424"
        Write-Status "  EMU1 redir add: $r1" "Gray"

        # EMU2: remove stale redir
        $r2del = Setup-EmulatorRedir -ConsolePort 5556 -RedirSpec "del udp:42501"
        Write-Status "  EMU2 redir cleanup: $r2del" "Gray"

        # Start UDP relay
        $relayScript = Join-Path $PSScriptRoot "..\udp_relay.ps1"
        if (-not (Test-Path $relayScript)) {
            Write-Status "FAIL: udp_relay.ps1 not found at $relayScript" "Red"; exit 1
        }
        $relayProc = Start-Process pwsh -ArgumentList "-File", $relayScript, "-Bind", "127.0.0.1" -PassThru -NoNewWindow `
            -RedirectStandardOutput (Join-Path $REPO_ROOT "temp\udp_relay.log") `
            -RedirectStandardError (Join-Path $REPO_ROOT "temp\udp_relay_err.log")
        $script:relayProc = $relayProc
        Write-Status "UDP relay started (PID $($relayProc.Id))" "Green"
        Start-Sleep -Seconds 1
    } else {
        Write-Status "--- Phase 2: Direct LAN (emulator 36.5+ shared Wi-Fi) ---" "White"
        # Get host emulator's wlan0 IP for direct connection
        $hostIpRaw = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @(
            "shell", "ip", "addr", "show", "wlan0"
        ) -Seconds 5
        $script:DirectHostIp = ($hostIpRaw | Select-String -Pattern 'inet (\d+\.\d+\.\d+\.\d+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
        if (-not $script:DirectHostIp) {
            Write-Status "FAIL: Could not get wlan0 IP from $EMU1 -- is emulator 36.5+?" "Red"
            exit 1
        }
        Write-Status "Host wlan0 IP: $($script:DirectHostIp)" "Green"
    }

    # -- Step 3: Launch game on both emulators via lan_launch --
    Write-Status ""
    Write-Status "--- Phase 3: Launch game via lan_launch ---" "White"

    # Start logcat capture BEFORE lan_launch so we catch all MPDIAG messages.
    $logcatFile1 = Join-Path $REPO_ROOT "temp\lan_emu1_logcat.txt"
    $logcatFile2 = Join-Path $REPO_ROOT "temp\lan_emu2_logcat.txt"
    & $ADB -s $EMU1 logcat -c 2>&1 | Out-Null
    & $ADB -s $EMU2 logcat -c 2>&1 | Out-Null
    $logcatProc1 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU1, "logcat", "-s", "DXX-MP:*", "DXX-Redux:*", "dxxredux:*", "AndroidRuntime:*", "LocalhostProxy:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile1 -RedirectStandardError (Join-Path $REPO_ROOT "temp\lan_emu1_logcat_err.txt")
    $logcatProc2 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU2, "logcat", "-s", "DXX-MP:*", "DXX-Redux:*", "dxxredux:*", "AndroidRuntime:*", "LocalhostProxy:*", "MatchmakingService:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile2 -RedirectStandardError (Join-Path $REPO_ROOT "temp\lan_emu2_logcat_err.txt")
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

    # Poll for host game process to appear (replaces fixed 5s sleep)
    $null = Wait-ForCondition -Description "Host game process" -TimeoutSec 30 -PollMs 500 -Condition {
        $gPid = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @("shell", "pidof", "${PACKAGE}:game") -Seconds 5
        return ($gPid -and $gPid -match '^\d+')
    }

    # Joiner (EMU2)
    Write-Status "Sending lan_launch join to $EMU2..."
    $joinExtras = @(
        "--es", "game", $Game,
        "--es", "mp_mode", "join",
        "--es", "mission", $MISSION,
        "--es", "mode", $MODE,
        "--ei", "max_players", "2",
        "--ei", "level_num", "1",
        "--ei", "difficulty", "1",
        "--es", "callsign", $CALLSIGN2
    )
    if ($UseRelay) {
        # Relay mode: 10.0.2.2 = host loopback from emulator, port 42600 = relay
        $joinExtras += @("--es", "host_addr", "10.0.2.2", "--ei", "host_port", "42600")
        Write-Status "  Joiner target: 10.0.2.2:42600 (via relay)"
    } else {
        # Direct LAN: use host's wlan0 IP on the engine port
        $joinExtras += @("--es", "host_addr", $script:DirectHostIp, "--ei", "host_port", "42424")
        Write-Status "  Joiner target: $($script:DirectHostIp):42424 (direct LAN)"
    }
    Send-MpCommand -Serial $EMU2 -Command "lan_launch" -Extras $joinExtras

    # -- Step 4: Verify multiplayer launch --
    #
    # Primary signal: both emulators reach in-game network state with two
    # connected players in game introspection.
    #
    # Fallback: if an emulator dies during 3D render, accept host MPDIAG
    # sync lines when they are available.
    #
    # In direct LAN mode, traffic goes:
    #   EMU2 -> LAN proxy -> host wlan0 IP -> host engine
    # In relay mode:
    #   EMU2 -> LAN proxy -> relay -> EMU1 redir -> host engine
    #
    # We do NOT require both emulators to survive the level load phase
    # (GPU rendering on swiftshader_indirect can crash the emulator).

    Write-Status ""
    Write-Status "--- Phase 4: Wait for multiplayer sync ---" "White"

    # Quick diagnostic: is relay still alive? (relay mode only)
    if ($UseRelay -and $script:relayProc -and $script:relayProc.HasExited) {
        Write-Status "WARN: UDP relay exited early (code $($script:relayProc.ExitCode))" "Yellow"
        $errLog = Join-Path $REPO_ROOT "temp\udp_relay_err.log"
        if (Test-Path $errLog) {
            Get-Content $errLog -ErrorAction SilentlyContinue | Select-Object -Last 5 | ForEach-Object { Write-Status "  relay err: $_" "Yellow" }
        }
    }

    # Dump early EMU2 logcat to see if game launched and proxy started
    Start-Sleep -Seconds 2
    if (Test-Path $logcatFile2) {
        $early = Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Select-Object -First 20
        foreach ($l in $early) { Write-Status "  [EMU2 early] $l" "Gray" }
    }

    $script:pollCount = 0
    $syncOk = Wait-ForCondition -Description "LAN game sync" -TimeoutSec $TimeoutSeconds -PollMs 3000 -Condition {
        $script:pollCount++

        $gi1 = Get-GameIntrospection -Serial $EMU1
        $gi2 = Get-GameIntrospection -Serial $EMU2
        $script:lastGi1 = $gi1
        $script:lastGi2 = $gi2

        $hostInGame = if ($gi1) { [bool]$gi1.in_game } else { $false }
        $hostNet = if ($gi1) { [bool]$gi1.is_network } else { $false }
        $hostPlayers = if ($gi1) { Get-IntroNumConnected -Intro $gi1 } else { $null }
        $joinInGame = if ($gi2) { [bool]$gi2.in_game } else { $false }
        $joinNet = if ($gi2) { [bool]$gi2.is_network } else { $false }
        $joinPlayers = if ($gi2) { Get-IntroNumConnected -Intro $gi2 } else { $null }

        if ($gi1 -or $gi2) {
            $hostPlayersLabel = if ($null -ne $hostPlayers) { $hostPlayers } else { "?" }
            $joinPlayersLabel = if ($null -ne $joinPlayers) { $joinPlayers } else { "?" }
            Write-Status "  [poll $($script:pollCount)] host in_game=$hostInGame net=$hostNet players=$hostPlayersLabel | join in_game=$joinInGame net=$joinNet players=$joinPlayersLabel" "Gray"
            if ($hostInGame -and $hostNet -and $hostPlayers -ge 2 -and $joinInGame -and $joinNet -and $joinPlayers -ge 2) {
                return $true
            }
        } elseif (Test-Path $logcatFile1) {
            $lines = Get-Content $logcatFile1 -ErrorAction SilentlyContinue
            $hasSync = $lines | Where-Object { $_ -match 'send_sync.*sending SYNC to all' }
            $hasTwoPlayers = $lines | Where-Object { $_ -match 'N_players now 2' }
            $lastMpdiag = ($lines | Where-Object { $_ -match 'MPDIAG' } | Select-Object -Last 1)
            if ($lastMpdiag) {
                Write-Status "  [poll $($script:pollCount)] fallback MPDIAG: $($lastMpdiag.Substring([Math]::Max(0, $lastMpdiag.Length - 80)))" "Gray"
            } else {
                Write-Status "  [poll $($script:pollCount)] introspection unavailable, no MPDIAG yet" "Gray"
            }
            if ($hasSync -and $hasTwoPlayers) {
                return $true
            }
        }

        # Every 3rd poll, show EMU2 proxy/mp state for diagnostics
        if ($script:pollCount % 3 -eq 1 -and (Test-Path $logcatFile2)) {
            $emu2lines = Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'LocalhostProxy|MPDIAG|lan_launch|DXX-MP' } | Select-Object -Last 3
            foreach ($l in $emu2lines) { Write-Status "  [EMU2] $l" "Gray" }
        }
        return $false
    }

    if (-not $syncOk) {
        Write-Status "FAIL: Multiplayer sync never completed" "Red"
        $gi1 = $script:lastGi1
        $gi2 = $script:lastGi2
        if ($gi1) {
            $hostPlayers = Get-IntroNumConnected -Intro $gi1
            if ($null -eq $hostPlayers) { $hostPlayers = "?" }
            Write-Status "  EMU1 state: screen=$($gi1.screen_mode) in_game=$($gi1.in_game) net=$($gi1.is_network) players=$hostPlayers" "Gray"
        }
        if ($gi2) {
            $joinPlayers = Get-IntroNumConnected -Intro $gi2
            if ($null -eq $joinPlayers) { $joinPlayers = "?" }
            Write-Status "  EMU2 state: screen=$($gi2.screen_mode) in_game=$($gi2.in_game) net=$($gi2.is_network) players=$joinPlayers" "Gray"
        }
        if (Test-Path $logcatFile1) {
            Write-Status "  EMU1 MPDIAG lines:" "Gray"
            Get-Content $logcatFile1 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG' } | ForEach-Object { Write-Status "    $_" "Gray" }
        }
        if (Test-Path $logcatFile2) {
            Write-Status "  EMU2 all captured lines:" "Gray"
            Get-Content $logcatFile2 -ErrorAction SilentlyContinue | Select-Object -Last 30 | ForEach-Object { Write-Status "    $_" "Gray" }
        }
        Cleanup; exit 1
    }

    Write-Status "Multiplayer sync completed" "Green"

    # -- Step 5: Verify results --
    Write-Status ""
    Write-Status "--- Phase 5: Verify networking ---" "White"

    # Check relay traffic (relay mode only)
    if ($UseRelay) {
        $relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
        $relayLines = @()
        if (Test-Path $relayLog) {
            $relayLines = Get-Content $relayLog -ErrorAction SilentlyContinue
        }
        $emu1ToEmu2 = @($relayLines | Where-Object { $_ -match 'EMU1->EMU2' }).Count
        $emu2ToEmu1 = @($relayLines | Where-Object { $_ -match 'EMU2->EMU1' }).Count
        Write-Status "Relay traffic: EMU1->EMU2: $emu1ToEmu2 packets, EMU2->EMU1: $emu2ToEmu1 packets"
    } else {
        Write-Status "Direct LAN mode -- no relay traffic to verify"
    }

    # Check captured host diagnostics from logcat
    $hostLines = Get-Content $logcatFile1 -ErrorAction SilentlyContinue | Where-Object { $_ -match 'MPDIAG|auto_net|lan_launch|LocalhostProxy' }
    Write-Status "Host LAN log ($($hostLines.Count) lines):" "Gray"
    foreach ($line in $hostLines) {
        Write-Status "  $line" "Gray"
    }

    $testPassed = $true

    if ($UseRelay -and ($emu1ToEmu2 -eq 0 -or $emu2ToEmu1 -eq 0)) {
        Write-Status "FAIL: Relay did not forward traffic in both directions" "Red"
        $testPassed = $false
    }

    # Optionally check in-game state if emulators are still alive
    $gi1 = if ($script:lastGi1) { $script:lastGi1 } else { Get-GameIntrospection -Serial $EMU1 }
    $gi2 = if ($script:lastGi2) { $script:lastGi2 } else { Get-GameIntrospection -Serial $EMU2 }
    if ($gi1) {
        $hostPlayers = Get-IntroNumConnected -Intro $gi1
        if ($null -eq $hostPlayers) { $hostPlayers = "?" }
        Write-Status "EMU1: screen=$($gi1.screen_mode) in_game=$($gi1.in_game) net=$($gi1.is_network) players=$hostPlayers game_mode=$($gi1.game_mode)"
    } else {
        Write-Status "EMU1: introspection unavailable (emulator may have crashed during level load)" "Yellow"
    }
    if ($gi2) {
        $joinPlayers = Get-IntroNumConnected -Intro $gi2
        if ($null -eq $joinPlayers) { $joinPlayers = "?" }
        Write-Status "EMU2: screen=$($gi2.screen_mode) in_game=$($gi2.in_game) net=$($gi2.is_network) players=$joinPlayers game_mode=$($gi2.game_mode)"
    } else {
        Write-Status "EMU2: introspection unavailable (emulator may have crashed during level load)" "Yellow"
    }
    $hostFinalPlayers = Get-IntroNumConnected -Intro $gi1
    $joinFinalPlayers = Get-IntroNumConnected -Intro $gi2
    if (-not ($gi1 -and $gi1.is_network -and $hostFinalPlayers -ge 2)) {
        Write-Status "FAIL: EMU1 did not report an active two-player network game" "Red"
        $testPassed = $false
    }
    if (-not ($gi2 -and $gi2.is_network -and $joinFinalPlayers -ge 2)) {
        Write-Status "FAIL: EMU2 did not report an active two-player network game" "Red"
        $testPassed = $false
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
    if (-not $testPassed) { Cleanup }
}

exit $(if ($testPassed) { 0 } else { 1 })
