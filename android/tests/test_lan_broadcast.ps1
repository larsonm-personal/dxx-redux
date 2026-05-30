#!/usr/bin/env pwsh
# test_lan_broadcast.ps1 -- Quick test of UDP broadcast between two emulators
# Requires emulator 36.5+ with shared virtual Wi-Fi LAN

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path (Join-Path (Join-Path $repoRoot "android") "helpers") "test_host_platform.ps1")
$depBase = (Get-Content (Join-Path $repoRoot "dependency_base.txt") -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$outDir = Join-Path $repoRoot "temp"

function Wait-FilePattern {
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$TimeoutSeconds = 8,
        [int]$PollMilliseconds = 100
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if (Test-Path -LiteralPath $Path) {
            $text = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
            if ($text -match $Pattern) { return $text }
        }
        Start-Sleep -Milliseconds $PollMilliseconds
    }
    if (Test-Path -LiteralPath $Path) {
        return (Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue)
    }
    return ""
}

function Stop-TestProcess {
    param([System.Diagnostics.Process]$Process)

    if ($Process -and -not $Process.HasExited) {
        try { $Process.Kill() } catch {}
    }
}

function Get-NdkVersion {
    $toolVersionsPath = Join-Path (Join-Path (Join-Path $repoRoot "android") "get_deps") "tool_versions.conf"
    if (Test-Path -LiteralPath $toolVersionsPath) {
        foreach ($line in Get-Content -LiteralPath $toolVersionsPath) {
            if ($line -match "^NDK_VERSION=(.+)$") { return $Matches[1].Trim() }
        }
    }
    return "r27d"
}

