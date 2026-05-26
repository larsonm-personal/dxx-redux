#!/usr/bin/env pwsh
# extract_mac_cd.ps1 -- Legacy reference extractor for a Mac (HFS) CD image.
#
# Pipeline:
#   1. Strip Mode1/2352 raw sector headers -> raw 2048-byte user data
#   2. Parse Apple Partition Map to find HFS partition
#   3. Extract HFS partition image
#   4. Read HFS volume with Python machfs library
#   5. Extract StuffIt Installer archive (STi2) with unar
#   6. Copy game data files (.hog, .pig, .msn, .dem) to output directory
#
# Normal desktop extraction should use extract_cd.exe. This script remains for
# legacy oracle generation when the external-tool reference path is needed.
#
# Prerequisites:
#   - Python 3 with 'machfs' package: pip install machfs
#   - unar: android/get_deps/helpers/get_unar.sh (or manually install)
#
# Usage:
#   .\extract_mac_cd.ps1 -CdFolder "game_data\CD images\Descent - Mac macplay"
#   .\extract_mac_cd.ps1 -CdFolder "game_data\CD images\Descent - Mac macplay" -Force
#
# Output: <CdFolder>\data_tracks\ containing extracted game files
param(
    [Parameter(Mandatory)][string]$CdFolder,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir
$DepBase = (Get-Content (Join-Path $RepoRoot "dependency_base.txt") -Raw).Trim()

# --- Locate tools ---

# unar
$UnarExe = Join-Path $DepBase "unar\unar.exe"
if (-not (Test-Path $UnarExe)) {
    # Try cargo-installed stuffit path, or system PATH
    $UnarExe = (Get-Command unar.exe -ErrorAction SilentlyContinue).Source
    if (-not $UnarExe) {
        Write-Error "unar.exe not found. Run android/get_deps/helpers/get_unar.sh first"
        exit 1
    }
}
Write-Host "unar: $UnarExe"

# Python
$Python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $Python) {
    $Python = (Get-Command python3 -ErrorAction SilentlyContinue).Source
}
if (-not $Python) {
    Write-Error "Python not found. Install Python 3 and 'pip install machfs'."
    exit 1
}
Write-Host "python: $Python"

# --- Validate input ---

if (-not (Test-Path $CdFolder)) {
    Write-Error "CD folder not found: $CdFolder"
    exit 1
}

$CdFolder = (Resolve-Path $CdFolder).Path
$OutputDir = Join-Path $CdFolder "data_tracks"

if ((Test-Path $OutputDir) -and -not $Force) {
    Write-Host "data_tracks/ already exists. Use -Force to re-extract"
    exit 0
}

if ($Force -and (Test-Path $OutputDir)) {
    Remove-Item -Recurse -Force $OutputDir
}

# Find the .cue and .bin files
$CueFile = Get-ChildItem -Path $CdFolder -Filter "*.cue" -File | Select-Object -First 1
if (-not $CueFile) {
    Write-Error "No .cue file found in $CdFolder"
    exit 1
}

# Parse the CUE to find the BIN filename
# Use -LiteralPath to handle filenames with brackets (e.g. "Descent [Mac].CUE")
$cueContent = Get-Content -LiteralPath $CueFile.FullName -Raw
$binMatch = [regex]::Match($cueContent, 'FILE\s+"([^"]+)"\s+BINARY')
if (-not $binMatch.Success) {
    Write-Error "Could not parse FILE directive from $($CueFile.Name)"
    exit 1
}
$BinFile = Join-Path $CdFolder $binMatch.Groups[1].Value
if (-not (Test-Path -LiteralPath $BinFile)) {
    Write-Error "BIN file not found: $BinFile"
    exit 1
}

Write-Host "`n=== Mac CD Extraction Pipeline ===" -ForegroundColor Cyan
Write-Host "  CUE: $($CueFile.Name)"
Write-Host "  BIN: $(Split-Path $BinFile -Leaf)"

# --- Create temp working directory ---

$TempDir = Join-Path $CdFolder "_mac_extract_temp"
if (Test-Path $TempDir) { Remove-Item -Recurse -Force $TempDir }
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

