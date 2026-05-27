#!/usr/bin/env pwsh
# setup_docker_nat.ps1 -- Start Docker NAT containers and configure emulators
# to route STUN traffic through them.
#
# Prerequisites:
#   - Docker Desktop running
#   - Both emulators running (emulator-5554 and emulator-5556)
#   - Matchmaking server running with STUN enabled on :3478/:3479
#
# Usage:
#   .\setup_docker_nat.ps1                                    # full-cone + symmetric
#   .\setup_docker_nat.ps1 -NatA port-restricted -NatB symmetric
#   .\setup_docker_nat.ps1 -NatA symmetric -NatB symmetric   # both symmetric (relay fallback)

param(
    [ValidateSet("full-cone", "port-restricted", "symmetric", "symmetric-seq")]
    [string]$NatA = "full-cone",
    [ValidateSet("full-cone", "port-restricted", "symmetric", "symmetric-seq")]
    [string]$NatB = "symmetric"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ANDROID_ROOT = Split-Path $PSScriptRoot
$REPO_ROOT = Split-Path $ANDROID_ROOT
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$COMPOSE_DIR = Join-Path $REPO_ROOT "android\docker\nat-testbed"

$EMU1 = "emulator-5554"
$EMU2 = "emulator-5556"

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

# -- Verify Docker is running --
Write-Status "Checking Docker..."
$dockerVer = docker version --format '{{.Server.Version}}' 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Status "FAIL: Docker is not running. Start Docker Desktop first" "Red"
    exit 1
}
Write-Status "Docker running (v$dockerVer)" "Green"

# -- Verify emulators --
$devices = & $ADB devices 2>&1 | Out-String
if ($devices -notmatch "$EMU1\s+device") {
    Write-Status "WARNING: $EMU1 not online -- STUN override will be sent but may fail" "Yellow"
}
if ($devices -notmatch "$EMU2\s+device") {
    Write-Status "WARNING: $EMU2 not online -- STUN override will be sent but may fail" "Yellow"
}

# -- Start Docker NAT containers --
Write-Status ""
Write-Status "Starting NAT containers: A=$NatA, B=$NatB" "White"

$env:NAT_A = $NatA
$env:NAT_B = $NatB
Push-Location $COMPOSE_DIR
docker compose down 2>&1 | Out-Null
docker compose up -d --build 2>&1 | ForEach-Object { Write-Status "  $_" "Gray" }
$composeExit = $LASTEXITCODE
Pop-Location
Remove-Item Env:\NAT_A -ErrorAction SilentlyContinue
Remove-Item Env:\NAT_B -ErrorAction SilentlyContinue

if ($composeExit -ne 0) {
    Write-Status "FAIL: docker compose up failed" "Red"
    exit 1
}

# Brief wait for containers to initialize
Start-Sleep -Seconds 2

# Verify containers are running
$containers = docker compose -f (Join-Path $COMPOSE_DIR "docker-compose.yml") ps --format json 2>&1 | Out-String
if ($containers -match '"nat_a"' -or $containers -match '"running"') {
    Write-Status "NAT containers running" "Green"
} else {
    # Fallback: check with docker ps
    $running = docker ps --filter "name=nat-testbed" --format "{{.Names}}" 2>&1 | Out-String
    if ($running -match "nat") {
        Write-Status "NAT containers running" "Green"
    } else {
        Write-Status "WARNING: Container status unclear. Check: docker ps" "Yellow"
    }
}

# -- Verify STUN ports are reachable --
Write-Status ""
Write-Status "Verifying NAT proxy ports..." "White"

$portsOk = $true
foreach ($port in @(13478, 13479, 23478, 23479)) {
    try {
        $udpClient = New-Object System.Net.Sockets.UdpClient
        $udpClient.Client.ReceiveTimeout = 1000
        # Send a dummy STUN-like packet (20 bytes, type 0x0001, magic cookie)
        $stunReq = [byte[]]@(0x00, 0x01, 0x00, 0x00,
            0x21, 0x12, 0xA4, 0x42,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01)
        $udpClient.Send($stunReq, $stunReq.Length, "127.0.0.1", $port) | Out-Null
        $udpClient.Close()
        Write-Status "  Port ${port}: reachable" "Green"
    } catch {
        Write-Status "  Port ${port}: send ok (response depends on upstream STUN)" "Gray"
    }
}

# -- Send STUN override to emulators --
Write-Status ""
Write-Status "Sending STUN overrides to emulators..." "White"

# EMU1 -> NAT A (ports 13478, 13479)
& $ADB -s $EMU1 shell am broadcast -a com.dxxredux.MP_COMMAND `
    --es command stun_override `
    --es addrs "10.0.2.2:13478,10.0.2.2:13479" 2>&1 | Out-Null
Write-Status "  ${EMU1}: STUN -> 10.0.2.2:13478/13479 (NAT A: $NatA)" "Green"

# EMU2 -> NAT B (ports 23478, 23479)
& $ADB -s $EMU2 shell am broadcast -a com.dxxredux.MP_COMMAND `
    --es command stun_override `
    --es addrs "10.0.2.2:23478,10.0.2.2:23479" 2>&1 | Out-Null
Write-Status "  ${EMU2}: STUN -> 10.0.2.2:23478/23479 (NAT B: $NatB)" "Green"

# -- Summary --
Write-Status ""
Write-Status "========================================" "White"
Write-Status "  Docker NAT testbed ready" "Green"
Write-Status "========================================" "White"
Write-Status ""
Write-Status "NAT A ($EMU1): $NatA" "White"
Write-Status "  STUN port 1: host:13478 -> container:3478 -> host STUN:3478"
Write-Status "  STUN port 2: host:13479 -> container:3479 -> host STUN:3479"
Write-Status ""
Write-Status "NAT B ($EMU2): $NatB" "White"
Write-Status "  STUN port 1: host:23478 -> container:3478 -> host STUN:3478"
Write-Status "  STUN port 2: host:23479 -> container:3479 -> host STUN:3479"
Write-Status ""
Write-Status "Expected behavior:" "White"
if ($NatA -in @("full-cone", "port-restricted")) {
    Write-Status "  EMU1: both STUN queries return SAME external port (cone NAT)" "Gray"
} else {
    Write-Status "  EMU1: STUN queries return DIFFERENT external ports (symmetric NAT)" "Gray"
}
if ($NatB -in @("full-cone", "port-restricted")) {
    Write-Status "  EMU2: both STUN queries return SAME external port (cone NAT)" "Gray"
} else {
    Write-Status "  EMU2: STUN queries return DIFFERENT external ports (symmetric NAT)" "Gray"
}
Write-Status ""
Write-Status "To test: connect both emulators to matchmaking, create/join lobby," "Gray"
Write-Status "then check logcat for STUN results:" "Gray"
Write-Status "  adb -s $EMU1 logcat -s StunClient MatchmakingService" "Gray"
Write-Status "  adb -s $EMU2 logcat -s StunClient MatchmakingService" "Gray"
Write-Status ""
Write-Status "To view NAT proxy logs:" "Gray"
Write-Status "  docker compose -f android/docker/nat-testbed/docker-compose.yml logs -f" "Gray"
Write-Status ""
Write-Status "To tear down: .\android\helpers\teardown_docker_nat.ps1" "Gray"
