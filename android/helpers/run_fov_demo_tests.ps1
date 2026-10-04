#!/usr/bin/env pwsh
# Record and play classic demos serially, exporting input/RNG/classic companions
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $script:REPO_ROOT ('android/temp/fov-demo-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$priorSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
try {
    foreach ($gameName in $games) {
        $destination = Join-Path $OutputDirectory $gameName
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        $name = 'fov_' + $gameName + '_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        & (Join-Path $PSScriptRoot 'run_test.ps1') -Game $gameName -ScriptName test_fov_demo_compatibility.jsonc -TimeoutSeconds 240 -Params @{DEMO_NAME = $name } *>&1 |
            Tee-Object -FilePath (Join-Path $destination 'android.txt')
        if ($LASTEXITCODE -ne 0) { throw "Android demo round trip failed for $gameName" }
        Adb -AdbArgs @('logcat', '-d') | Set-Content (Join-Path $destination 'logcat.txt') -Encoding utf8NoBOM
        foreach ($extension in @('.dximdemo', '.dximdemo.rngtrace.jsonl', '.dem')) {
            # Launch reconciliation moves demos into the engine projection
            # Export the managed source, which stays stable across launches
            $sourcePaths = @(Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'find', 'files/imported/sets/default/.content/entries', '-name', "$name$extension"))
            if ($sourcePaths.Count -ne 1) { throw "Expected one managed source for $name$extension" }
            $remote = $sourcePaths[0].Trim()
            # Base64 keeps binary classic demos intact through PowerShell pipelines
            $encoded = Adb -AdbArgs @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', $remote)
            $bytes = [Convert]::FromBase64String(($encoded -join ''))
            if (!$bytes.Length) { throw "Empty demo export: $remote" }
            [IO.File]::WriteAllBytes((Join-Path $destination ('fov_cpu_compat' + $extension)), $bytes)
        }
        Write-Output "PASS: $gameName recording and classic playback; companions: $destination"
    }
} finally {
    $env:ANDROID_SERIAL = $priorSerial
}
