#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot "helpers\mission_zip_batch_recovery.ps1")

function Assert-Equal {
    param(
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)]$Actual,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if ($Expected -ne $Actual) {
        throw "$Message (expected '$Expected', got '$Actual')"
    }
}

$preparationAttempts = 0
$recoveries = 0
Invoke-MissionZipPreparationWithRetry -ZipName "transient.zip" -Prepare {
    $script:preparationAttempts++
    if ($script:preparationAttempts -eq 1) {
        throw "SetupActivity did not become ready"
    }
} -Recover {
    param($Reason)
    $script:recoveries++
    if ($Reason -notmatch 'transient\.zip') {
        throw "recovery reason did not identify the ZIP"
    }
}
Assert-Equal 2 $preparationAttempts "recoverable preparation failure was not retried once"
Assert-Equal 1 $recoveries "recoverable preparation failure did not recover once"

$preparationAttempts = 0
$recoveries = 0
try {
    Invoke-MissionZipPreparationWithRetry -ZipName "base-data.zip" -Prepare {
        $script:preparationAttempts++
        throw "standard D1/D2 base data is not active after game-state reset"
    } -Recover {
        param($Reason)
        $script:recoveries++
    }
    throw "persistent base-data failure unexpectedly passed"
} catch {
    if ($_.Exception.Message -notmatch 'standard D1/D2 base data') {
        throw
    }
}
Assert-Equal 2 $preparationAttempts "persistent base-data failure did not stop after the bounded retry"
Assert-Equal 1 $recoveries "persistent base-data failure recovered an unexpected number of times"

if (-not (Test-MissionZipNeedsEmulatorRecovery -Reason "launcher process exited before automation produced a result")) {
    throw "launcher process exit was not classified for emulator recovery"
}

$preparationAttempts = 0
$recoveries = 0
try {
    Invoke-MissionZipPreparationWithRetry -ZipName "invalid.zip" -Prepare {
        $script:preparationAttempts++
        throw "mission archive is invalid"
    } -Recover {
        param($Reason)
        $script:recoveries++
    }
    throw "non-recoverable preparation failure unexpectedly passed"
} catch {
    if ($_.Exception.Message -ne "mission archive is invalid") {
        throw
    }
}
Assert-Equal 1 $preparationAttempts "non-recoverable preparation failure was retried"
Assert-Equal 0 $recoveries "non-recoverable preparation failure triggered recovery"

Write-Host "PASS: mission ZIP preparation recovery is bounded and selective"
