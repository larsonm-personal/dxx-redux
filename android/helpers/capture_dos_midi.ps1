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
    [switch]$OplCapture,
    [switch]$SongAsTitle,
    [switch]$Launch,
    [switch]$Unattended,
    [ValidateRange(20, 600)][int]$Seconds = 60
)
$ErrorActionPreference = 'Stop'
if ($Unattended -and ($Launch -or -not $SongAsTitle -or ($MusicDevice -eq 'adlib' -and -not $OplCapture))) {
    throw '-Unattended requires -SongAsTitle and MIDI or OPL capture, and cannot be combined with -Launch'
}
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $OutputDirectory
$captureArgs = @('--source', $SourceDirectory, '--output', $OutputDirectory, '--exe', $GameExecutable, '--config', $GameConfig, '--hog', $Hog)
if ($Song) { $captureArgs += @('--song', $Song) }
$captureArgs += @('--music-device', $MusicDevice)
if ($MuteEffects) { $captureArgs += '--mute-effects' }
if ($OplCapture) { $captureArgs += '--opl-capture' }
if ($SongAsTitle) { $captureArgs += '--song-as-title' }
python "$PSScriptRoot/dos_midi_capture.py" @captureArgs
if ($LASTEXITCODE -ne 0) { throw 'DOS MIDI capture preparation failed' }
if ($Unattended) {
    $captureFormat = if ($OplCapture) { 'opl' } else { 'midi' }
    $runArgs = @('--session', $OutputDirectory, '--seconds', $Seconds, '--format', $captureFormat)
    if ($GameExecutable -ieq 'DESCENT2.EXE') { $runArgs += @('--startup-escape-at', '5', '8') }
    python "$PSScriptRoot/run_dos_midi_capture.py" @runArgs
    if ($LASTEXITCODE -ne 0) { throw 'Unattended DOS capture failed' }
} elseif ($Launch) {
    $captureRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $dosboxPath = Join-Path $captureRoot 'game/DOSBOX/DOSBox.exe'
    $configPath = Join-Path $captureRoot 'capture.conf'
    # Interactive capture: the user must see the pause prompt and game menus
    Start-Process -FilePath $dosboxPath -ArgumentList @('-conf', "`"$configPath`"") -WorkingDirectory $captureRoot -WindowStyle Normal
}
