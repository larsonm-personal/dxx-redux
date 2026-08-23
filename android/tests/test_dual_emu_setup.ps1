#!/usr/bin/env pwsh
# test_dual_emu_setup.ps1 -- Launch two Android emulators with full
# multiplayer infrastructure ready for manual testing.
#
# Sets up: APK install, game data push, matchmaking server, and app
# launch on both emulators. Uses emulator 36.5+ shared Wi-Fi for
# direct LAN connectivity (no relay or port redirections needed).
#
# Usage:
#   .\test_dual_emu_setup.ps1                          # full build + launch
#   .\test_dual_emu_setup.ps1 -NoBuild                 # skip gradle + cargo builds
#   .\test_dual_emu_setup.ps1 -NoData                  # skip game data push
#   .\test_dual_emu_setup.ps1 -NoServer                # skip matchmaking server
#   .\test_dual_emu_setup.ps1 -KillOnExit              # stop emulators on exit

param(
    [switch]$NoBuild,
    [switch]$NoData,
    [switch]$NoServer,
    [switch]$KillOnExit
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Source shared env setup (JAVA_HOME, cmake, cargo) and test helpers
. "$PSScriptRoot\..\helpers\test_env.ps1"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$EMULATOR = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "emulator" -ToolName "emulator"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$EMU1_SERIAL = "emulator-5554"
$EMU2_SERIAL = "emulator-5556"
$AVD1 = "Nexus5X_Light_1"
$AVD2 = "Nexus5X_Light_2"

$script:serverProcess = $null
$script:emu1Proc = $null
$script:emu2Proc = $null

$script:LogFile = Join-Path $REPO_ROOT "temp\dual_emu_log.txt"
New-Item -Path (Join-Path $REPO_ROOT "temp") -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
try { [IO.File]::WriteAllText($script:LogFile, [Environment]::NewLine, [Text.UTF8Encoding]::new($false)) } catch {}

function Wait-ForBoot {
    param([string]$Serial, [int]$TimeoutSec = 240)
    Write-Status "Waiting for $Serial to boot (timeout ${TimeoutSec}s)..."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $result = Adb-Dev-Timeout -Serial $Serial -AdbArgs @("shell", "getprop", "sys.boot_completed") -Seconds 5
        if ($result -and $result.Trim() -eq "1") {
            Write-Status "  $Serial booted" "Green"
            return $true
        }
        Start-Sleep -Seconds 2
    }
    Write-Status "  TIMEOUT: $Serial did not boot" "Red"
    return $false
}

function Cleanup {
    Write-Status ""
    Write-Status "Cleaning up..." "White"
    if ($script:serverProcess -and -not $script:serverProcess.HasExited) {
        Write-Status "  Stopping matchmaking server (PID $($script:serverProcess.Id))"
        try { $script:serverProcess.Kill() } catch {}
        try { $script:serverProcess.WaitForExit(5000) } catch {}
    }
    if ($KillOnExit) {
        Write-Status "  Stopping emulators..."
        Adb-Dev-Timeout -Serial $EMU1_SERIAL -AdbArgs @("emu", "kill") -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $EMU2_SERIAL -AdbArgs @("emu", "kill") -Seconds 5 | Out-Null
        Start-Sleep -Seconds 2
        Get-Process -Name "qemu-system*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}

# ── Phase A: Prerequisites and cleanup ──────────────────────────────────

Write-Status "=== Dual Emulator Launch ===" "White"
Write-Status ""

# Kill zombie processes
Write-Status "Cleaning stale processes..."
Get-Process powershell -ErrorAction SilentlyContinue |
    Where-Object { $_.Id -ne $PID -and $_.StartTime -lt (Get-Date).AddMinutes(-30) } |
    ForEach-Object { try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {} }

# Kill stale server on port 9000
$existingServer = Get-NetTCPConnection -LocalPort 9000 -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existingServer) {
    Write-Status "  Killing stale server on port 9000 (PID $($existingServer.OwningProcess))"
    Stop-Process -Id $existingServer.OwningProcess -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

# Kill stale cl.exe zombies (common issue on Windows)
Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# ── Phase A2: Build APK ─────────────────────────────────────────────────

$ANDROID_DIR = Join-Path $REPO_ROOT "android"
$APK = Join-Path $ANDROID_DIR "app\build\outputs\apk\debug\app-debug.apk"

if (-not $NoBuild) {
    Write-Status ""
    Write-Status "--- Building APK ---" "White"
    $gradleOut = & (Join-Path $ANDROID_DIR "gradlew.bat") -p $ANDROID_DIR assembleDebug 2>&1 | Out-String
    $gradleExit = $LASTEXITCODE
    if ($gradleExit -ne 0) {
        Write-Status "FAIL: Gradle build failed" "Red"
        Write-Status ($gradleOut | Select-Object -Last 20) "Yellow"
        exit 1
    }
    Write-Status "APK built" "Green"
}

if (-not (Test-Path $APK)) {
    Write-Status "FAIL: APK not found at $APK" "Red"
    Write-Status "Run without -NoBuild, or build first" "Yellow"
    exit 1
}

# ── Phase B: Launch emulators ───────────────────────────────────────────

Write-Status ""
Write-Status "--- Launching emulators ---" "White"

$emu1Running = Test-DeviceOnline -Serial $EMU1_SERIAL
$emu2Running = Test-DeviceOnline -Serial $EMU2_SERIAL

if ($emu1Running) {
    Write-Status "  $AVD1 ($EMU1_SERIAL) already running"
} else {
    Write-Status "  Starting $AVD1..."
    $script:emu1Proc = Start-Process $EMULATOR -ArgumentList "-avd", $AVD1, "-no-snapshot-save", "-gpu", "host" -PassThru
    Write-Status "  $AVD1 started (PID $($script:emu1Proc.Id))"
}

if ($emu2Running) {
    Write-Status "  $AVD2 ($EMU2_SERIAL) already running"
} else {
    Write-Status "  Starting $AVD2..."
    $script:emu2Proc = Start-Process $EMULATOR -ArgumentList "-avd", $AVD2, "-no-snapshot-save", "-gpu", "host" -PassThru
    Write-Status "  $AVD2 started (PID $($script:emu2Proc.Id))"
}

# Wait for both to boot
if (-not $emu1Running) {
    # Wait for adb to see the device first
    Write-Status "  Waiting for $EMU1_SERIAL to appear in adb..."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 60 -and -not (Test-DeviceOnline -Serial $EMU1_SERIAL)) {
        Start-Sleep -Seconds 2
    }
}
if (-not $emu2Running) {
    Write-Status "  Waiting for $EMU2_SERIAL to appear in adb..."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 60 -and -not (Test-DeviceOnline -Serial $EMU2_SERIAL)) {
        Start-Sleep -Seconds 2
    }
}