try {

# --- Stage 1: Parse CUE to find data track boundaries ---

Write-Host "`n--- Stage 1: Parse CUE sheet ---" -ForegroundColor Yellow

# Parse TRACK entries from CUE to find track 1 (data) boundaries
$tracks = @()
$currentTrack = $null
foreach ($line in (Get-Content -LiteralPath $CueFile.FullName)) {
    $line = $line.Trim()
    if ($line -match 'TRACK\s+(\d+)\s+(\S+)') {
        if ($currentTrack) { $tracks += $currentTrack }
        $currentTrack = @{
            Number = [int]$Matches[1]
            Mode   = $Matches[2]
            Index  = $null
        }
    }
    if ($line -match 'INDEX\s+01\s+(\d+):(\d+):(\d+)' -and $currentTrack) {
        $m = [int]$Matches[1]; $s = [int]$Matches[2]; $f = [int]$Matches[3]
        $currentTrack.Index = $m * 60 * 75 + $s * 75 + $f
    }
}
if ($currentTrack) { $tracks += $currentTrack }

$dataTrack = $tracks | Where-Object { $_.Mode -match "MODE" } | Select-Object -First 1
if (-not $dataTrack) {
    Write-Error "No data track (MODE1/MODE2) found in CUE"
    exit 1
}

# Determine sector format
$sectorSize = 2352  # raw
if ($dataTrack.Mode -match "MODE1/2048") { $sectorSize = 2048 }

$startSector = $dataTrack.Index
# End sector: next track's index or end of file
$nextTrack = $tracks | Where-Object { $_.Number -gt $dataTrack.Number } | Select-Object -First 1
$totalFileSectors = [math]::Floor((Get-Item $BinFile).Length / $sectorSize)
if ($nextTrack -and $nextTrack.Index) {
    $endSector = $nextTrack.Index
} else {
    $endSector = $totalFileSectors
}
$numSectors = $endSector - $startSector

Write-Host "  Data track: Track $($dataTrack.Number), mode $($dataTrack.Mode)"
Write-Host "  Sectors: $startSector to $($endSector - 1) ($numSectors sectors)"
Write-Host "  Sector size: $sectorSize bytes"

# --- Stage 2: Strip Mode1 headers -> raw user data ---

Write-Host "`n--- Stage 2: Extract raw user data from data track ---" -ForegroundColor Yellow

$RawDataPath = Join-Path $TempDir "data_track_raw.img"

if ($sectorSize -eq 2352) {
    # Mode1/2352: strip 16-byte header, keep 2048 user bytes, skip 288 ECC
    $f = [System.IO.File]::OpenRead($BinFile)
    $out = [System.IO.File]::Create($RawDataPath)
    $buf = New-Object byte[] 2352
    $written = 0

    for ($s = 0; $s -lt $numSectors; $s++) {
        $rawOff = [long]($startSector + $s) * 2352
        $f.Seek($rawOff, 'Begin') | Out-Null
        $n = $f.Read($buf, 0, 2352)
        if ($n -ne 2352) { Write-Warning "Short read at sector $($startSector + $s)"; break }
        $out.Write($buf, 16, 2048)
        $written++
        if ($written % 20000 -eq 0) { Write-Host "  $written / $numSectors sectors..." }
    }
    $f.Close()
    $out.Close()
    Write-Host "  Extracted $written sectors ($($written * 2048) bytes)"
} else {
    # MODE1/2048: just copy the range directly
    $f = [System.IO.File]::OpenRead($BinFile)
    $out = [System.IO.File]::Create($RawDataPath)
    $startByte = [long]$startSector * 2048
    $copyLen = [long]$numSectors * 2048
    $f.Seek($startByte, 'Begin') | Out-Null
    $buf = New-Object byte[] 65536
    $remaining = $copyLen
    while ($remaining -gt 0) {
        $toRead = [Math]::Min($remaining, $buf.Length)
        $n = $f.Read($buf, 0, $toRead)
        if ($n -le 0) { break }
        $out.Write($buf, 0, $n)
        $remaining -= $n
    }
    $f.Close()
    $out.Close()
    Write-Host "  Copied $($copyLen) bytes"
}

# --- Stage 3: Parse Apple Partition Map ---

Write-Host "`n--- Stage 3: Parse Apple Partition Map ---" -ForegroundColor Yellow

# Read just the first 64 blocks (32KB) for partition map parsing - no need to load whole image
$pmBuf = New-Object byte[] 32768
$pmStream = [System.IO.File]::OpenRead($RawDataPath)
$pmStream.Read($pmBuf, 0, 32768) | Out-Null
$pmStream.Close()

# Check DDR signature at block 0
$ddrSig = ([int]$pmBuf[0] -shl 8) -bor [int]$pmBuf[1]
if ($ddrSig -ne 0x4552) {
    Write-Error "No Apple DDR signature (expected 0x4552 'ER', got 0x$($ddrSig.ToString('X4')))"
    exit 1
}

$ddrBlockSize = ([int]$pmBuf[2] -shl 8) -bor [int]$pmBuf[3]
Write-Host "  DDR: blockSize=$ddrBlockSize"

# Read partition map entries (start at block 1, each 512 bytes)
$hfsPartOffset = [long]-1
$hfsPartSize = [long]-1

for ($pmIdx = 1; $pmIdx -lt 64; $pmIdx++) {
    $pmOff = $pmIdx * 512
    if ($pmOff + 512 -gt $pmBuf.Length) { break }

    $pmSig = ([int]$pmBuf[$pmOff] -shl 8) -bor [int]$pmBuf[$pmOff + 1]
    if ($pmSig -ne 0x504D) { break }  # "PM"

    # Type string at offset 48 (32 bytes)
    $typeBytes = New-Object byte[] 32
    [Array]::Copy($pmBuf, $pmOff + 48, $typeBytes, 0, 32)
    $type = [System.Text.Encoding]::ASCII.GetString($typeBytes).TrimEnd([char]0)

    # Physical block start (offset 8, 4 bytes big-endian)
    $pStart = ([int]$pmBuf[$pmOff + 8] -shl 24) -bor ([int]$pmBuf[$pmOff + 9] -shl 16) -bor
              ([int]$pmBuf[$pmOff + 10] -shl 8) -bor [int]$pmBuf[$pmOff + 11]
    # Block count (offset 12, 4 bytes big-endian)
    $pCount = ([int]$pmBuf[$pmOff + 12] -shl 24) -bor ([int]$pmBuf[$pmOff + 13] -shl 16) -bor
              ([int]$pmBuf[$pmOff + 14] -shl 8) -bor [int]$pmBuf[$pmOff + 15]

    Write-Host "  PM[$pmIdx]: type=$type start=$pStart count=$pCount"

    if ($type -eq "Apple_HFS") {
        $hfsPartOffset = [long]$pStart * $ddrBlockSize
        $hfsPartSize = [long]$pCount * $ddrBlockSize
    }
}

$pmBuf = $null

if ($hfsPartOffset -lt 0) {
    Write-Error "No Apple_HFS partition found in partition map"
    exit 1
}

# --- Stage 4: Extract HFS partition ---

Write-Host "`n--- Stage 4: Extract HFS partition ---" -ForegroundColor Yellow

$HfsImgPath = Join-Path $TempDir "hfs_partition.img"

# Stream-copy the HFS partition range to avoid loading full image into memory
$srcStream = [System.IO.File]::OpenRead($RawDataPath)
$dstStream = [System.IO.File]::Create($HfsImgPath)
$srcStream.Seek($hfsPartOffset, 'Begin') | Out-Null
$copyBuf = New-Object byte[] 65536
$remaining = $hfsPartSize
while ($remaining -gt 0) {
    $toRead = [Math]::Min($remaining, $copyBuf.Length)
    $n = $srcStream.Read($copyBuf, 0, $toRead)
    if ($n -le 0) { break }
    $dstStream.Write($copyBuf, 0, $n)
    $remaining -= $n
}
$srcStream.Close()
$dstStream.Close()
$copyBuf = $null
Write-Host "  HFS partition: offset=$hfsPartOffset size=$hfsPartSize bytes"
Write-Host "  Saved to $HfsImgPath"

# --- Stage 5: Extract files from HFS volume using Python machfs ---

Write-Host "`n--- Stage 5: Extract files from HFS volume ---" -ForegroundColor Yellow

$HfsExtractDir = Join-Path $TempDir "hfs_files"

# Write a small inline Python script
$pyScript = @"
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import machfs

img_path = sys.argv[1]
out_dir = sys.argv[2]

with open(img_path, 'rb') as f:
    img_data = f.read()

vol = machfs.Volume()
vol.read(img_data)
print(f'Volume: {vol.name}')

def extract(vol, out_dir, path=''):
    os.makedirs(out_dir, exist_ok=True)
    for name, obj in sorted(vol.items()):
        full = path + '/' + name if path else name
        if isinstance(obj, machfs.Folder):
            extract(obj, os.path.join(out_dir, name), full)
        elif isinstance(obj, machfs.File):
            if obj.data and len(obj.data) > 0:
                outpath = os.path.join(out_dir, name)
                with open(outpath, 'wb') as f:
                    f.write(obj.data)
                print(f'  {full} ({len(obj.data)} bytes)')

extract(vol, out_dir)
print('HFS extraction complete.')
"@

$pyFile = Join-Path $TempDir "extract_hfs.py"
$pyScript | Set-Content -Path $pyFile -Encoding UTF8

& $Python $pyFile $HfsImgPath $HfsExtractDir
if ($LASTEXITCODE -ne 0) {
    Write-Error "HFS extraction failed"
    exit 1
}

# --- Stage 6: Extract StuffIt archive with unar ---

Write-Host "`n--- Stage 6: Extract StuffIt archive with unar ---" -ForegroundColor Yellow

# Find the StuffIt installer file (look for common names)
$stuffitNames = @("Install Descent", "Install Descent 2", "Install Descent II")
$StuffitFile = $null
foreach ($name in $stuffitNames) {
    $candidate = Join-Path $HfsExtractDir $name
    if (Test-Path $candidate) {
        $StuffitFile = $candidate
        break
    }
}

# If not found by name, look for STi2 magic in all files
if (-not $StuffitFile) {
    $candidates = Get-ChildItem $HfsExtractDir -File -Recurse | Where-Object { $_.Length -gt 100000 }
    foreach ($f in $candidates) {
        $header = New-Object byte[] 4
        $stream = [System.IO.File]::OpenRead($f.FullName)
        $stream.Read($header, 0, 4) | Out-Null
        $stream.Close()
        $magic = [System.Text.Encoding]::ASCII.GetString($header)
        if ($magic -eq "STi2" -or $magic -eq "SIT!") {
            $StuffitFile = $f.FullName
            break
        }
    }
}

$UnarOutDir = Join-Path $TempDir "unar_output"

if (-not $StuffitFile) {
    Write-Warning "No StuffIt archive found in HFS volume -- will collect files directly from HFS"
} else {
    Write-Host "  StuffIt archive: $(Split-Path $StuffitFile -Leaf)"
    & $UnarExe -o $UnarOutDir $StuffitFile 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "unar extraction failed"
        exit 1
    }
    Write-Host "  unar extraction complete"
}

