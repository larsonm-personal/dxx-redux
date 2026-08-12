#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Converts d2x-xl high-res WAV sound packs into .dxa (ZIP) files
    loadable by the DXX-Rebirth/Redux engine.

.DESCRIPTION
    Extracts WAV files from the d2x-xl hires-sounds.7z archive, converts
    them to the raw 8-bit unsigned mono format loaded by each game, and
    packages them into .dxa files. D1 loads 11025 Hz Sounds/{name}.raw;
    D2 loads 22050 Hz Sounds/{name}.r22 in its configured 22 kHz mode.

    For D2: uses the 44khz subdirectory (best quality source) and
    downsamples to 22050 Hz.
    For D1: uses the d1 subdirectory.

    The d2x-xl-specific sounds (sounds/d2x-xl/) are skipped as they have
    no matching sound IDs in the base games.

    Requires: 7-Zip (7z.exe), PowerShell 5.1+

.PARAMETER Game
    Which game to convert: "d1", "d2", or "both" (default: "both")

.PARAMETER OutputDir
    Output directory for .dxa files. Default: same directory as this script

.PARAMETER SevenZip
    Path to 7z.exe. Default: "C:\Program Files\7-Zip\7z.exe"
#>
param(
    [ValidateSet("d1", "d2", "both")]
    [string]$Game = "both",

    [string]$OutputDir = "",

    [string]$ReadmeText = "",

    [string]$ProjectReadmeText = "",

    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) { $OutputDir = $scriptDir }
$progressHelperScript = Join-Path $scriptDir "convert_progress_helpers.ps1"
if (-not (Test-Path $progressHelperScript)) {
    Write-Error "Missing helper script: $progressHelperScript"
    exit 1
}
. $progressHelperScript
$packLibScript = Join-Path $scriptDir "d2xxl_pack_lib.ps1"
if (-not (Test-Path $packLibScript)) {
    Write-Error "Missing helper script: $packLibScript"
    exit 1
}
. $packLibScript

$archivePath = Join-Path $scriptDir "d2x-xl\hires-sounds.7z"

function Read-WavData {
    <#
    .SYNOPSIS
        Parses a WAV file and returns sample data info.
        Returns hashtable with: SampleRate, BitsPerSample, Channels, Data (byte array of raw PCM)
    #>
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 44) { throw "WAV too small: $Path" }

    # RIFF header
    $riff = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4)
    if ($riff -ne "RIFF") { throw "Not a RIFF file: $Path" }

    $wave = [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4)
    if ($wave -ne "WAVE") { throw "Not a WAVE file: $Path" }

    $riffSize = [uint64][BitConverter]::ToUInt32($bytes, 4)
    $riffEnd = 8L + [int64]$riffSize
    if ($riffEnd -lt 12 -or $riffEnd -gt $bytes.LongLength) {
        throw "Invalid RIFF extent: $Path"
    }

    # Find fmt and data chunks
    $pos = 12
    $fmt = $null
    $dataBytes = $null

    while ($pos -lt $riffEnd) {
        if ($riffEnd - $pos -lt 8) { throw "Truncated WAV chunk header: $Path" }
        $chunkId = [System.Text.Encoding]::ASCII.GetString($bytes, $pos, 4)
        $chunkSize = [uint64][BitConverter]::ToUInt32($bytes, $pos + 4)
        $payloadStart = [int64]$pos + 8
        if ($chunkSize -gt [uint64][int]::MaxValue) { throw "WAV chunk is too large: $Path" }
        $payloadEnd = $payloadStart + [int64]$chunkSize
        $nextPos = $payloadEnd + [int64]($chunkSize % 2)
        if ($payloadEnd -lt $payloadStart -or $nextPos -le $pos -or $nextPos -gt $riffEnd) {
            throw "Invalid WAV chunk extent: $Path"
        }

        if ($chunkId -eq "fmt ") {
            if ($fmt) { throw "Duplicate fmt chunk: $Path" }
            if ($chunkSize -lt 16) { throw "WAV fmt chunk is too small: $Path" }
            $audioFormat = [BitConverter]::ToUInt16($bytes, [int]$payloadStart)
            $numChannels = [BitConverter]::ToUInt16($bytes, [int]$payloadStart + 2)
            $sampleRate = [BitConverter]::ToInt32($bytes, [int]$payloadStart + 4)
            $bitsPerSample = [BitConverter]::ToUInt16($bytes, [int]$payloadStart + 14)

            if ($audioFormat -ne 1) { throw "Not PCM format (format=$audioFormat): $Path" }
            if ($numChannels -eq 0 -or $sampleRate -le 0 -or $bitsPerSample -notin @(8, 16, 24)) {
                throw "Invalid PCM format fields: $Path"
            }

            $fmt = @{
                Channels = $numChannels
                SampleRate = $sampleRate
                BitsPerSample = $bitsPerSample
            }
        } elseif ($chunkId -eq "data") {
            if ($null -ne $dataBytes) { throw "Duplicate data chunk: $Path" }
            $dataBytes = New-Object byte[] ([int]$chunkSize)
            [Array]::Copy($bytes, [int]$payloadStart, $dataBytes, 0, [int]$chunkSize)
        }

        $pos = $nextPos
    }

    if (-not $fmt) { throw "No fmt chunk found: $Path" }
    if ($null -eq $dataBytes) { throw "No data chunk found: $Path" }
    $frameSize = ([int]$fmt.BitsPerSample / 8) * [int]$fmt.Channels
    if ($dataBytes.Length % $frameSize -ne 0) { throw "PCM data is not frame-aligned: $Path" }

    $fmt.Data = $dataBytes
    return $fmt
}

