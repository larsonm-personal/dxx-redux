#!/usr/bin/env pwsh
# Build real archives and verify the 22 kHz contract, repeatability and failed-batch preservation
[CmdletBinding()]
param([string]$SevenZip = '')
$ErrorActionPreference = 'Stop'
if (!$SevenZip) {
    $command = Get-Command 7z -ErrorAction SilentlyContinue
    $SevenZip = if ($command) { $command.Source } else { 'C:/Program Files/7-Zip/7z.exe' }
}
$repo = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repo 'game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1') -SevenZip $SevenZip
$root = Join-Path $repo ('android/temp/d2xxl-sound-test/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Force $root | Out-Null
function Write-SourceWav([string]$Path) {
    New-Item -ItemType Directory -Force (Split-Path $Path) | Out-Null
    $writer = [IO.BinaryWriter]::new([IO.File]::Create($Path))
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([uint32]52)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
        $writer.Write([uint32]16)
        $writer.Write([uint16]1)
        $writer.Write([uint16]1)
        $writer.Write([uint32]44100)
        $writer.Write([uint32]88200)
        $writer.Write([uint16]2)
        $writer.Write([uint16]16)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
        $writer.Write([uint32]16)
        foreach ($sample in @(-32768, -2000, 0, 8000, 32767, 10000, 0, 1000)) { $writer.Write([int16]$sample) }
    } finally { $writer.Dispose() }
}
$sourceRoot = Join-Path $root 'source'
Write-SourceWav (Join-Path $sourceRoot 'sounds/d1/laser.wav')
Write-SourceWav (Join-Path $sourceRoot 'sounds/d2/44khz/laser.wav')
$emptyWav = Join-Path $root 'empty.wav'
$bytes = [IO.File]::ReadAllBytes((Join-Path $sourceRoot 'sounds/d1/laser.wav'))[0..43]
[BitConverter]::GetBytes([uint32]36).CopyTo($bytes, 4)
[BitConverter]::GetBytes([uint32]0).CopyTo($bytes, 40)
[IO.File]::WriteAllBytes($emptyWav, $bytes)
$emptyRejected = $false
try { Read-WavData $emptyWav | Out-Null } catch {
    if ($_.Exception.Message -notmatch 'Empty PCM data:') { throw }
    $emptyRejected = $true
}
if (!$emptyRejected) { throw 'Empty PCM source accepted' }
$good = Join-Path $root 'good.7z'
Push-Location $sourceRoot
try { & $SevenZip a $good 'sounds' | Out-Null } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'Cannot build source fixture' }
foreach ($game in @('d1', 'd2')) {
    $format = Get-D2xxlSoundFormat $game
    if ($format.SampleRate -ne 22050 -or $format.Extension -ne '.r22') { throw "Wrong generated format: $game" }
    Convert-GameSounds -GameId $game -ArchivePath $good -OutDir $root -Readme 'Fixture credits' -ProjectReadme 'Fixture project'
    $target = Join-Path $root "$game-hires-sounds.dxa"
    $zip = [IO.Compression.ZipFile]::OpenRead($target)
    try {
        if ($zip.Entries.Count -ne 3 -or !$zip.GetEntry('README.md') -or !$zip.GetEntry('README.dxx-redux.md')) { throw 'Missing credits or unexpected archive contents' }
        $entry = $zip.GetEntry('Sounds/laser.r22')
        if (!$entry) { throw "Missing 22 kHz PCM: $game" }
        $reader = [IO.BinaryReader]::new($entry.Open())
        try { $pcm = $reader.ReadBytes(100) } finally { $reader.Dispose() }
        # Eight 44.1 kHz source samples become four 22.05 kHz unsigned samples
        if (($pcm -join ',') -ne '1,128,255,128') { throw "Wrong decoded PCM or playback duration: $game" }
    } finally { $zip.Dispose() }
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    $stamp = (Get-Item -LiteralPath $target).LastWriteTimeUtc.Ticks
    Convert-GameSounds -GameId $game -ArchivePath $good -OutDir $root -Readme 'Fixture credits' -ProjectReadme 'Fixture project'
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash -or (Get-Item -LiteralPath $target).LastWriteTimeUtc.Ticks -ne $stamp) { throw 'Identical conversion changed archive bytes or modification time' }
    $wavDir = if ($game -eq 'd1') { 'sounds/d1' } else { 'sounds/d2/44khz' }
    [IO.File]::WriteAllBytes((Join-Path $sourceRoot "$wavDir/broken.wav"), [byte[]]@(1, 2, 3))
    $bad = Join-Path $root "$game-bad.7z"
    Push-Location $sourceRoot
    try { & $SevenZip a $bad 'sounds' | Out-Null } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw 'Cannot build malformed source fixture' }
    $rejected = $false
    try { Convert-GameSounds -GameId $game -ArchivePath $bad -OutDir $root } catch {
        if ($_.Exception.Message -notmatch 'Sound conversion failed:') { throw }
        $rejected = $true
    }
    if (!$rejected -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash -or (Get-Item -LiteralPath $target).LastWriteTimeUtc.Ticks -ne $stamp) { throw 'Failed conversion replaced the good archive' }
    if (@(Get-ChildItem $root -Filter '*.building-*').Count) { throw 'Failed conversion left an incomplete publication candidate' }
    Write-Host "PASS: $game 22 kHz PCM, repeatability and failed-batch preservation"
}
$rejected = $false
try { Get-D2xxlSoundFormat unknown | Out-Null } catch { $rejected = $true }
if (!$rejected) { throw 'Unsupported game ID accepted' }
Write-Host "Sound conversion regression passed: $root"
