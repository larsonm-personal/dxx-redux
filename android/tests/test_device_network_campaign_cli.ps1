#!/usr/bin/env pwsh
# Host-only campaign argument admission and controlled process orchestration
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$tempRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'android/temp'))
$fixtureRoot = Join-Path $tempRoot ('device_network_cli_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixtureRoot -DirectoryPrefix device_network_cli_
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
try {
    & python (Join-Path $PSScriptRoot 'device_network_campaign_cli_fixture.py') $fixtureRoot
    $testExitCode = $LASTEXITCODE
} finally {
    $resolved = [IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolved.StartsWith($tempRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Fixture cleanup escaped android/temp: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
exit $testExitCode
