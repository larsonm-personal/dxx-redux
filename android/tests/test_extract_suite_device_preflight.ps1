#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
. (Join-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers') 'test_helpers.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

$script:callLog = @()
$script:installed = $true
$script:installSuccess = $true
$script:recoverySuccess = $true
$script:startResults = [System.Collections.Queue]::new()

function Ensure-EmulatorHealthy {
    $script:callLog += 'health'
    return $true
}

function Test-AppPackageInstalled {
    return $script:installed
}

function Install-ApkOnDevice {
    $script:callLog += 'install'
    if ($script:installSuccess) {
        $script:installed = $true
    }
    return $script:installSuccess
}

function Start-PrimarySetupActivity {
    param([int]$TimeoutSeconds)
    $script:callLog += "start:$TimeoutSeconds"
    return $script:startResults.Dequeue()
}

function Invoke-LauncherStartupRecovery {
    param([string]$Reason)
    $script:callLog += 'recover'
    return $script:recoverySuccess
}

function Stop-AppAndWait {
    $script:callLog += 'stop'
}

$script:startResults.Enqueue($true)
$result = Ensure-LauncherTestDeviceReady -TimeoutSeconds 17
Assert-True $result 'An installed responsive launcher should pass preflight'
Assert-True (($script:callLog -join ',') -eq 'health,start:17,stop') 'Ready launcher preflight called unexpected operations'

$script:callLog = @()
$script:installed = $false
$script:startResults.Enqueue($true)
$result = Ensure-LauncherTestDeviceReady -TimeoutSeconds 19
Assert-True $result 'A missing APK should be installed before launcher preflight'
Assert-True (($script:callLog -join ',') -eq 'health,install,start:19,stop') 'Missing APK preflight sequence is incorrect'

$script:callLog = @()
$script:installed = $true
$script:startResults.Enqueue($false)
$script:startResults.Enqueue($true)
$result = Ensure-LauncherTestDeviceReady -TimeoutSeconds 23
Assert-True $result 'An unresponsive launcher should recover once'
Assert-True (($script:callLog -join ',') -eq 'health,start:23,recover,health,install,start:23,stop') 'Launcher recovery sequence is incorrect'

$script:callLog = @()
$script:recoverySuccess = $false
$script:startResults.Enqueue($false)
$result = Ensure-LauncherTestDeviceReady
Assert-True (-not $result) 'Failed emulator recovery should fail preflight'
Assert-True (($script:callLog -join ',') -eq 'health,start:60,recover') 'Failed recovery should stop immediately'

Write-Host 'Extraction suite device preflight tests passed' -ForegroundColor Green
exit 0
