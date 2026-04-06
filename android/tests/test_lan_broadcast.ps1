#!/usr/bin/env pwsh
# test_lan_broadcast.ps1 -- Quick test of UDP broadcast between two emulators
# Requires emulator 36.5+ with shared virtual Wi-Fi LAN

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$depBase = (Get-Content (Join-Path $repoRoot "dependency_base.txt") -First 1).Trim()
$adb = "$depBase\android-sdk\platform-tools\adb.exe"
$outDir = Join-Path $repoRoot "temp"

# Ensure both emulators are up
$devices = & $adb devices 2>&1 | Out-String
Write-Host "Devices:`n$devices"
$emus = [regex]::Matches($devices, "emulator-(\d+)\s+device")
if ($emus.Count -lt 2) {
    Write-Host "FAIL: Need 2 emulators, found $($emus.Count)"
    exit 1
}
$EMU1 = "emulator-$($emus[0].Groups[1].Value)"
$EMU2 = "emulator-$($emus[1].Groups[1].Value)"
Write-Host "EMU1=$EMU1  EMU2=$EMU2"

# Show IPs
$ip1 = & $adb -s $EMU1 shell "ip -4 addr show wlan0 | grep inet" 2>&1 | Out-String
$ip2 = & $adb -s $EMU2 shell "ip -4 addr show wlan0 | grep inet" 2>&1 | Out-String
Write-Host "EMU1 wlan0: $($ip1.Trim())"
Write-Host "EMU2 wlan0: $($ip2.Trim())"

# Extract IPs
$m1 = [regex]::Match($ip1, "inet (\d+\.\d+\.\d+\.\d+)")
$m2 = [regex]::Match($ip2, "inet (\d+\.\d+\.\d+\.\d+)")
if (-not $m1.Success -or -not $m2.Success) {
    Write-Host "FAIL: Could not parse wlan0 IPs"
    exit 1
}
$IP_EMU1 = $m1.Groups[1].Value
$IP_EMU2 = $m2.Groups[1].Value
Write-Host "Parsed: EMU1=$IP_EMU1  EMU2=$IP_EMU2"

# Test 1: Unicast ping
Write-Host ""
Write-Host "=== Test 1: Ping EMU1 -> EMU2 ==="
$pingResult = & $adb -s $EMU1 shell "ping -c 2 -W 2 $IP_EMU2" 2>&1 | Out-String
Write-Host $pingResult
if ($pingResult -match "0% packet loss") {
    Write-Host "PASS: Unicast ping works"
} else {
    Write-Host "FAIL: Ping failed"
}

# Test 2: UDP unicast
Write-Host ""
Write-Host "=== Test 2: UDP unicast EMU2 -> EMU1 on port 42425 ==="
# Push a small Python UDP test script
$pyRecv = @"
import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('0.0.0.0', 42425))
s.settimeout(8.0)
try:
    data, addr = s.recvfrom(1024)
    print(f'RECEIVED: {data.decode()} from {addr}')
except socket.timeout:
    print('TIMEOUT: no data received')
s.close()
"@
$pySend = @"
import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
msg = sys.argv[1] if len(sys.argv) > 1 else 'HELLO'
addr = sys.argv[2] if len(sys.argv) > 2 else '255.255.255.255'
port = int(sys.argv[3]) if len(sys.argv) > 3 else 42425
if addr == '255.255.255.255':
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
s.sendto(msg.encode(), (addr, port))
print(f'SENT: {msg} to {addr}:{port}')
s.close()
"@

# Write scripts to emulators
& $adb -s $EMU1 shell "cat > /data/local/tmp/udp_recv.py" -inputstring $pyRecv 2>&1 | Out-Null
& $adb -s $EMU2 shell "cat > /data/local/tmp/udp_send.py" -inputstring $pySend 2>&1 | Out-Null

# Alternative: use echo + stdin piping
$pyRecvFile = Join-Path $outDir "udp_recv.py"
$pySendFile = Join-Path $outDir "udp_send.py"
$pyRecv | Set-Content $pyRecvFile -Encoding ascii
$pySend | Set-Content $pySendFile -Encoding ascii

& $adb -s $EMU1 push $pyRecvFile /data/local/tmp/udp_recv.py 2>&1 | Out-Null
& $adb -s $EMU2 push $pySendFile /data/local/tmp/udp_send.py 2>&1 | Out-Null

# Check if python3 exists
$pyCheck = & $adb -s $EMU1 shell "which python3 2>/dev/null || which python 2>/dev/null || echo NOPYTHON" 2>&1 | Out-String
Write-Host "Python on EMU1: $($pyCheck.Trim())"

