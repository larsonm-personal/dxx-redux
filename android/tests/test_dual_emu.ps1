#!/usr/bin/env pwsh
# test_dual_emu.ps1 -- Interactive dual-emulator multiplayer test setup.
#
# Launches two emulators, installs APK + game data, starts the matchmaking
# server and UDP relay, and then presents a menu to optionally configure
# Docker NAT simulation. Waits for manual testing when ready.
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

# Source shared env setup (JAVA_HOME, cmake, cargo)
. "$PSScriptRoot\..\test_env.ps1"

$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$ANDROID_DIR = Join-Path $REPO_ROOT "android"
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$EMULATOR = "$DEP_BASE\android-sdk\emulator\emulator.exe"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$EMU1_SERIAL = "emulator-5554"
$EMU2_SERIAL = "emulator-5556"
$AVD1 = "Nexus5X_Light_1"
$AVD2 = "Nexus5X_Light_2"

$script:serverProcess = $null
$script:relayProc = $null
$script:emu1Proc = $null
$script:emu2Proc = $null
$script:dockerNatActive = $false

$script:LogFile = Join-Path $REPO_ROOT "temp\dual_emu_log.txt"
New-Item -Path (Join-Path $REPO_ROOT "temp") -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
"" | Set-Content -Path $script:LogFile -Encoding utf8 -ErrorAction SilentlyContinue

# ── Helpers ──────────────────────────────────────────────────────────────

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    $line = "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg"
    Write-Host $line -ForegroundColor $Color
    $line | Add-Content -Path $script:LogFile -Encoding utf8
}

function Adb-Dev {
    param([string]$Serial, [string[]]$AdbArgs)
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $output = & $ADB -s $Serial @AdbArgs 2>&1 | Out-String
    $ErrorActionPreference = $prev
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
        if (-not $proc.WaitForExit($Seconds * 1000)) {
            try { $proc.Kill() } catch {}
            return $null
        }
        return $stdoutTask.GetAwaiter().GetResult().Trim()
    } catch {
        return $null
    }
}

function Test-DeviceOnline {
    param([string]$Serial)
    $devices = & $ADB devices 2>&1 | Out-String
    return $devices -match "$Serial\s+device"
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

function Setup-EmulatorRedir {
    param([int]$ConsolePort, [string]$RedirSpec)
    $tokenPath = Join-Path $env:USERPROFILE ".emulator_console_auth_token"
    if (-not (Test-Path $tokenPath)) {
        Write-Status "  WARNING: No emulator auth token at $tokenPath" "Yellow"
        return "ERROR: no auth token"
    }
    $token = (Get-Content $tokenPath).Trim()
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
    Write-Status ""
    Write-Status "Cleaning up..." "White"
    if ($script:dockerNatActive) {
        Write-Status "  Tearing down Docker NAT..."
        foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @(
                "shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND",
                "--es", "command", "stun_override_clear"
            ) -Seconds 5 | Out-Null
        }
        $composeFile = Join-Path $REPO_ROOT "docker\nat-testbed\docker-compose.yml"
        if (Test-Path $composeFile) {
            docker compose -f $composeFile down 2>&1 | Out-Null
        }
    }
    if ($script:relayProc -and -not $script:relayProc.HasExited) {
        Write-Status "  Stopping UDP relay (PID $($script:relayProc.Id))"
        try { $script:relayProc.Kill() } catch {}
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
    @{ Label = "No NAT simulation (direct STUN)";       NatA = $null;              NatB = $null }
    @{ Label = "Full-cone <-> Full-cone";                NatA = "full-cone";        NatB = "full-cone" }
    @{ Label = "Full-cone <-> Symmetric";                NatA = "full-cone";        NatB = "symmetric" }
    @{ Label = "Port-restricted <-> Symmetric";          NatA = "port-restricted";  NatB = "symmetric" }
    @{ Label = "Symmetric <-> Symmetric (relay only)";   NatA = "symmetric";        NatB = "symmetric" }
    @{ Label = "Full-cone <-> Port-restricted";          NatA = "full-cone";        NatB = "port-restricted" }
    @{ Label = "Port-restricted <-> Port-restricted";    NatA = "port-restricted";  NatB = "port-restricted" }
    @{ Label = "Symmetric-seq <-> Symmetric-seq";        NatA = "symmetric-seq";    NatB = "symmetric-seq" }
)

function Show-NatMenu {
    Write-Host ""
    Write-Host "=== NAT Simulation Mode ===" -ForegroundColor Cyan
    Write-Host ""
    for ($i = 0; $i -lt $NAT_PRESETS.Count; $i++) {
        $p = $NAT_PRESETS[$i]
        $tag = if ($p.NatA) { "[$($p.NatA) / $($p.NatB)]" } else { "[direct]" }
        Write-Host "  $i) $($p.Label)  $tag"
    }
    Write-Host ""
    $choice = Read-Host "Select NAT mode (0-$($NAT_PRESETS.Count - 1))"
    $idx = 0
    if (-not [int]::TryParse($choice, [ref]$idx) -or $idx -lt 0 -or $idx -ge $NAT_PRESETS.Count) {
        Write-Status "Invalid selection, defaulting to 0 (no NAT)" "Yellow"
        $idx = 0
    }
    return $NAT_PRESETS[$idx]
}

function Setup-DockerNat {
    param([string]$NatA, [string]$NatB)
    $composeDir = Join-Path $REPO_ROOT "docker\nat-testbed"
    if (-not (Test-Path (Join-Path $composeDir "docker-compose.yml"))) {
        Write-Status "FAIL: docker/nat-testbed/docker-compose.yml not found" "Red"
        return $false
    }

    Write-Status "Checking Docker..."
    $dockerVer = docker version --format '{{.Server.Version}}' 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Status "FAIL: Docker not running. Start Docker Desktop first." "Red"
        return $false
    }
    Write-Status "Docker running (v$dockerVer)" "Green"

    Write-Status "Starting NAT containers: A=$NatA, B=$NatB"
    $env:NAT_A = $NatA
    $env:NAT_B = $NatB
    Push-Location $composeDir
    docker compose down 2>&1 | Out-Null
    docker compose up -d --build 2>&1 | ForEach-Object { Write-Status "  $_" "Gray" }
    $rc = $LASTEXITCODE
    Pop-Location
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
    Adb-Dev-Timeout -Serial $EMU1_SERIAL -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND",
        "--es", "command", "stun_override",
        "--es", "addrs", "10.0.2.2:13478,10.0.2.2:13479"
    ) -Seconds 10 | Out-Null
    Write-Status "  ${EMU1_SERIAL}: STUN -> NAT A ($NatA) on 13478/13479" "Green"

    # EMU2 -> NAT B (23478/23479)
    Adb-Dev-Timeout -Serial $EMU2_SERIAL -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND",
        "--es", "command", "stun_override",
        "--es", "addrs", "10.0.2.2:23478,10.0.2.2:23479"
    ) -Seconds 10 | Out-Null
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
$existingRelay = Get-NetUDPEndpoint -LocalPort 42600 -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existingRelay) {
    Write-Status "  Killing stale relay on port 42600 (PID $($existingRelay.OwningProcess))"
    Stop-Process -Id $existingRelay.OwningProcess -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}
Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