function Convert-ToUnsignedMonoPcm {
    <#
    .SYNOPSIS
        Converts WAV sample data to 8-bit unsigned mono at the requested rate.
        Returns byte array of raw PCM data.
    #>
    param(
        [hashtable]$Wav,
        [int]$TargetRate
    )

    $srcRate = $Wav.SampleRate
    $srcBits = $Wav.BitsPerSample
    $srcCh = $Wav.Channels
    $data = $Wav.Data

    $bytesPerSample = $srcBits / 8
    $frameSize = $bytesPerSample * $srcCh
    $numFrames = $data.Length / $frameSize

    # Convert to float mono first
    $floatSamples = New-Object double[] $numFrames
    for ($i = 0; $i -lt $numFrames; $i++) {
        $offset = $i * $frameSize
        $sum = 0.0

        for ($ch = 0; $ch -lt $srcCh; $ch++) {
            $chOffset = $offset + $ch * $bytesPerSample
            if ($srcBits -eq 8) {
                # 8-bit unsigned: 0-255, center at 128
                $sum += ($data[$chOffset] - 128) / 128.0
            } elseif ($srcBits -eq 16) {
                $sample = [BitConverter]::ToInt16($data, $chOffset)
                $sum += $sample / 32768.0
            } elseif ($srcBits -eq 24) {
                # 24-bit signed little-endian
                $val = $data[$chOffset] -bor ($data[$chOffset + 1] -shl 8) -bor ($data[$chOffset + 2] -shl 16)
                if ($val -band 0x800000) { $val = $val -bor (-1 -bxor 0xFFFFFF) }
                $sum += $val / 8388608.0
            } else {
                throw "Unsupported bit depth: $srcBits"
            }
        }

        $floatSamples[$i] = $sum / $srcCh
    }

    # Resample to target rate using linear interpolation
    $ratio = $srcRate / $TargetRate
    $outLen = [int][Math]::Floor($numFrames / $ratio)
    $output = New-Object byte[] $outLen

    for ($i = 0; $i -lt $outLen; $i++) {
        $srcPos = $i * $ratio
        $idx = [int][Math]::Floor($srcPos)
        $frac = $srcPos - $idx

        $s0 = $floatSamples[$idx]
        $s1 = if ($idx + 1 -lt $numFrames) { $floatSamples[$idx + 1] } else { $s0 }
        $val = $s0 + ($s1 - $s0) * $frac

        # Convert to 8-bit unsigned (0-255, center at 128)
        $byteVal = [int]([Math]::Round($val * 127 + 128))
        if ($byteVal -lt 0) { $byteVal = 0 }
        if ($byteVal -gt 255) { $byteVal = 255 }
        $output[$i] = [byte]$byteVal
    }

    return $output
}

