#!/usr/bin/env pwsh
# Stage the MP3 fixture before exercising launcher media controls
[CmdletBinding()]
param(
    [string]$Serial,
    [string]$AudioFile,
    [switch]$Install,
    [switch]$GeneratedAudio
)

$ErrorActionPreference = 'Stop'
$arguments = @{} + $PSBoundParameters
[void]$arguments.Remove('GeneratedAudio')
if ($GeneratedAudio) {
    if ($AudioFile) { throw 'Choose either -AudioFile or -GeneratedAudio, not both' }
    . (Join-Path $PSScriptRoot '../helpers/test_host_platform.ps1')
    $python = Resolve-RegressionPythonCommand
    $fixture = Join-Path (Split-Path $PSScriptRoot) ("temp/preview_fixture_" + [guid]::NewGuid().ToString('N'))
    & (Join-Path $PSScriptRoot '../helpers/retain-recent-artifacts.ps1') -Artifacts $fixture -DirectoryPrefix preview_fixture_
    $prefix = @($python.PrefixArguments)
    & $python.Path @prefix (Join-Path $PSScriptRoot '../helpers/generate_preview_test_audio.py') --output $fixture
    if ($LASTEXITCODE -ne 0) { throw 'Synthetic MP3 generation failed' }
    $arguments.AudioFile = Join-Path $fixture 'preview.mp3'
}
& (Join-Path $PSScriptRoot '../helpers/test_launcher_media_controls.ps1') @arguments
exit $LASTEXITCODE
