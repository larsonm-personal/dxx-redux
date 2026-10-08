#!/usr/bin/env pwsh
# Host-only native packet-log grammar and paired decoder CLI coverage
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$tempRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'android/temp'))
$fixtureRoot = Join-Path $tempRoot ('object_packet_decoders_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixtureRoot -DirectoryPrefix object_packet_decoders_
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
try {
    & python (Join-Path $PSScriptRoot 'object_packet_decoder_cli_fixture.py') $fixtureRoot
    $testExitCode = $LASTEXITCODE
} finally {
    $resolved = [IO.Path]::GetFullPath($fixtureRoot)
    if (-not $resolved.StartsWith($tempRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Fixture cleanup escaped android/temp: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
exit $testExitCode
