#!/usr/bin/env pwsh
# test_dual_emu.ps1 -- Interactive dual-emulator multiplayer test setup.
#
# Launches two emulators, installs APK + game data, starts the matchmaking
# server, and then presents a menu to optionally configure Docker NAT
# simulation. Uses emulator 36.5+ shared Wi-Fi (no relay needed).
# Waits for manual testing when ready.
#
# Usage:
#   .\test_dual_emu.ps1                   # full build + launch
#   .\test_dual_emu.ps1 -NoBuild          # skip gradle + cargo builds
#   .\test_dual_emu.ps1 -NoData           # skip game data push
#   .\test_dual_emu.ps1 -KillOnExit       # stop emulators on exit

param(
    [switch]$NoBuild,
    [switch]$NoData,
    [switch]$KillOnExit
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Source shared env setup and helpers
. "$PSScriptRoot\..\helpers\test_env.ps1"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$ANDROID_DIR = Join-Path $REPO_ROOT "android"
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
$script:dockerNatActive = $false

$script:LogFile = Join-Path $REPO_ROOT "temp\dual_emu_log.txt"
New-Item -Path (Join-Path $REPO_ROOT "temp") -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
"" | Set-Content -Path $script:LogFile -Encoding utf8 -ErrorAction SilentlyContinue

# ── Helpers ──────────────────────────────────────────────────────────────

function Read-NumberedChoice {
    param(
        [string]$Prompt,
        [int]$OptionCount,
        [int]$DefaultChoice = 1
    )

    while ($true) {
        $choice = Read-Host $Prompt
        if ([string]::IsNullOrWhiteSpace($choice)) {
            return $DefaultChoice
        }

        $selected = 0
        if ([int]::TryParse($choice, [ref]$selected) -and $selected -ge 1 -and $selected -le $OptionCount) {
            return $selected
        }

        Write-Status "Enter a number between 1 and $OptionCount" "Yellow"
    }
}

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
    if ($script:dockerNatActive) {
        Write-Status "  Tearing down Docker NAT..."
        foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
            Send-MpCommand -Serial $serial -Command "stun_override_clear"
        }
        Stop-DockerNat
    }
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

# ── NAT menu helper ─────────────────────────────────────────────────────

$NAT_PRESETS = @(
    @{ Label = "No NAT simulation (direct STUN)"; NatA = $null; NatB = $null }
    @{ Label = "Full-cone <-> Full-cone"; NatA = "full-cone"; NatB = "full-cone" }
    @{ Label = "Full-cone <-> Symmetric"; NatA = "full-cone"; NatB = "symmetric" }
    @{ Label = "Port-restricted <-> Symmetric"; NatA = "port-restricted"; NatB = "symmetric" }
    @{ Label = "Symmetric <-> Symmetric (relay only)"; NatA = "symmetric"; NatB = "symmetric" }
    @{ Label = "Full-cone <-> Port-restricted"; NatA = "full-cone"; NatB = "port-restricted" }
    @{ Label = "Port-restricted <-> Port-restricted"; NatA = "port-restricted"; NatB = "port-restricted" }
    @{ Label = "Symmetric-seq <-> Symmetric-seq"; NatA = "symmetric-seq"; NatB = "symmetric-seq" }
)

function Show-NatMenu {
    Write-Host ""
    Write-Host "=== NAT Simulation Mode ===" -ForegroundColor Cyan
    Write-Host ""
    for ($i = 0; $i -lt $NAT_PRESETS.Count; $i++) {
        $p = $NAT_PRESETS[$i]
        $tag = if ($p.NatA) { "[$($p.NatA) / $($p.NatB)]" } else { "[direct]" }
        Write-Host "  $($i + 1)) $($p.Label)  $tag"
    }
    Write-Host ""
    $idx = Read-NumberedChoice -Prompt "Select NAT mode (1-$($NAT_PRESETS.Count))" -OptionCount $NAT_PRESETS.Count
    return $NAT_PRESETS[$idx - 1]
}

function Setup-DockerNat {
    param([string]$NatA, [string]$NatB)
    $composeDir = Join-Path $REPO_ROOT "android\docker\nat-testbed"
    if (-not (Test-Path (Join-Path $composeDir "docker-compose.yml"))) {
        Write-Status "FAIL: android/docker/nat-testbed/docker-compose.yml not found" "Red"
        return $false
    }

    Write-Status "Checking Docker..."
    $dockerVer = docker version --format '{{.Server.Version}}' 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Status "FAIL: Docker not running. Start Docker Desktop first" "Red"
        return $false
    }
    Write-Status "Docker running (v$dockerVer)" "Green"

    Write-Status "Starting NAT containers: A=$NatA, B=$NatB"
    $env:NAT_A = $NatA
    $env:NAT_B = $NatB
    $composeFile = Join-Path $composeDir "docker-compose.yml"
    docker compose --project-directory $composeDir -f $composeFile down 2>&1 | Out-Null
    docker compose --project-directory $composeDir -f $composeFile up -d --build 2>&1 | ForEach-Object { Write-Status "  $_" "Gray" }
    $rc = $LASTEXITCODE
    Remove-Item Env:\NAT_A -ErrorAction SilentlyContinue
    Remove-Item Env:\NAT_B -ErrorAction SilentlyContinue

    if ($rc -ne 0) {
        Write-Status "FAIL: docker compose up failed" "Red"
        return $false
    }
    Start-Sleep -Seconds 2

    # Send STUN overrides to emulators
    Write-Status "Sending STUN overrides..."
    # EMU1 -> NAT A (13478/13479)
    Send-MpCommand -Serial $EMU1_SERIAL -Command "stun_override" -Extras @(
        "--es", "addrs", "10.0.2.2:13478,10.0.2.2:13479"
    )
    Write-Status "  ${EMU1_SERIAL}: STUN -> NAT A ($NatA) on 13478/13479" "Green"

    # EMU2 -> NAT B (23478/23479)
    Send-MpCommand -Serial $EMU2_SERIAL -Command "stun_override" -Extras @(
        "--es", "addrs", "10.0.2.2:23478,10.0.2.2:23479"
    )
    Write-Status "  ${EMU2_SERIAL}: STUN -> NAT B ($NatB) on 23478/23479" "Green"

    $script:dockerNatActive = $true
    return $true
}

