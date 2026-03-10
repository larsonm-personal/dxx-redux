#!/usr/bin/env pwsh
# emu_health.ps1 — Check Android emulator health and optionally restart it.
#
# Usage:
#   .\emu_health.ps1              # Check health, print status
#   .\emu_health.ps1 -Restart     # Kill and restart if unhealthy
#   .\emu_health.ps1 -Wait        # Block until emulator is fully healthy
#   .\emu_health.ps1 -Restart -Wait  # Restart if unhealthy, then wait until ready
#
# Exit codes:
#   0 = healthy
#   1 = unhealthy (no device, offline, or shell unresponsive)
#   2 = restarted (with -Restart flag)

param(
    [switch]$Restart,
    [switch]$Wait,
    [int]$TimeoutSeconds = 120
)

$_depBaseFile = Join-Path (Split-Path $PSScriptRoot) "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Error "dependency_base.txt not found at $_depBaseFile. Create it with a single line containing the path to your dependency directory (e.g. C:\local)."
    exit 1
}
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$EMULATOR = "$DEP_BASE\android-sdk\emulator\emulator.exe"
$AVD_NAME = "Pixel_6_API_34"

function Test-EmulatorHealth {
    # Step 0: Check if emulator process is actually running
    # This catches the common crash mode where the emulator process dies
    # but adb still shows a stale "device" entry.
    $emuProc = Get-Process | Where-Object { $_.ProcessName -match 'qemu-system|emulator' -and $_.Path -like "*android*" }
    if (-not $emuProc) {
        return @{ Healthy = $false; Reason = "emulator process not running (crashed)" }
    }

    # Step 1: Check if any emulator device is listed
    $devices = & $ADB devices 2>&1 | Out-String
    if ($devices -notmatch 'emulator-\d+\s+device') {
        if ($devices -match 'emulator-\d+\s+offline') {
            return @{ Healthy = $false; Reason = "emulator is offline" }
        }
        return @{ Healthy = $false; Reason = "no emulator device found" }
    }

    # Step 2: Check if shell responds (boot_completed property)
    # Use a background job with timeout to detect hangs
    try {
        $job = Start-Job -ScriptBlock {
            param($adb)
            & $adb shell getprop sys.boot_completed 2>&1 | Out-String
        } -ArgumentList $ADB
        $completed = $job | Wait-Job -Timeout 5
        if (-not $completed) {
            Remove-Job $job -Force -ErrorAction SilentlyContinue
            return @{ Healthy = $false; Reason = "shell unresponsive (getprop timed out - emulator likely frozen)" }
        }
        $result = Receive-Job $job
        Remove-Job $job -Force -ErrorAction SilentlyContinue
        if ([string]::IsNullOrWhiteSpace($result) -or $result.Trim() -ne "1") {
            return @{ Healthy = $false; Reason = "shell unresponsive (boot_completed='$($result.Trim())')" }
        }
    } catch {
        if ($job) { Remove-Job $job -Force -ErrorAction SilentlyContinue }
        return @{ Healthy = $false; Reason = "shell command failed: $_" }
    }

    return @{ Healthy = $true; Reason = "ok" }
}

function Stop-Emulator {
    Write-Host "Killing emulator..."
    # Try graceful kill (may hang if emulator is frozen, so use a timeout)
    $job = Start-Job -ScriptBlock { param($a); & $a emu kill 2>&1 } -ArgumentList $ADB
    $job | Wait-Job -Timeout 3 | Out-Null
    Remove-Job $job -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    # Force-kill emulator processes
    Get-Process | Where-Object { $_.ProcessName -match 'qemu-system|emulator' -and $_.Path -like "*android*" } |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    # Kill adb server to clear stale device entries
    & $ADB kill-server 2>&1 | Out-Null
    Start-Sleep -Seconds 2
}

function Start-EmulatorFresh {
    Write-Host "Starting emulator ($AVD_NAME)..."
    Start-Process -FilePath $EMULATOR -ArgumentList "-avd", $AVD_NAME, "-no-snapshot-load", "-gpu", "auto" -WindowStyle Minimized
}

function Wait-EmulatorHealthy {
    param([int]$Timeout = 120)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $Timeout) {
        $status = Test-EmulatorHealth
        if ($status.Healthy) {
            Write-Host "Emulator healthy after $([int]$sw.Elapsed.TotalSeconds)s"
            return $true
        }
        Write-Host "  waiting... ($($status.Reason))" 
        Start-Sleep -Seconds 5
    }
    Write-Host "ERROR: Emulator not healthy after ${Timeout}s"
    return $false
}

# ── Main ──
$status = Test-EmulatorHealth
Write-Host "Emulator status: $($status.Reason)"

if ($status.Healthy) {
    if (-not $Wait) {
        Write-Host "HEALTHY"
        exit 0
    }
    # Already healthy, just confirm
    Write-Host "HEALTHY"
    exit 0
}

# Unhealthy
if ($Restart) {
    Stop-Emulator
    Start-EmulatorFresh
    if ($Wait) {
        $ok = Wait-EmulatorHealthy -Timeout $TimeoutSeconds
        if ($ok) {
            Write-Host "RESTARTED_AND_HEALTHY"
            exit 2
        } else {
            Write-Host "RESTART_FAILED"
            exit 1
        }
    } else {
        Write-Host "RESTARTED"
        exit 2
    }
} elseif ($Wait) {
    $ok = Wait-EmulatorHealthy -Timeout $TimeoutSeconds
    if ($ok) { exit 0 } else { exit 1 }
} else {
    Write-Host "UNHEALTHY"
    exit 1
}