# ── Phase B: Build APK ──────────────────────────────────────────────────

$APK = Join-Path $ANDROID_DIR "app\build\outputs\apk\debug\app-debug.apk"

if (-not $NoBuild) {
    Write-Status ""
    Write-Status "--- Building APK ---" "White"
    Push-Location $ANDROID_DIR
    $gradleOut = & .\gradlew.bat assembleDebug 2>&1 | Out-String
    $gradleExit = $LASTEXITCODE
    Pop-Location
    if ($gradleExit -ne 0) {
        Write-Status "FAIL: Gradle build failed" "Red"
        Write-Status ($gradleOut | Select-Object -Last 20) "Yellow"
        exit 1
    }
    Write-Status "APK built" "Green"
}
if (-not (Test-Path $APK)) {
    Write-Status "FAIL: APK not found at $APK" "Red"
    Write-Status "Run without -NoBuild, or build first." "Yellow"
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
    $script:emu1Proc = Start-Process $EMULATOR -ArgumentList "-avd",$AVD1,"-no-snapshot-save","-gpu","host" -PassThru
    Write-Status "  $AVD1 started (PID $($script:emu1Proc.Id))"
}

if ($emu2Running) {
    Write-Status "  $AVD2 ($EMU2_SERIAL) already running"
} else {
    Write-Status "  Starting $AVD2..."
    $script:emu2Proc = Start-Process $EMULATOR -ArgumentList "-avd",$AVD2,"-no-snapshot-save","-gpu","host" -PassThru
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

    $pushScript = Join-Path $ANDROID_DIR "push_game_data.sh"
    $gameDataDir = Join-Path $REPO_ROOT "game_data_to_copy_to_emulator"
    $hasData = (Test-Path (Join-Path $gameDataDir "data")) -or (Test-Path (Join-Path $gameDataDir "download"))

    if ($hasData -and (Test-Path $pushScript)) {
        foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
            Write-Status "  Pushing data to $serial..."
            $env:ANDROID_SERIAL = $serial
            $env:CALLED_FROM_SCRIPT = "1"
            $bashExe = "bash"
            if (Test-Path "$DEP_BASE\git\bin\bash.exe") { $bashExe = "$DEP_BASE\git\bin\bash.exe" }
            $pushOut = & $bashExe $pushScript 2>&1 | Out-String
            $pushExit = $LASTEXITCODE
            if ($pushExit -ne 0) {
                Write-Status "  WARNING: push_game_data.sh failed for $serial (exit $pushExit)" "Yellow"
                Write-Status ($pushOut | Select-Object -Last 10) "Gray"
            } else {
                Write-Status "  ${serial}: data pushed" "Green"
            }
        }
        Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue
        Remove-Item Env:\CALLED_FROM_SCRIPT -ErrorAction SilentlyContinue
    } else {
        Write-Status "  No game data to push (place files in game_data_to_copy_to_emulator/)" "Yellow"
    }
}