function Convert-GameSounds {
    param(
        [string]$GameId,  # "d1" or "d2"
        [string]$ArchivePath,
        [string]$OutDir,
        [string]$Readme,
        [string]$ProjectReadme
    )

    if (-not (Test-Path $ArchivePath)) {
        Write-Host "Archive not found: $ArchivePath -- skipping"
        return
    }

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "dxx_snd_convert_$GameId"
    if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $dxaName = "${GameId}-hires-sounds.dxa"
    $dxaPath = Join-Path $OutDir $dxaName
    $soundFormat = Get-D2xxlSoundFormat -GameId $GameId
    $totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    Write-Host "=== Converting $GameId sounds ==="
    Write-Host "  Archive: $ArchivePath"
    Write-Host "  Output:  $dxaPath"
    Write-Host "  Format:  $($soundFormat.SampleRate) Hz $($soundFormat.Extension)"

    # Extract
    $extractDir = Join-Path $tempDir "extract"
    Invoke-7ZipExtract -SevenZipPath $SevenZip -ArchivePath $ArchivePath -ExtractDir $extractDir

    # Find the right source directory
    if ($GameId -eq "d2") {
        # Prefer 44khz, fall back to 22khz
        $wavDir = Join-Path (Join-Path (Join-Path $extractDir "sounds") "d2") "44khz"
        if (-not (Test-Path $wavDir)) {
            $wavDir = Join-Path (Join-Path (Join-Path $extractDir "sounds") "d2") "22khz"
        }
    } else {
        $wavDir = Join-Path (Join-Path $extractDir "sounds") "d1"
    }

    if (-not (Test-Path $wavDir)) {
        Write-Host "  Sound directory not found: $wavDir -- skipping"
        Remove-Item -Recurse -Force $tempDir
        return
    }

    $inventoryStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $wavFiles = Get-ChildItem -Path $wavDir -Filter "*.wav" -File
    $total = $wavFiles.Count
    Write-Host "  Inventory complete: WAV=$total in $(Format-ElapsedText $inventoryStopwatch.Elapsed)"

    # Create ZIP (dxa)
    if (Test-Path $dxaPath) { Remove-Item $dxaPath }
    Write-Host "  Creating DXA container: $dxaName"
    $zip = [System.IO.Compression.ZipFile]::Open($dxaPath, [System.IO.Compression.ZipArchiveMode]::Create)
    $conversionStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $converted = 0
    $errors = 0
    $processed = 0

    foreach ($wav in $wavFiles) {
        $processed++
        Write-ItemStartLine -Stopwatch $conversionStopwatch -Index $processed -Total $total -ItemName $wav.Name
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($wav.Name)
        $entryName = Get-D2xxlSoundEntryPath -GameId $GameId -BaseName $baseName

        try {
            $wavData = Read-WavData $wav.FullName
            $rawPcm = Convert-ToUnsignedMonoPcm -Wav $wavData -TargetRate $soundFormat.SampleRate

            $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::Optimal)
            $stream = $entry.Open()
            $stream.Write($rawPcm, 0, $rawPcm.Length)
            $stream.Close()

            $converted++
            if ($processed -le 3 -or $processed % 25 -eq 0 -or $processed -eq $total) {
                Write-ProgressSummaryLine -Stopwatch $conversionStopwatch -Processed $processed -Total $total -ItemName $wav.Name -Succeeded $converted -Errors $errors
            }
        } catch {
            Write-Host "  ERROR converting $($wav.Name): $_"
            $errors++
            Write-ProgressSummaryLine -Stopwatch $conversionStopwatch -Processed $processed -Total $total -ItemName $wav.Name -Succeeded $converted -Errors $errors
        }
    }

    $finalizeStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    if ($Readme) {
        $readmeEntry = $zip.CreateEntry("README.md", [System.IO.Compression.CompressionLevel]::Optimal)
        $readmeStream = $readmeEntry.Open()
        $readmeBytes = [System.Text.Encoding]::UTF8.GetBytes($Readme)
        $readmeStream.Write($readmeBytes, 0, $readmeBytes.Length)
        $readmeStream.Close()
    }
    if ($ProjectReadme) {
        $projectReadmeEntry = $zip.CreateEntry((Get-D2xxlProjectReadmeEntryName), [System.IO.Compression.CompressionLevel]::Optimal)
        $projectReadmeStream = $projectReadmeEntry.Open()
        $projectReadmeBytes = [System.Text.Encoding]::UTF8.GetBytes($ProjectReadme)
        $projectReadmeStream.Write($projectReadmeBytes, 0, $projectReadmeBytes.Length)
        $projectReadmeStream.Close()
    }
    Write-Host "  Finalizing archive: $dxaName"
    $zip.Dispose()
    Write-Host "  Archive finalized in $(Format-ElapsedText $finalizeStopwatch.Elapsed)"

    # Clean up temp
    Remove-Item -Recurse -Force $tempDir

    $dxaSize = (Get-Item $dxaPath).Length / 1MB
    Write-Host "  Done: processed $processed / $total, converted $converted, errors $errors in $(Format-ElapsedText $totalStopwatch.Elapsed)"
    Write-Host "  Output: $dxaPath ($([Math]::Round($dxaSize, 1)) MB)"
}

# ── Main ──

if ($MyInvocation.InvocationName -eq ".") { return }

if (-not (Test-Path $SevenZip)) {
    Write-Error "7-Zip not found at: $SevenZip"
    exit 1
}

if (-not (Test-Path $archivePath)) {
    Write-Error "Sound archive not found: $archivePath"
    exit 1
}

$games = if ($Game -eq "both") { @("d1", "d2") } else { @($Game) }

foreach ($g in $games) {
    Convert-GameSounds -GameId $g -ArchivePath $archivePath -OutDir $OutputDir -Readme $ReadmeText -ProjectReadme $ProjectReadmeText
}

Write-Host ""
Write-Host "Conversion complete. Place .dxa files in the game data directory"
Write-Host "to enable hires sounds"