# ── Phase A: Prerequisites ──────────────────────────────────────────────

Write-Status "=== Dual Emulator Test Setup ===" "White"
Write-Status ""

# Kill stale processes
Write-Status "Cleaning stale processes..."
Get-Process powershell -ErrorAction SilentlyContinue |
    Where-Object { $_.Id -ne $PID -and $_.StartTime -lt (Get-Date).AddMinutes(-30) } |
    ForEach-Object { try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {} }

$existingServer = Get-NetTCPConnection -LocalPort 9000 -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existingServer) {
    Write-Status "  Killing stale server on port 9000 (PID $($existingServer.OwningProcess))"
    Stop-Process -Id $existingServer.OwningProcess -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}
Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# ── Phase B: Build APK ──────────────────────────────────────────────────

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

# ── Phase C: Launch emulators ───────────────────────────────────────────

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

if (-not $emu1Running) {
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

# ── Phase D: Install APK + push game data ───────────────────────────────

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

# ── Phase E: Server + networking ────────────────────────────────────────

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

$serverLog = Join-Path $REPO_ROOT "temp\server.log"
$serverErr = Join-Path $REPO_ROOT "temp\server_err.log"
# Launch server binary directly -- do NOT wrap in pwsh.  PowerShell's
# internal pipe handling deadlocks when the native process's stderr
# output fills the internal buffer.
$serverEnvVars = @{
    SKIP_GPGS_VERIFY  = "true"
    RUST_LOG          = "info"
    RELAY_PUBLIC_ADDR = "10.0.2.2:9001"
    LOG_DIR           = (Join-Path $REPO_ROOT "temp")
}
foreach ($kv in $serverEnvVars.GetEnumerator()) {
    [System.Environment]::SetEnvironmentVariable($kv.Key, $kv.Value, "Process")
}
$script:serverProcess = Start-Process -FilePath $serverBin `
    -WorkingDirectory $serverDir `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $serverLog `
    -RedirectStandardError $serverErr
Write-Status "  Server PID: $($script:serverProcess.Id)"

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

# ── Phase F: Launch apps ────────────────────────────────────────────────

Write-Status ""
Write-Status "--- Launching app on both emulators ---" "White"

foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
    if (-not (Start-SetupActivity -Serial $serial)) {
        Write-Status "  ${serial}: setup screen not confirmed (app may still be loading)" "Yellow"
    }
}

# ── Phase G: NAT menu ──────────────────────────────────────────────────

$natPreset = Show-NatMenu
if ($natPreset.NatA) {
    Write-Status ""
    Write-Status "--- Setting up Docker NAT simulation ---" "White"
    $natOk = Setup-DockerNat -NatA $natPreset.NatA -NatB $natPreset.NatB
    if ($natOk) {
        Write-Status "Docker NAT active: A=$($natPreset.NatA), B=$($natPreset.NatB)" "Green"
    } else {
        Write-Status "Docker NAT setup failed -- continuing without NAT simulation" "Yellow"
    }
} else {
    Write-Status ""
    Write-Status "No NAT simulation -- using direct STUN connection" "White"
}

# ── Phase H: Summary + wait ─────────────────────────────────────────────

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
Write-Status "  Matchmaking server: ws://10.0.2.2:9000/ws (PID $($script:serverProcess.Id))"
Write-Status "  Server relay: 10.0.2.2:9001 (UDP)"
if ($script:dockerNatActive) {
    Write-Status "  NAT A ($($natPreset.NatA)): 10.0.2.2:13478/13479 -> STUN :3478/:3479"
    Write-Status "  NAT B ($($natPreset.NatB)): 10.0.2.2:23478/23479 -> STUN :3478/:3479"
}
Write-Status ""
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
if ($script:dockerNatActive) {
    Write-Status ""
    Write-Status '  # View Docker NAT logs:' "Gray"
    Write-Status "  docker compose -f android\docker\nat-testbed\docker-compose.yml logs -f" "Gray"
    Write-Status '  # Switch NAT mode (re-run from menu):' "Gray"
    Write-Status "  .\android\tests\test_dual_emu.ps1 -NoBuild -NoData" "Gray"
}
Write-Status ""
Write-Status "Press Ctrl+C or Enter to shut down" "Yellow"

try {
    Read-Host "Press Enter to exit"
} catch {
    # Ctrl+C
}

Cleanup
Write-Status "Done" "Green"
