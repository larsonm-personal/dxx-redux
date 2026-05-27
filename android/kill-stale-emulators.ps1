#!/usr/bin/env pwsh
# kill-stale-emulators.ps1 -- Detect and optionally kill stale Android emulator state
#
# Usage:
#   .\kill-stale-emulators.ps1          # detect only
#   .\kill-stale-emulators.ps1 -Kill    # force-kill stale state when detected
#
# Exit codes:
#   0 = no stale state detected
#   1 = stale state detected and not cleaned
#   2 = stale state detected and cleaned

param(
    [switch]$Kill
)

$ErrorActionPreference = "Continue"

$_depBaseFile = Join-Path (Split-Path $PSScriptRoot) "dependency_base.txt"
if (-not (Test-Path $_depBaseFile)) {
    Write-Host "dependency_base.txt not found at $_depBaseFile" -ForegroundColor Red
    exit 1
}

$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
. (Join-Path $PSScriptRoot "test_host_platform.ps1")
$ADB = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "platform-tools" -ToolName "adb"

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

function Get-OfflineEmulatorSerials {
    $devices = (& $ADB devices 2>&1) | Out-String
    if (-not $devices) {
        return @()
    }

    return @(
        [regex]::Matches($devices, "(emulator-\d+)\s+offline") |
            ForEach-Object { $_.Groups[1].Value } |
            Sort-Object -Unique
    )
}

function Get-EmulatorProcessRecords {
    param([switch]$IncludeCrashHandlers)

    if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue) {
        $namePattern = if ($IncludeCrashHandlers) {
            '^(emulator|qemu-system.*|crashpad_handler)\.exe$'
        } else {
            '^(emulator|qemu-system.*)\.exe$'
        }

        return @(
            Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
                Where-Object {
                    ($_.Name -match $namePattern) -and
                    (
                        $_.CommandLine -match '\s-avd\s' -or
                        $_.CommandLine -match 'qemu-system' -or
                        ($IncludeCrashHandlers -and $_.CommandLine -match 'AndroidEmulator|android-sdk|[\\/]emulator[\\/]')
                    )
                } |
                Select-Object ProcessId, Name, CommandLine
        )
    }

    return @(
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -match 'emulator|qemu-system' } |
            Select-Object Id, ProcessName |
            ForEach-Object {
                [pscustomobject]@{
                    ProcessId = $_.Id
                    Name = $_.ProcessName
                    CommandLine = ""
                }
            }
    )
}

function Stop-StaleEmulatorState {
    param(
        [string[]]$OnlineSerials,
        [string[]]$OfflineSerials,
        [object[]]$EmulatorProcesses,
        [bool]$NeedAdbReset
    )

    foreach ($serial in ($OnlineSerials + $OfflineSerials | Sort-Object -Unique)) {
        try {
            & $ADB -s $serial emu kill 2>&1 | Out-Null
        } catch {}
    }

    foreach ($proc in $EmulatorProcesses) {
        try {
            Stop-Process -Id $proc.ProcessId -Force -ErrorAction SilentlyContinue
        } catch {}
    }

    if ($NeedAdbReset -or $OfflineSerials.Count -gt 0) {
        try { & $ADB kill-server 2>&1 | Out-Null } catch {}
        Start-Sleep -Seconds 1
        try { & $ADB start-server 2>&1 | Out-Null } catch {}
    }
}

$onlineSerials = @(Get-OnlineEmulatorSerials)
$offlineSerials = @(Get-OfflineEmulatorSerials)
$emuProcs = @(Get-EmulatorProcessRecords)
$cleanupProcs = @(Get-EmulatorProcessRecords -IncludeCrashHandlers)
$crashHandlers = @($cleanupProcs | Where-Object { $_.Name -eq 'crashpad_handler' })

$reasons = @()
if ($offlineSerials.Count -gt 0) {
    $reasons += "offline adb serials detected: $($offlineSerials -join ', ')"
}
if ($emuProcs.Count -gt 0 -and $onlineSerials.Count -eq 0) {
    $reasons += "emulator processes running without online adb serial"
}
if ($onlineSerials.Count -gt 0 -and $emuProcs.Count -eq 0) {
    $reasons += "online adb serials present without emulator process"
}
if ($crashHandlers.Count -gt 0 -and $emuProcs.Count -eq 0) {
    $reasons += "emulator crash handler process remains without emulator process"
}

$isStale = $reasons.Count -gt 0

if (-not $isStale) {
    Write-Host "No stale emulator state detected" -ForegroundColor Green
    exit 0
}

Write-Host "Stale emulator state detected" -ForegroundColor Yellow
foreach ($reason in $reasons) {
    Write-Host "  - $reason" -ForegroundColor Yellow
}

if (-not $Kill) {
    Write-Host "Run with -Kill to force-clean stale state" -ForegroundColor Yellow
    exit 1
}

Stop-StaleEmulatorState -OnlineSerials $onlineSerials -OfflineSerials $offlineSerials -EmulatorProcesses $cleanupProcs -NeedAdbReset ($onlineSerials.Count -gt 0 -and $emuProcs.Count -eq 0)

Write-Host "Stale emulator cleanup requested" -ForegroundColor Green
exit 2
