#!/usr/bin/env pwsh
<#
.SYNOPSIS
Prepare and optionally launch an isolated GOG DOSBox MIDI capture session
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$GameExecutable = 'DESCENTR.EXE',
    [string]$GameConfig = 'DESCENT.CFG',
    [string]$Song,
    [string]$Hog = 'DESCENT.HOG',
    [ValidateSet('general-midi', 'adlib')][string]$MusicDevice = 'general-midi',
    [switch]$MuteEffects,
    [switch]$Launch
)
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $OutputDirectory
$captureArgs = @('--source', $SourceDirectory, '--output', $OutputDirectory, '--exe', $GameExecutable, '--config', $GameConfig, '--hog', $Hog)
if ($Song) { $captureArgs += @('--song', $Song) }
$captureArgs += @('--music-device', $MusicDevice)
if ($MuteEffects) { $captureArgs += '--mute-effects' }
python "$PSScriptRoot/dos_midi_capture.py" @captureArgs
if ($LASTEXITCODE -ne 0) { throw 'DOS MIDI capture preparation failed' }
if ($Launch) {
    $captureRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $dosboxPath = Join-Path $captureRoot 'game/DOSBOX/DOSBox.exe'
    $configPath = Join-Path $captureRoot 'capture.conf'
    # Interactive capture: the user must see the pause prompt and game menus
    Start-Process -FilePath $dosboxPath -ArgumentList @('-conf', "`"$configPath`"") -WorkingDirectory $captureRoot -WindowStyle Normal
}