# ── Phase E: Server + networking ────────────────────────────────────────

Write-Status ""
Write-Status "--- Starting matchmaking server ---" "White"

$serverDir = Join-Path $REPO_ROOT "server"
$serverBin = Join-Path $serverDir "target\release\dxx-matchmaking.exe"

if (-not $NoBuild -or -not (Test-Path $serverBin)) {
    Write-Status "  Building matchmaking server..."
    Push-Location $serverDir
    $buildOut = & cargo build --release 2>&1 | Out-String
    Pop-Location
    if (-not (Test-Path $serverBin)) {
        Write-Status "FAIL: Server build failed" "Red"
        Write-Status ($buildOut | Select-Object -Last 10) "Yellow"
        exit 1
    }
    Write-Status "  Server built" "Green"
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
$script:serverProcess = [System.Diagnostics.Process]::Start($psi)
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

# Emulator port redirections
Write-Status ""
Write-Status "--- Setting up network ---" "White"

$r1del = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "del udp:42500"
Write-Status "  EMU1 redir cleanup: $r1del" "Gray"
$r1 = Setup-EmulatorRedir -ConsolePort 5554 -RedirSpec "add udp:42500:42424"
Write-Status "  EMU1 redir add udp:42500:42424: $r1" "Gray"
$r2del = Setup-EmulatorRedir -ConsolePort 5556 -RedirSpec "del udp:42501"
Write-Status "  EMU2 redir cleanup: $r2del" "Gray"

# UDP relay
$relayScript = Join-Path $ANDROID_DIR "udp_relay.py"
$relayLog = Join-Path $REPO_ROOT "temp\udp_relay.log"
$relayErr = Join-Path $REPO_ROOT "temp\udp_relay_err.log"
$script:relayProc = Start-Process python -ArgumentList "-u",$relayScript `
    -PassThru -NoNewWindow `
    -RedirectStandardOutput $relayLog `
    -RedirectStandardError $relayErr
Write-Status "  UDP relay started (PID $($script:relayProc.Id)) on port 42600" "Green"
Start-Sleep -Seconds 1

# ── Phase F: Launch apps ────────────────────────────────────────────────

Write-Status ""
Write-Status "--- Launching app on both emulators ---" "White"

foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
    Write-Status "  Force-stopping app on $serial..."
    Adb-Dev -Serial $serial -AdbArgs @("shell", "am", "force-stop", $PACKAGE) | Out-Null
    Start-Sleep -Seconds 1
    Adb-Dev -Serial $serial -AdbArgs @("logcat", "-c") | Out-Null
    Write-Status "  Launching SetupActivity on $serial..."
    Adb-Dev -Serial $serial -AdbArgs @("shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY") | Out-Null
}
Start-Sleep -Seconds 3

foreach ($serial in @($EMU1_SERIAL, $EMU2_SERIAL)) {
    Adb-Dev -Serial $serial -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_INTROSPECT") | Out-Null
    Start-Sleep -Milliseconds 500
    $json = Adb-Dev-Timeout -Serial $serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "cat", "files/setup_introspect.json"
    ) -Seconds 5
    if ($json -and $json -match '"screen"') {
        Write-Status "  ${serial}: setup screen ready" "Green"
    } else {
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
Write-Status "Network:" "White"
Write-Status "  Matchmaking server: ws://10.0.2.2:9000/ws (PID $($script:serverProcess.Id))"
Write-Status "  UDP relay: port 42600 (PID $($script:relayProc.Id))"
Write-Status "  EMU1 redir: host:42500 -> EMU1:42424"
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
Write-Status '  # Set join target for relay + start game:' "Gray"
Write-Status "  adb -s $EMU2_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command set_join_target --es host_addr 10.0.2.2 --ei host_port 42600" "Gray"
Write-Status "  adb -s $EMU1_SERIAL shell am broadcast -a com.dxxredux.MP_COMMAND --es command start_game" "Gray"
if ($script:dockerNatActive) {
    Write-Status ""
    Write-Status '  # View Docker NAT logs:' "Gray"
    Write-Status "  docker compose -f docker\nat-testbed\docker-compose.yml logs -f" "Gray"
    Write-Status '  # Switch NAT mode (re-run from menu):' "Gray"
    Write-Status "  .\android\tests\test_dual_emu.ps1 -NoBuild -NoData" "Gray"
}
Write-Status ""
Write-Status "Press Ctrl+C or Enter to shut down." "Yellow"

try {
    Read-Host "Press Enter to exit"
} catch {
    # Ctrl+C
}

Cleanup
Write-Status "Done." "Green"