# --- Stage 7: Collect game data files ---

Write-Host "`n--- Stage 7: Collect game data files ---" -ForegroundColor Yellow

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# Game file extensions we care about
$gameExtensions = @(".hog", ".pig", ".ham", ".mvl", ".s11", ".s22", ".mn2", ".msn", ".dem")
$copied = 0

# Collect from unar output first (StuffIt archive contents)
$unarFiles = @()
if (Test-Path $UnarOutDir) {
    $unarFiles = @(Get-ChildItem $UnarOutDir -Recurse -File)
}
foreach ($f in $unarFiles) {
    $ext = $f.Extension.ToLower()
    if ($ext -in $gameExtensions) {
        $destPath = Join-Path $OutputDir $f.Name
        Copy-Item $f.FullName $destPath -Force
        Write-Host "  $($f.Name) ($($f.Length) bytes)"
        $copied++
    }
}

# Also collect game files directly from HFS volume (e.g. D2 Mac has .mvl files
# on the HFS volume outside the StuffIt archive)
$hfsFiles = Get-ChildItem $HfsExtractDir -Recurse -File
foreach ($f in $hfsFiles) {
    $ext = $f.Extension.ToLower()
    if ($ext -in $gameExtensions) {
        $destPath = Join-Path $OutputDir $f.Name
        # Only copy if not already present from unar (unar files take priority)
        if (-not (Test-Path $destPath)) {
            Copy-Item $f.FullName $destPath -Force
            Write-Host "  $($f.Name) ($($f.Length) bytes) [from HFS volume]"
            $copied++
        }
    }
}

# Also copy the game executable if present (for identification/hashing)
# D1 Mac: "Descent", D2 Mac: "Descent II"
$gameExeNames = @("Descent", "Descent II")
$allCandidates = @($unarFiles) + @($hfsFiles)
foreach ($exeName in $gameExeNames) {
    $gameExe = $allCandidates | Where-Object { $_.Name -eq $exeName -and -not $_.Extension } | Select-Object -First 1
    if ($gameExe) {
        Copy-Item $gameExe.FullName (Join-Path $OutputDir $exeName) -Force
        Write-Host "  $exeName (executable, $($gameExe.Length) bytes)"
        $copied++
    }
}

Write-Host "`n  Copied $copied game files to $OutputDir"

} finally {
    # --- Cleanup temp directory ---
    if (Test-Path $TempDir) {
        Write-Host "`n--- Cleanup ---" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $TempDir
        Write-Host "  Removed temp directory"
    }
}

Write-Host "`n=== Done ===" -ForegroundColor Green
Write-Host "Game data extracted to: $OutputDir"
Get-ChildItem $OutputDir | Format-Table Name, Length -AutoSize
