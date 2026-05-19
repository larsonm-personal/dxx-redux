#!/usr/bin/env pwsh
# emu_health.ps1 -- Check Android emulator health and optionally restart it.
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
    [int]$TimeoutSeconds = 120,
    [string]$AvdName = "Nexus5X_Light_1",
    [string]$GpuRenderer = "host",
    [switch]$ForceRestart
)

# Prevent inherited ErrorActionPreference=Stop from treating adb stderr as errors
$ErrorActionPreference = 'Continue'

$_depBaseFile = Join-Path (Split-Path $PSScriptRoot) "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Error "dependency_base.txt not found at $_depBaseFile. Create it with a single line containing the path to your dependency directory (e.g. C:\local)."
    exit 1
}
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$EMULATOR = "$DEP_BASE\android-sdk\emulator\emulator.exe"

function Get-OnlineEmulatorSerials {
    $devices = (& $ADB devices 2>&1) | Out-String
    if (-not $devices) {
        return @()
    }

    return @(
        [regex]::Matches($devices, "(emulator-\d+)\s+device") |
            ForEach-Object { $_.Groups[1].Value } |
            Sort-Object -Unique
    )
}

function Test-EmulatorHealth {
    param([string]$PreferredSerial)

    # Step 0: Check if emulator process is actually running
    # This catches the common crash mode where the emulator process dies
    # but adb still shows a stale "device" entry.
    $emuProc = Get-Process | Where-Object { $_.ProcessName -match 'qemu-system|emulator' -and $_.Path -like "*android*" }
    if (-not $emuProc) {
        return @{ Healthy = $false; Reason = "emulator process not running (crashed)" }
    }

    # Step 1: Check if any emulator device is listed
    $devices = (& $ADB devices 2>&1) | Out-String
    $onlineSerials = Get-OnlineEmulatorSerials
    if ($onlineSerials.Count -eq 0) {
        if ($devices -match 'emulator-\d+\s+offline') {
            return @{ Healthy = $false; Reason = "emulator is offline" }
        }
        return @{ Healthy = $false; Reason = "no emulator device found" }
    }

    $serial = if ($PreferredSerial) { $PreferredSerial } else { $onlineSerials | Select-Object -First 1 }
    if ($PreferredSerial -and $onlineSerials -notcontains $PreferredSerial) {
        return @{ Healthy = $false; Reason = "preferred emulator $PreferredSerial not online" }
    }

    # Step 2: Check if shell responds (boot_completed property)
    # Use a background job with timeout to detect hangs
    try {
        $job = Start-Job -ScriptBlock {
            param($adb, $deviceSerial)
            & $adb -s $deviceSerial shell getprop sys.boot_completed 2>&1 | Out-String
        } -ArgumentList $ADB, $serial
        $completed = $job | Wait-Job -Timeout 5
        if (-not $completed) {
            Remove-Job $job -Force -ErrorAction SilentlyContinue
            return @{ Healthy = $false; Reason = "shell unresponsive on $serial (getprop timed out - emulator likely frozen)" }
        }
        $result = Receive-Job $job
        Remove-Job $job -Force -ErrorAction SilentlyContinue
        if ([string]::IsNullOrWhiteSpace($result) -or $result.Trim() -ne "1") {
            return @{ Healthy = $false; Reason = "shell unresponsive on $serial (boot_completed='$($result.Trim())')" }
        }
    } catch {
        if ($job) { Remove-Job $job -Force -ErrorAction SilentlyContinue }
        return @{ Healthy = $false; Reason = "shell command failed on ${serial}: $_" }
    }

    # Step 3: Check disk space on /data (warn if <500MB free)
    try {
        $dfJob = Start-Job -ScriptBlock {
            param($adb, $deviceSerial)
            & $adb -s $deviceSerial shell "df /data" 2>&1 | Out-String
        } -ArgumentList $ADB, $serial
        $dfCompleted = $dfJob | Wait-Job -Timeout 5
        if ($dfCompleted) {
            $dfOutput = Receive-Job $dfJob
            Remove-Job $dfJob -Force -ErrorAction SilentlyContinue
            # Parse df output: header line then data line with 1K-blocks
            $lines = ($dfOutput -split "`n") | Where-Object { $_ -match '/data' }
            if ($lines) {
                # df columns: Filesystem 1K-blocks Used Available Use% Mounted
                $parts = ($lines[0].Trim() -split '\s+')
                if ($parts.Count -ge 4) {
                    $availKB = 0
                    if ([int64]::TryParse($parts[3], [ref]$availKB)) {
                        $availMB = [math]::Floor($availKB / 1024)
                        if ($availMB -lt 500) {
                            return @{ Healthy = $false; Reason = "low disk space on /data for ${serial}: ${availMB}MB free (need 500MB)" }
                        }
                    }
                }
            }
        } else {
            Remove-Job $dfJob -Force -ErrorAction SilentlyContinue
            # Non-fatal: if df times out, don't fail the health check
        }
    } catch {
        # Non-fatal disk space check
    }

    return @{ Healthy = $true; Reason = "ok ($serial)" }
}

function Stop-Emulator {
    $onlineSerials = Get-OnlineEmulatorSerials
    if ($onlineSerials.Count -gt 0) {
        Write-Host "Cleaning up /data/local/tmp on online emulators..."
        foreach ($serial in $onlineSerials) {
            try {
                & $ADB -s $serial shell "rm -rf /data/local/tmp/*" 2>$null
            } catch {}
        }
    }
    Write-Host "Killing emulator..."
    foreach ($serial in $onlineSerials) {
        $job = Start-Job -ScriptBlock { param($a, $deviceSerial); & $a -s $deviceSerial emu kill 2>&1 } -ArgumentList $ADB, $serial
        $job | Wait-Job -Timeout 3 | Out-Null
        Remove-Job $job -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
    # Force-kill emulator processes
    Get-Process | Where-Object { $_.ProcessName -match 'qemu-system|emulator' -and $_.Path -like "*android*" } |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    # Kill adb server to clear stale device entries
    try { & $ADB kill-server 2>&1 | Out-Null } catch {}
    Start-Sleep -Seconds 1
}

function Start-EmulatorFresh {
    Write-Host "Starting emulator ($AvdName)..."
    Start-Process -FilePath $EMULATOR -ArgumentList "-avd", $AvdName, "-no-snapshot-load", "-no-snapshot-save", "-gpu", $GpuRenderer, "-crash-report-mode", "disabled" -WindowStyle Minimized
}

function Wait-EmulatorHealthy {
    param(
        [int]$Timeout = 120,
        [string]$PreferredSerial
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $Timeout) {
        $status = Test-EmulatorHealth -PreferredSerial $PreferredSerial
        if ($status.Healthy) {
            Write-Host "Emulator healthy after $([int]$sw.Elapsed.TotalSeconds)s"
            return $true
        }
        Write-Host "  waiting... ($($status.Reason))"
        Start-Sleep -Seconds 2
    }
    Write-Host "ERROR: Emulator not healthy after ${Timeout}s"
    return $false
}

# -- Main --
$status = Test-EmulatorHealth
Write-Host "Emulator status: $($status.Reason)"

if ($Restart -and $ForceRestart) {
    Stop-Emulator
    Start-EmulatorFresh
    if ($Wait) {
        $ok = Wait-EmulatorHealthy -Timeout $TimeoutSeconds
        if ($ok) {
            Write-Host "FORCE_RESTARTED_AND_HEALTHY"
            exit 2
        }
        Write-Host "FORCE_RESTART_FAILED"
        exit 1
    }
    Write-Host "FORCE_RESTARTED"
    exit 2
}

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
