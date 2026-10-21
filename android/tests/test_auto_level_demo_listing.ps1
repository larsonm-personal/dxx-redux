#!/usr/bin/env pwsh
# Host-pure fixtures for the real listing boundary, without SDK/device initialization
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/auto_level_demo_listing.ps1"
$root = Join-Path ([IO.Path]::GetTempPath()) ('auto-level-listing-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$remote = 'files/d2x-redux/input_demo_recordings/new'
$two = "$remote/level1.dximdemo`r`n$remote/level2.dximdemo`r`n"
$script:response = ''
$script:query = $null
$script:calls = @()
function Adb-Timeout {
    param([string[]]$AdbArgs, [int]$Seconds, [ref]$DiagnosticResult, [switch]$IncludeStandardError)
    if ($IncludeStandardError -or $Seconds -gt 30) { throw 'Listing must use bounded stdout-only ADB' }
    $script:calls += , $AdbArgs
    $DiagnosticResult.Value = $script:query
    return $script:response
}
try {
    $cases = @(
        @{ name = 'valid two'; stdout = $two; count = 2 },
        @{ name = 'valid one'; stdout = "$remote/level1.dximdemo"; failure = 'Expected two new d2 recordings, got 1' },
        @{ name = 'stderr-only nonzero'; stderr = 'run-as: package is not debuggable'; exit_code = 1; failure = 'Recording listing failed with exit code' },
        @{ name = 'timeout'; timed_out = $true; stdout = $two; failure = 'Recording listing timed out' },
        @{ name = 'process error'; error = 'Cannot start adb'; stdout = $two; failure = 'Recording listing process error' },
        @{ name = 'stdout and stderr'; stdout = $two; stderr = 'adb daemon startup diagnostic'; count = 2 },
        @{ name = 'valid paths but failed exit'; stdout = $two; exit_code = 1; failure = 'Recording listing failed with exit code' },
        @{ name = 'missing exit'; exit_code = $null; stdout = $two; failure = 'Recording listing failed with exit code' },
        @{ name = 'diagnostic in stdout'; stdout = "run-as: diagnostic`n$remote/level1.dximdemo"; failure = 'Invalid recording path' },
        @{ name = 'wrong game'; stdout = ($two -replace 'd2x-redux', 'd1x-redux'); failure = 'Invalid recording path' },
        @{ name = 'nested path'; stdout = "$remote/nested/level1.dximdemo`n$remote/level2.dximdemo"; failure = 'Invalid recording path' },
        @{ name = 'duplicate'; stdout = "$remote/level1.dximdemo`n$remote/level1.dximdemo"; failure = 'Duplicate recording path' },
        @{ name = 'empty'; failure = 'Expected two new d2 recordings, got 0' }
    )
    foreach ($case in $cases) {
        $script:response = [string]$case.stdout
        $script:query = [ordered]@{ arguments = @('-s', 'fixture-device'); exit_code = 0; timed_out = $false; stderr = ''; error = '' }
        foreach ($key in @('exit_code', 'timed_out', 'stderr', 'error')) {
            if ($case.ContainsKey($key)) { $script:query[$key] = $case[$key] }
        }
        $failure = $null
        try {
            $paths = @(Get-AutoLevelRecordingPaths -Game d2 -Package fixture.package -Serial fixture-device -OutputDirectory $root)
        } catch { $failure = $_.Exception.Message }
        if ($case.failure) {
            if (-not $failure -or -not $failure.StartsWith($case.failure)) { throw "$($case.name): unexpected failure '$failure'" }
        } elseif ($failure -or $paths.Count -ne $case.count -or ($paths -join ',') -cne "$remote/level1.dximdemo,$remote/level2.dximdemo") {
            throw "$($case.name): incorrect paths/failure '$failure'"
        }
        $saved = Get-Content -LiteralPath (Join-Path $root 'recording-listing.json') -Raw | ConvertFrom-Json
        if ($saved.stdout -cne $script:response -or $saved.diagnostic.stderr -cne $script:query.stderr -or
            $saved.diagnostic.exit_code -cne $script:query.exit_code -or $saved.diagnostic.timed_out -ne $script:query.timed_out -or
            $saved.diagnostic.error -cne $script:query.error -or $saved.serial -cne 'fixture-device' -or $saved.package -cne 'fixture.package') {
            throw "$($case.name): diagnostic evidence was lost"
        }
        Write-Host "PASS: $($case.name)"
    }
    $script:response = 'x' * 20000
    Save-AutoLevelScenarioFailure -OutputDirectory $root -Serial fixture-device -Package fixture.package
    foreach ($name in @('automation_result.json', 'automation_log.jsonl')) {
        $saved = Get-Content -LiteralPath (Join-Path $root "$name.diagnostic.json") -Raw | ConvertFrom-Json
        if ($saved.stdout.Length -ne 8192) { throw 'Scenario snapshot was not bounded' }
    }
    if (($script:calls[-1] -join ' ') -cne 'exec-out run-as fixture.package tail -c 8192 files/automation_log.jsonl') {
        throw 'Scenario capture must be bounded at the device'
    }
    Write-Host 'PASS: bounded scenario failure retention'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force
}
