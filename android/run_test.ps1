#!/usr/bin/env pwsh
# run_test.ps1 — Run an automation test script on the Android emulator with health checks.
#
# Usage:
#   .\run_test.ps1 test_death_d1_level11.json5
#   .\run_test.ps1 test_death_d1_level11.json5 -Install   # also install APK first
#   .\run_test.ps1 test_death_d1_level11.json5 -Verbose   # show all logcat
#
# The script will:
#   1. Verify emulator is healthy (restart if needed)
#   2. Push the test script to the device
#   3. Launch the app (SetupActivity → game)
#   4. Wait for the game to start, then send the automation broadcast
#   5. Tail logcat for DXX-Automate, with periodic health checks
#   6. Report PASS/FAIL and exit

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$ScriptName,
    [switch]$Install,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"
$ADB = "C:\local\android-sdk\platform-tools\adb.exe"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

# ── Helpers ──────────────────────────────────────────────────

function Adb {
    param([string[]]$Args)
    $output = & $ADB @Args 2>&1 | Out-String
    return $output.Trim()
}

function Adb-Timeout {
    # Run an adb command with a timeout; returns $null if it hangs
    param([string[]]$Args, [int]$Seconds = 8)
    $job = Start-Job -ScriptBlock {
        param($adb, $a)
        & $adb @a 2>&1 | Out-String
    } -ArgumentList $ADB, $Args
    $result = $job | Wait-Job -Timeout $Seconds | Receive-Job
    $state = $job.State
    Remove-Job $job -Force -ErrorAction SilentlyContinue
    if ($state -eq 'Running') { return $null }
    if ($null -eq $result) { return "" }
    return $result.Trim()
}

function Test-EmulatorHealthy {
    # Check if emulator process is actually running
    $emuProc = Get-Process | Where-Object { $_.ProcessName -match 'qemu-system|emulator' -and $_.Path -like "*android*" }
    if (-not $emuProc) { return $false }
    # Check adb sees it
    $devices = Adb -Args "devices"
    if ($devices -notmatch 'emulator-\d+\s+device') { return $false }
    # Check shell responds (with timeout)
    $boot = Adb-Timeout -Args @("shell", "getprop", "sys.boot_completed") -Seconds 5
    if ($null -eq $boot -or $boot -ne "1") { return $false }
    return $true
}

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

# ── Step 1: Health check ─────────────────────────────────────

Write-Status "Checking emulator health..."

if (-not (Test-EmulatorHealthy)) {
    Write-Status "Emulator unhealthy — attempting restart..." "Yellow"
    & "$SCRIPT_DIR\emu_health.ps1" -Restart -Wait -TimeoutSeconds 120
    if (-not (Test-EmulatorHealthy)) {
        Write-Status "FAIL: Emulator could not be restored" "Red"
        exit 1
    }
}
Write-Status "Emulator healthy" "Green"

# ── Step 2: Install APK if requested ────────────────────────

if ($Install) {
    $apk = "$SCRIPT_DIR\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    Adb -Args @("install", "-r", $apk) | Write-Host
}

# ── Step 3: Push test script ────────────────────────────────

$scriptPath = "$SCRIPT_DIR\game_scripts\$ScriptName"
if (-not (Test-Path $scriptPath)) {
    Write-Status "FAIL: Script not found: $scriptPath" "Red"
    exit 1
}

Write-Status "Pushing test script: $ScriptName"
Adb -Args @("push", $scriptPath, "/data/local/tmp/$ScriptName") | Out-Null
Adb -Args @("shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/$ScriptName", "files/$ScriptName") | Out-Null

# ── Step 4: Force stop and launch app ───────────────────────

Write-Status "Force-stopping app..."
Adb -Args @("shell", "am", "force-stop", $PACKAGE) | Out-Null
Start-Sleep -Seconds 2

Write-Status "Launching SetupActivity..."
Adb -Args @("shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY") | Out-Null
Start-Sleep -Seconds 3

# Launch game via setup command
Write-Status "Sending launch command..."
Adb -Args @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "launch") | Out-Null
Start-Sleep -Seconds 5

# ── Step 5: Send automation broadcast ───────────────────────

# Clear logcat
Adb -Args @("logcat", "-c") | Out-Null

Write-Status "Sending automation broadcast for: $ScriptName"
Adb -Args @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null

# ── Step 6: Monitor with health checks ──────────────────────

Write-Status "Monitoring test (timeout: ${TimeoutSeconds}s)..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastHealthCheck = 0
$finished = $false
$passed = $false

while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds -and -not $finished) {
    Start-Sleep -Seconds 3

    # Periodic health check every 15 seconds
    $elapsed = [int]$sw.Elapsed.TotalSeconds
    if ($elapsed - $lastHealthCheck -ge 15) {
        if (-not (Test-EmulatorHealthy)) {
            Write-Status "FAIL: Emulator crashed during test (after ${elapsed}s)" "Red"
            exit 1
        }
        $lastHealthCheck = $elapsed
    }

    # Check logcat for automation output
    $log = Adb-Timeout -Args @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
    if ($null -eq $log) {
        Write-Status "Warning: logcat not responding" "Yellow"
        continue
    }

    # Show new log lines
    if ($log.Length -gt 0) {
        $lines = $log -split "`n"
        foreach ($line in $lines) {
            if ($line -match 'SCRIPT_RESULT:\s*PASS') {
                Write-Status "PASS" "Green"
                $finished = $true
                $passed = $true
            }
            elseif ($line -match 'SCRIPT_RESULT:\s*FAIL') {
                Write-Status "FAIL: $line" "Red"
                $finished = $true
                $passed = $false
            }
            elseif ($line -match 'DXX-Automate' -and $line.Trim().Length -gt 0) {
                # Print automation log lines
                Write-Host "  $($line.Trim())" -ForegroundColor Gray
            }
        }
    }
}

if (-not $finished) {
    Write-Status "FAIL: Test timed out after ${TimeoutSeconds}s" "Red"
    # Dump final logcat
    $log = Adb-Timeout -Args @("logcat", "-d", "-s", "DXX-Automate:*") -Seconds 5
    if ($log) { Write-Host $log }
    exit 1
}

# ── Step 7: Dump introspection ───────────────────────────────

Write-Status "Dumping final introspection state..."
Adb -Args @("shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT") | Out-Null
Start-Sleep -Seconds 2
$intro = Adb-Timeout -Args @("shell", "run-as", $PACKAGE, "cat", "files/introspect.json") -Seconds 5
if ($intro) {
    Write-Host $intro
}

if ($passed) { exit 0 } else { exit 1 }