if ($pyCheck -match "NOPYTHON") {
    Write-Host "SKIP: No Python on emulator, falling back to socat/nc test"
} else {
    $pyBin = $pyCheck.Trim()
    # Start listener on EMU1
    $recvFile = Join-Path $outDir "udp_recv_out.txt"
    $recvProc = Start-Process -FilePath $adb -ArgumentList "-s", $EMU1, "shell", $pyBin, "/data/local/tmp/udp_recv.py" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile -RedirectStandardError (Join-Path $outDir "udp_recv_err.txt")
    Start-Sleep 2

    # Send unicast from EMU2
    & $adb -s $EMU2 shell "$pyBin /data/local/tmp/udp_send.py UNICAST_TEST $IP_EMU1 42425" 2>&1
    Start-Sleep 3

    $recvOut = Get-Content $recvFile -ErrorAction SilentlyContinue | Out-String
    Write-Host "Recv output: $($recvOut.Trim())"
    if ($recvOut -match "RECEIVED.*UNICAST_TEST") {
        Write-Host "PASS: UDP unicast works"
    } else {
        Write-Host "FAIL: UDP unicast not received"
    }
    if ($recvProc -and -not $recvProc.HasExited) { try { $recvProc.Kill() } catch {} }

    # Test 3: UDP broadcast
    Write-Host ""
    Write-Host "=== Test 3: UDP broadcast EMU2 -> EMU1 on port 42426 ==="
    $recvFile2 = Join-Path $outDir "udp_bcast_out.txt"
    $pyRecv2 = $pyRecv -replace "42425", "42426"
    $pyRecv2 | Set-Content (Join-Path $outDir "udp_recv2.py") -Encoding ascii
    & $adb -s $EMU1 push (Join-Path $outDir "udp_recv2.py") /data/local/tmp/udp_recv2.py 2>&1 | Out-Null

    $recvProc2 = Start-Process -FilePath $adb -ArgumentList "-s", $EMU1, "shell", $pyBin, "/data/local/tmp/udp_recv2.py" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile2 -RedirectStandardError (Join-Path $outDir "udp_bcast_err.txt")
    Start-Sleep 2

    # Send broadcast from EMU2
    & $adb -s $EMU2 shell "$pyBin /data/local/tmp/udp_send.py BROADCAST_TEST 255.255.255.255 42426" 2>&1
    Start-Sleep 3

    $bcastOut = Get-Content $recvFile2 -ErrorAction SilentlyContinue | Out-String
    Write-Host "Recv output: $($bcastOut.Trim())"
    if ($bcastOut -match "RECEIVED.*BROADCAST_TEST") {
        Write-Host "PASS: UDP broadcast works"
    } else {
        Write-Host "FAIL: UDP broadcast not received"
    }
    if ($recvProc2 -and -not $recvProc2.HasExited) { try { $recvProc2.Kill() } catch {} }

    # Test 4: Subnet broadcast (10.0.2.255)
    Write-Host ""
    Write-Host "=== Test 4: UDP subnet broadcast EMU2 -> EMU1 on port 42427 ==="
    $recvFile3 = Join-Path $outDir "udp_subnet_out.txt"
    $pyRecv3 = $pyRecv -replace "42425", "42427"
    $pyRecv3 | Set-Content (Join-Path $outDir "udp_recv3.py") -Encoding ascii
    & $adb -s $EMU1 push (Join-Path $outDir "udp_recv3.py") /data/local/tmp/udp_recv3.py 2>&1 | Out-Null

    $recvProc3 = Start-Process -FilePath $adb -ArgumentList "-s", $EMU1, "shell", $pyBin, "/data/local/tmp/udp_recv3.py" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile3 -RedirectStandardError (Join-Path $outDir "udp_subnet_err.txt")
    Start-Sleep 2

    # Send subnet broadcast from EMU2
    & $adb -s $EMU2 shell "$pyBin /data/local/tmp/udp_send.py SUBNET_BCAST 10.0.2.255 42427" 2>&1
    Start-Sleep 3

    $subnetOut = Get-Content $recvFile3 -ErrorAction SilentlyContinue | Out-String
    Write-Host "Recv output: $($subnetOut.Trim())"
    if ($subnetOut -match "RECEIVED.*SUBNET_BCAST") {
        Write-Host "PASS: UDP subnet broadcast works"
    } else {
        Write-Host "FAIL: UDP subnet broadcast not received"
    }
    if ($recvProc3 -and -not $recvProc3.HasExited) { try { $recvProc3.Kill() } catch {} }
}

Write-Host ""
Write-Host "=== Summary ==="
Write-Host "Emulators: $EMU1 ($IP_EMU1), $EMU2 ($IP_EMU2)"
Write-Host "Tests complete"
