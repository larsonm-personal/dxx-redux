<#
.SYNOPSIS
    Converts d2x-xl high-res WAV sound packs into .dxa (ZIP) files
    loadable by the DXX-Rebirth/Redux engine.

.DESCRIPTION
    Extracts WAV files from the d2x-xl hires-sounds.7z archive, converts
    them to raw 8-bit unsigned PCM mono at 22050 Hz (.r22 format), and
    packages them into .dxa files. The engine's ds_load() function looks
    for Sounds/{name}.r22 via PhysFS.

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

    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) { $OutputDir = $scriptDir }

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

    # Find fmt and data chunks
    $pos = 12
    $fmt = $null
    $dataBytes = $null

    while ($pos -lt $bytes.Length - 8) {
        $chunkId = [System.Text.Encoding]::ASCII.GetString($bytes, $pos, 4)
        $chunkSize = [BitConverter]::ToInt32($bytes, $pos + 4)

        if ($chunkId -eq "fmt ") {
            $audioFormat   = [BitConverter]::ToUInt16($bytes, $pos + 8)
            $numChannels   = [BitConverter]::ToUInt16($bytes, $pos + 10)
            $sampleRate    = [BitConverter]::ToInt32($bytes, $pos + 12)
            $byteRate      = [BitConverter]::ToInt32($bytes, $pos + 16)
            $blockAlign    = [BitConverter]::ToUInt16($bytes, $pos + 20)
            $bitsPerSample = [BitConverter]::ToUInt16($bytes, $pos + 22)

            if ($audioFormat -ne 1) { throw "Not PCM format (format=$audioFormat): $Path" }

            $fmt = @{
                Channels = $numChannels
                SampleRate = $sampleRate
                BitsPerSample = $bitsPerSample
            }
        } elseif ($chunkId -eq "data") {
            $dataBytes = New-Object byte[] $chunkSize
            [Array]::Copy($bytes, $pos + 8, $dataBytes, 0, $chunkSize)
        }

        $pos += 8 + $chunkSize
        # Chunks are word-aligned
        if ($chunkSize % 2 -ne 0) { $pos++ }
    }

    if (-not $fmt) { throw "No fmt chunk found: $Path" }
    if (-not $dataBytes) { throw "No data chunk found: $Path" }

    $fmt.Data = $dataBytes
    return $fmt
}

function Convert-ToR22 {
    <#
    .SYNOPSIS
        Converts WAV sample data to 8-bit unsigned mono at 22050 Hz.
        Returns byte array of raw PCM data.
    #>
    param([hashtable]$Wav)

    $srcRate = $Wav.SampleRate
    $srcBits = $Wav.BitsPerSample
    $srcCh   = $Wav.Channels
    $data    = $Wav.Data

    $bytesPerSample = $srcBits / 8
    $frameSize = $bytesPerSample * $srcCh
    $numFrames = $data.Length / $frameSize

    $targetRate = 22050

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
                $val = $data[$chOffset] -bor ($data[$chOffset+1] -shl 8) -bor ($data[$chOffset+2] -shl 16)
                if ($val -band 0x800000) { $val = $val -bor (-1 -bxor 0xFFFFFF) }
                $sum += $val / 8388608.0
            } else {
                throw "Unsupported bit depth: $srcBits"
            }
        }

        $floatSamples[$i] = $sum / $srcCh
    }

    # Resample to target rate using linear interpolation
    $ratio = $srcRate / $targetRate
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
        [string]$OutDir
    )

    if (-not (Test-Path $ArchivePath)) {
        Write-Host "Archive not found: $ArchivePath -- skipping"
        return
    }

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "dxx_snd_convert_$GameId"
    if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $dxaName = "d2xxl-hires-sounds-$GameId.dxa"
    $dxaPath = Join-Path $OutDir $dxaName

    Write-Host "=== Converting $GameId sounds ==="
    Write-Host "  Archive: $ArchivePath"
    Write-Host "  Output:  $dxaPath"

    # Extract
    Write-Host "  Extracting archive..."
    $extractDir = Join-Path $tempDir "extract"
    & $SevenZip x "-o$extractDir" $ArchivePath -y 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "7z extraction failed" }

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

    $wavFiles = Get-ChildItem -Path $wavDir -Filter "*.wav" -File
    $total = $wavFiles.Count
    Write-Host "  Found $total WAV files"

    # Create ZIP (dxa)
    if (Test-Path $dxaPath) { Remove-Item $dxaPath }
    $zip = [System.IO.Compression.ZipFile]::Open($dxaPath, [System.IO.Compression.ZipArchiveMode]::Create)

    $converted = 0
    $errors = 0

    foreach ($wav in $wavFiles) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($wav.Name)
        # Sound files go in Sounds/ subdirectory with .r22 extension
        $r22Name = "Sounds/$baseName.r22"

        try {
            $wavData = Read-WavData $wav.FullName
            $rawPcm = Convert-ToR22 $wavData

            $entry = $zip.CreateEntry($r22Name, [System.IO.Compression.CompressionLevel]::Optimal)
            $stream = $entry.Open()
            $stream.Write($rawPcm, 0, $rawPcm.Length)
            $stream.Close()

            $converted++
            if ($converted % 25 -eq 0) {
                Write-Host "  Converted $converted / $total..."
            }
        } catch {
            Write-Host "  ERROR converting $($wav.Name): $_"
            $errors++
        }
    }

    $zip.Dispose()

    # Clean up temp
    Remove-Item -Recurse -Force $tempDir

    $dxaSize = (Get-Item $dxaPath).Length / 1MB
    Write-Host "  Done: $converted converted, $errors errors"
    Write-Host "  Output: $dxaPath ($([Math]::Round($dxaSize, 1)) MB)"
}

# ── Main ──

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
    Convert-GameSounds -GameId $g -ArchivePath $archivePath -OutDir $OutputDir
}

Write-Host ""
Write-Host "Conversion complete. Place .dxa files in the game data directory"
Write-Host "to enable hires sounds"
