#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helperPath = Join-Path $repoRoot "android\get_deps\helpers\Get-DepPlatform.ps1"
. $helperPath

$platform = Get-HostPlatform
if ($platform -eq "Unknown") {
    throw "Current PowerShell host platform was not detected"
}
if ($env:OS -eq "Windows_NT" -and $platform -ne "Windows") {
    throw "Windows host was detected as $platform"
}

$windowsPowerShell = Get-Command powershell.exe -ErrorAction SilentlyContinue
if ($windowsPowerShell) {
    $legacyResult = & $windowsPowerShell.Source -NoProfile -NonInteractive -Command `
        '& { param($Path) Set-StrictMode -Version Latest; . $Path; Get-HostPlatform }' $helperPath
    if ($LASTEXITCODE -ne 0 -or @($legacyResult)[-1] -ne "Windows") {
        throw "Windows PowerShell strict-mode platform detection failed"
    }
}

Write-Host "Dependency platform detection tests passed"
