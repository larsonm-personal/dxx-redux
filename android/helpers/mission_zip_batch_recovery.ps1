#!/usr/bin/env pwsh

function Test-MissionZipNeedsEmulatorRecovery {
    param([string]$Reason)

    return $Reason -match 'SetupActivity did not become ready|standard D1/D2 base data is not active after game-state reset|(?:launcher and game|launcher) process(?:es)? exited before automation produced a result|adb push failed|run-as copy failed|Emulator crashed|device .*not found|device offline|protocol fault|closed'
}

function Invoke-MissionZipPreparationWithRetry {
    param(
        [Parameter(Mandatory = $true)][string]$ZipName,
        [Parameter(Mandatory = $true)][scriptblock]$Prepare,
        [Parameter(Mandatory = $true)][scriptblock]$Recover,
        [int]$MaxAttempts = 2
    )

    if ($MaxAttempts -lt 1) {
        throw "mission ZIP preparation attempts must be at least 1"
    }

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        try {
            & $Prepare
            return
        } catch {
            $reason = $_.Exception.Message
            if ($attempt -ge $MaxAttempts -or -not (Test-MissionZipNeedsEmulatorRecovery -Reason $reason)) {
                throw
            }
            & $Recover "retrying $ZipName after preparation failure: $reason"
        }
    }
}