function Resolve-NativeUdpCompiler {
    param([string]$Abi)

    $toolBase = switch ($Abi) {
        "arm64-v8a" { "aarch64-linux-android23-clang" }
        "armeabi-v7a" { "armv7a-linux-androideabi23-clang" }
        "x86" { "i686-linux-android23-clang" }
        "x86_64" { "x86_64-linux-android23-clang" }
        default { throw "Unsupported emulator ABI $Abi" }
    }

    $ndkDir = Join-Path $depBase ("android-ndk-" + (Get-NdkVersion))
    $prebuiltRoot = Join-Path (Join-Path (Join-Path $ndkDir "toolchains") "llvm") "prebuilt"
    $prebuiltDir = $null
    foreach ($hostDir in @("windows-x86_64", "linux-x86_64", "darwin-x86_64", "darwin-aarch64")) {
        $candidate = Join-Path $prebuiltRoot $hostDir
        if (Test-Path -LiteralPath $candidate) {
            $prebuiltDir = $candidate
            break
        }
    }
    if (-not $prebuiltDir) { throw "Could not find Android NDK prebuilt toolchain under $prebuiltRoot" }

    $binDir = Join-Path $prebuiltDir "bin"
    foreach ($toolName in @(($toolBase + ".cmd"), $toolBase)) {
        $candidate = Join-Path $binDir $toolName
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "Could not find Android clang for ABI $Abi under $binDir"
}

function Build-NativeUdpTool {
    param([string]$Abi)

    $source = Join-Path $PSScriptRoot "udp_test_tool_android.csrc"
    $output = Join-Path $outDir ("udp_test_tool_" + $Abi)
    $clang = Resolve-NativeUdpCompiler -Abi $Abi
    & $clang "-x" "c" $source "-Os" "-Wall" "-Wextra" "-o" $output
    if ($LASTEXITCODE -ne 0) { throw "Failed to build native UDP test helper" }
    return $output
}

function Invoke-NativeUdpCheck {
    param(
        [string]$AdbPath,
        [string]$RecvSerial,
        [string]$SendSerial,
        [string]$DeviceTool,
        [int]$Port,
        [string]$Message,
        [string]$Address,
        [string]$PassMessage,
        [string]$FailMessage
    )

    $safeName = $Message.ToLowerInvariant()
    $recvFile = Join-Path $outDir ("udp_native_" + $safeName + "_out.txt")
    $recvErr = Join-Path $outDir ("udp_native_" + $safeName + "_err.txt")
    Remove-Item -LiteralPath $recvFile, $recvErr -Force -ErrorAction SilentlyContinue

    $recvProc = Start-Process -FilePath $AdbPath -ArgumentList "-s", $RecvSerial, "shell", $DeviceTool, "recv", $Port, "8" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile -RedirectStandardError $recvErr
    try {
        $recvOut = Wait-FilePattern -Path $recvFile -Pattern "READY" -TimeoutSeconds 5
        if ($recvOut -match "READY") {
            $sendOut = & $AdbPath -s $SendSerial shell $DeviceTool send $Message $Address $Port 2>&1 | Out-String
            if ($sendOut.Trim()) { Write-Host $sendOut.Trim() }
            $recvOut = Wait-FilePattern -Path $recvFile -Pattern ("RECEIVED.*" + [regex]::Escape($Message) + "|TIMEOUT") -TimeoutSeconds 9
        }

        Write-Host "Recv output: $($recvOut.Trim())"
        if ($recvOut -match ("RECEIVED.*" + [regex]::Escape($Message))) {
            Write-Host $PassMessage
        } else {
            Write-Host $FailMessage
        }
    } finally {
        Stop-TestProcess $recvProc
    }
}

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
print('READY', flush=True)
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
    Write-Host "Python unavailable on emulator, building native UDP fallback"
    $abi1 = (& $adb -s $EMU1 shell getprop ro.product.cpu.abi 2>&1 | Out-String).Trim()
    $abi2 = (& $adb -s $EMU2 shell getprop ro.product.cpu.abi 2>&1 | Out-String).Trim()
    if ($abi1 -ne $abi2) {
        Write-Host "FAIL: Emulator ABI mismatch $abi1 vs $abi2"
        exit 1
    }

    $nativeToolHost = Build-NativeUdpTool -Abi $abi1
    $nativeToolDevice = "/data/local/tmp/udp_test_tool"
    foreach ($emu in @($EMU1, $EMU2)) {
        & $adb -s $emu push $nativeToolHost $nativeToolDevice 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to push native UDP helper to $emu" }
        & $adb -s $emu shell chmod 755 $nativeToolDevice 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to chmod native UDP helper on $emu" }
    }

    Invoke-NativeUdpCheck -AdbPath $adb -RecvSerial $EMU1 -SendSerial $EMU2 -DeviceTool $nativeToolDevice -Port 42425 -Message "UNICAST_TEST" -Address $IP_EMU1 -PassMessage "PASS: UDP unicast works" -FailMessage "FAIL: UDP unicast not received"

    Write-Host ""
    Write-Host "=== Test 3: UDP broadcast EMU2 -> EMU1 on port 42426 ==="
    Invoke-NativeUdpCheck -AdbPath $adb -RecvSerial $EMU1 -SendSerial $EMU2 -DeviceTool $nativeToolDevice -Port 42426 -Message "BROADCAST_TEST" -Address "255.255.255.255" -PassMessage "PASS: UDP broadcast works" -FailMessage "FAIL: UDP broadcast not received"

    Write-Host ""
    Write-Host "=== Test 4: UDP subnet broadcast EMU2 -> EMU1 on port 42427 ==="
    Invoke-NativeUdpCheck -AdbPath $adb -RecvSerial $EMU1 -SendSerial $EMU2 -DeviceTool $nativeToolDevice -Port 42427 -Message "SUBNET_BCAST" -Address "10.0.2.255" -PassMessage "PASS: UDP subnet broadcast works" -FailMessage "FAIL: UDP subnet broadcast not received"
} else {
    $pyBin = $pyCheck.Trim()
    # Start listener on EMU1
    $recvFile = Join-Path $outDir "udp_recv_out.txt"
    $recvErr = Join-Path $outDir "udp_recv_err.txt"
    Remove-Item -LiteralPath $recvFile, $recvErr -Force -ErrorAction SilentlyContinue
    $recvProc = Start-Process -FilePath $adb -ArgumentList "-s", $EMU1, "shell", $pyBin, "/data/local/tmp/udp_recv.py" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile -RedirectStandardError $recvErr
    $recvOut = Wait-FilePattern -Path $recvFile -Pattern "READY" -TimeoutSeconds 5

    if ($recvOut -match "READY") {
        # Send unicast from EMU2
        & $adb -s $EMU2 shell "$pyBin /data/local/tmp/udp_send.py UNICAST_TEST $IP_EMU1 42425" 2>&1
        $recvOut = Wait-FilePattern -Path $recvFile -Pattern "RECEIVED.*UNICAST_TEST|TIMEOUT" -TimeoutSeconds 9
    }

    Write-Host "Recv output: $($recvOut.Trim())"
    if ($recvOut -match "RECEIVED.*UNICAST_TEST") {
        Write-Host "PASS: UDP unicast works"
    } else {
        Write-Host "FAIL: UDP unicast not received"
    }
    Stop-TestProcess $recvProc

    # Test 3: UDP broadcast
    Write-Host ""
    Write-Host "=== Test 3: UDP broadcast EMU2 -> EMU1 on port 42426 ==="
    $recvFile2 = Join-Path $outDir "udp_bcast_out.txt"
    $pyRecv2 = $pyRecv -replace "42425", "42426"
    $pyRecv2 | Set-Content (Join-Path $outDir "udp_recv2.py") -Encoding ascii
    & $adb -s $EMU1 push (Join-Path $outDir "udp_recv2.py") /data/local/tmp/udp_recv2.py 2>&1 | Out-Null

    $recvErr2 = Join-Path $outDir "udp_bcast_err.txt"
    Remove-Item -LiteralPath $recvFile2, $recvErr2 -Force -ErrorAction SilentlyContinue
    $recvProc2 = Start-Process -FilePath $adb -ArgumentList "-s", $EMU1, "shell", $pyBin, "/data/local/tmp/udp_recv2.py" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile2 -RedirectStandardError $recvErr2
    $bcastOut = Wait-FilePattern -Path $recvFile2 -Pattern "READY" -TimeoutSeconds 5

    if ($bcastOut -match "READY") {
        # Send broadcast from EMU2
        & $adb -s $EMU2 shell "$pyBin /data/local/tmp/udp_send.py BROADCAST_TEST 255.255.255.255 42426" 2>&1
        $bcastOut = Wait-FilePattern -Path $recvFile2 -Pattern "RECEIVED.*BROADCAST_TEST|TIMEOUT" -TimeoutSeconds 9
    }

    Write-Host "Recv output: $($bcastOut.Trim())"
    if ($bcastOut -match "RECEIVED.*BROADCAST_TEST") {
        Write-Host "PASS: UDP broadcast works"
    } else {
        Write-Host "FAIL: UDP broadcast not received"
    }
    Stop-TestProcess $recvProc2

    # Test 4: Subnet broadcast (10.0.2.255)
    Write-Host ""
    Write-Host "=== Test 4: UDP subnet broadcast EMU2 -> EMU1 on port 42427 ==="
    $recvFile3 = Join-Path $outDir "udp_subnet_out.txt"
    $pyRecv3 = $pyRecv -replace "42425", "42427"
    $pyRecv3 | Set-Content (Join-Path $outDir "udp_recv3.py") -Encoding ascii
    & $adb -s $EMU1 push (Join-Path $outDir "udp_recv3.py") /data/local/tmp/udp_recv3.py 2>&1 | Out-Null

    $recvErr3 = Join-Path $outDir "udp_subnet_err.txt"
    Remove-Item -LiteralPath $recvFile3, $recvErr3 -Force -ErrorAction SilentlyContinue
    $recvProc3 = Start-Process -FilePath $adb -ArgumentList "-s", $EMU1, "shell", $pyBin, "/data/local/tmp/udp_recv3.py" -PassThru -NoNewWindow -RedirectStandardOutput $recvFile3 -RedirectStandardError $recvErr3
    $subnetOut = Wait-FilePattern -Path $recvFile3 -Pattern "READY" -TimeoutSeconds 5

    if ($subnetOut -match "READY") {
        # Send subnet broadcast from EMU2
        & $adb -s $EMU2 shell "$pyBin /data/local/tmp/udp_send.py SUBNET_BCAST 10.0.2.255 42427" 2>&1
        $subnetOut = Wait-FilePattern -Path $recvFile3 -Pattern "RECEIVED.*SUBNET_BCAST|TIMEOUT" -TimeoutSeconds 9
    }

    Write-Host "Recv output: $($subnetOut.Trim())"
    if ($subnetOut -match "RECEIVED.*SUBNET_BCAST") {
        Write-Host "PASS: UDP subnet broadcast works"
    } else {
        Write-Host "FAIL: UDP subnet broadcast not received"
    }
    Stop-TestProcess $recvProc3
}

Write-Host ""
Write-Host "=== Summary ==="
Write-Host "Emulators: $EMU1 ($IP_EMU1), $EMU2 ($IP_EMU2)"
Write-Host "Tests complete"