$boot1 = Wait-ForBoot -Serial $EMU1_SERIAL -TimeoutSec 240
$boot2 = Wait-ForBoot -Serial $EMU2_SERIAL -TimeoutSec 240
if (-not $boot1 -or -not $boot2) {
    Write-Status "FAIL: Not all emulators booted successfully" "Red"
    exit 1
}
Write-Status "Both emulators booted" "Green"

# ── Phase C: Install APK + push game data ───────────────────────────────

Write-Status ""
Write-Status "--- Installing APK on both emulators ---" "White"

foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
    Write-Status "  Installing on $serial..."
    $installOut = Adb-Dev-Timeout -Serial $serial -AdbArgs @("install", "-r", $APK) -Seconds 60
    if ($installOut -and $installOut -match "Success") {
        Write-Status "  ${serial}: installed" "Green"
    } else {
        Write-Status "  ${serial}: install output: $installOut" "Yellow"
    }
}

if (-not $NoData) {
    Write-Status ""
    Write-Status "--- Pushing game data to both emulators ---" "White"
    foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
        Install-AppAndData -Serial $serial
    }
}

# ── Phase D: Server + networking ────────────────────────────────────────

if (-not $NoServer) {
    Write-Status ""
    Write-Status "--- Starting matchmaking server ---" "White"

    $serverDir = Join-Path $REPO_ROOT "server"
    $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "release") -BaseName "dxx-matchmaking"

    if (-not $NoBuild -or -not $serverBin) {
        Write-Status "  Building matchmaking server..."
        $serverManifest = Join-Path $serverDir "Cargo.toml"
        $buildOut = & cargo build --release --manifest-path $serverManifest 2>&1 | Out-String
        $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "release") -BaseName "dxx-matchmaking"
        if (-not $serverBin) {
            Write-Status "FAIL: Server build failed" "Red"
            Write-Status ($buildOut | Select-Object -Last 10) "Yellow"
            exit 1
        }
        Write-Status "  Server built" "Green"
    }

    # Generate a self-signed TLS cert so the server can serve wss://
    # (the Android client always connects via wss:// to private IPs)
    $tlsDir = Join-Path $REPO_ROOT "temp\tls"
    New-Item -Path $tlsDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
    $tlsCert = Join-Path $tlsDir "server.crt"
    $tlsKey = Join-Path $tlsDir "server.key"
    if (-not (Test-Path $tlsCert) -or -not (Test-Path $tlsKey)) {
        Write-Status "  Generating self-signed TLS certificate..."
        $opensslExe = Resolve-RegressionOpenSslTool -DepBase $DEP_BASE
        & $opensslExe req -x509 -newkey rsa:2048 -keyout $tlsKey -out $tlsCert `
            -days 365 -nodes -subj "/CN=localhost" 2>&1 | Out-Null
        if (-not (Test-Path $tlsCert) -or -not (Test-Path $tlsKey)) {
            Write-Status "  WARNING: Failed to generate TLS cert, server will run without TLS" "Yellow"
        } else {
            Write-Status "  TLS cert generated" "Green"
        }
    }

    $serverLog = Join-Path $REPO_ROOT "temp\server.log"
    $serverErr = Join-Path $REPO_ROOT "temp\server_err.log"
    $serverEnv = @{
        SKIP_GPGS_VERIFY  = "true"
        RUST_LOG          = "info"
        # Emulators access host at 10.0.2.2; relay listens on 0.0.0.0:9001
        RELAY_PUBLIC_ADDR = "10.0.2.2:9001"
        LOG_DIR           = (Join-Path $REPO_ROOT "temp")
    }
    if ((Test-Path $tlsCert) -and (Test-Path $tlsKey)) {
        $serverEnv["TLS_CERT_PATH"] = $tlsCert
        $serverEnv["TLS_KEY_PATH"] = $tlsKey
    }
    # Launch server binary directly -- do NOT wrap in pwsh.  PowerShell's
    # internal pipe handling deadlocks when the native process's stderr
    # output fills the internal buffer (same class of bug as the
    # ProcessStartInfo pipe deadlock, just one level deeper).
    foreach ($kv in $serverEnv.GetEnumerator()) {
        [System.Environment]::SetEnvironmentVariable($kv.Key, $kv.Value, "Process")
    }
    $script:serverProcess = Start-Process -FilePath $serverBin `
        -WorkingDirectory $serverDir `
        -PassThru -NoNewWindow `
        -RedirectStandardOutput $serverLog `
        -RedirectStandardError $serverErr
    Write-Status "  Server PID: $($script:serverProcess.Id) (log: temp\server.log)"

    # Wait for server to be ready
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $serverListening = $false
    while ($sw.Elapsed.TotalSeconds -lt 15) {
        $conn = Get-NetTCPConnection -LocalPort 9000 -State Listen -ErrorAction SilentlyContinue
        if ($conn) { $serverListening = $true; break }
        Start-Sleep -Seconds 1
    }
    if (-not $serverListening) {
        Write-Status "FAIL: Server didn't start on port 9000" "Red"
        Cleanup; exit 1
    }
    Write-Status "  Server listening on port 9000" "Green"
}

# ── Phase E: Launch apps ────────────────────────────────────────────────

Write-Status ""
Write-Status "--- Launching app on both emulators ---" "White"

foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
    if (-not (Start-SetupActivity -Serial $serial)) {
        Write-Status "  ${serial}: setup screen not confirmed (app may still be loading)" "Yellow"
    }
}

# ── Phase F: Print summary and wait ─────────────────────────────────────

Write-Status ""
Write-Status "========================================" "White"
Write-Status "  Dual emulator setup complete" "Green"
Write-Status "========================================" "White"
Write-Status ""
Write-Status "Emulators:" "White"
Write-Status "  EMU1: $EMU1_SERIAL ($AVD1) -- Player 1 / Host"
Write-Status "  EMU2: $EMU2_SERIAL ($AVD2) -- Player 2 / Joiner"
Write-Status ""
Write-Status "Network (emulator 36.5+ shared Wi-Fi, no relay needed):" "White"
if (-not $NoServer) {
    Write-Status "  Matchmaking server: ws://10.0.2.2:9000/ws (PID $($script:serverProcess.Id))"
    Write-Status "  Server relay: 10.0.2.2:9001 (UDP)"
    Write-Status "  Server log: temp\server.log / temp\server_err.log"
}
Write-Status ""
if ($NoServer) {
    Write-Status "LAN manual connection (shared Wi-Fi):" "White"
    Write-Status "  EMU1: Start New Multiplayer Game (host)" "Gray"
    Write-Status "  EMU2: Join Game > scan LAN or enter host's wlan0 IP" "Gray"
} else {
    Write-Status "Quick test commands:" "White"
    Write-Status '  # Connect both to server:' "Gray"
    Write-Status "  adb -s $EMU1_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command set_callsign --es callsign Host" "Gray"
    Write-Status "  adb -s $EMU1_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command connect" "Gray"
    Write-Status "  adb -s $EMU2_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command set_callsign --es callsign Join" "Gray"
    Write-Status "  adb -s $EMU2_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command connect" "Gray"
    Write-Status '  # Create lobby + join:' "Gray"
    Write-Status "  adb -s $EMU1_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command create_lobby --es game d2 --es mission d2 --es mode anarchy" "Gray"
    Write-Status "  adb -s $EMU2_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command refresh_lobbies" "Gray"
    Write-Status "  adb -s $EMU2_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command join_first_lobby" "Gray"
    Write-Status '  # Start game (server relay on port 9001 handles routing):' "Gray"
    Write-Status "  adb -s $EMU1_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command start_game" "Gray"
    Write-Status ""
    Write-Status '  # Run existing multiplayer test:' "Gray"
    Write-Status "  .\test_mp.ps1 -SkipBuild" "Gray"
}
Write-Status ""
Write-Status "Press Ctrl+C or Enter to shut down$(if (-not $NoServer) {' server and'}) exit" "Yellow"

try {
    Read-Host "Press Enter to exit"
} catch {
    # Ctrl+C
}

Cleanup
Write-Status "Done" "Green"
