#!/usr/bin/env pwsh
# hash_assets.ps1 -- Hash game asset files and update known_versions.json5
#
# Scans game_data_to_copy_to_emulator/, game_data/extracted/, and
# game_data/CD images/*/data_tracks/ for game assets. It also scans
# game_data/demo installers/*_extracted/ for demo helper outputs. Computes
# SHA-256 and merges into known_versions.json5.
#
# Version labels use game release versions (D1 v1.0, D2 v1.2, etc.)
# rather than disc names. Files < 2 bytes are skipped (extraction stubs).
# Idempotent: re-running with the same files produces no changes.
#
# Usage:  .\hash_assets.ps1 [-Force]   (Force re-scans even if hash exists)
param([switch]$Force)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir
$Json5File = Join-Path $RepoRoot "android\app\src\main\assets\known_versions.json5"
$ExtractDir = Join-Path $ScriptDir "extracted"
$CdImgDir = Join-Path $ScriptDir "CD images"
$DemoDir = Join-Path $ScriptDir "demo installers"
$GogDir = Join-Path $RepoRoot "game_data_to_copy_to_emulator"

$GameExtensions = @(".hog", ".pig", ".ham", ".mvl", ".s11", ".s22", ".mn2", ".dem", ".gog", ".inst")
$MinFileSize = 2  # Skip 1-byte extraction stubs

# -- Version mapping --------------------------------------------------
# Maps source folder name -> version label for all files from that folder.
# GOG directory uses file-level detection (D1 vs D2) via Get-GogVersion.

$VersionMap = @{
    # extracted/ subfolders
    "d1 mac extracted"                  = "D1 Demo (Mac)"
    "Descent Shareware_extracted"       = "D1 Demo (Mac)"
    "d2_mac_demo"                       = "D2 Demo (Mac)"
    "Descent II Preview_extracted"      = "D2 Demo (Mac)"
    "descent 1 demo 1-4_extracted"      = "D1 Demo v1.4"
    "descent 2 demo 1-0_extracted"      = "D2 Demo v1.0"
    "VERTIGO"                           = "D2 Vertigo Series"

    # CD images/ subfolders -- Mac
    "d2 mac"                                                       = "D2 Mac v1.1"

    # CD images/ subfolders -- D1
    "Descent (Europe)"                                             = "D1 v1.0"
    "Descent (Europe) (Alt)"                                       = "D1 v1.0"
    "Descent (USA)"                                                = "D1 v1.0"
    "Descent - Anniversary Edition (Brazil) (Covermount)"          = "D1 Anniversary Edition"
    "Descent - Anniversary Edition (USA)"                          = "D1 Anniversary Edition"
    "Descent - Destination Saturn (USA)"                           = "D1 Destination Saturn"
    "Descent - Levels of the World (USA)"                          = "D1 Levels of the World"
    "Descent - Mac macplay"                                        = "D1 Mac (MacPlay)"
    "d1 mac 2nd bin+cue"                                           = "D1 Mac (MacPlay)"
    "Descent - Test Flight (USA)"                                  = "D1 Test Flight"
    "Descent I and II - The Definitive Collection (Europe) (Disc 1)" = "D1 v1.5"
    "Descent I and II - The Definitive Collection (USA) (Disc 1)"    = "D1 v1.5"
    "Dimensions for Descent (USA)"                                   = "D1 Dimensions for Descent"

    # CD images/ subfolders -- D2 retail
    "Descent II (Europe)"                                          = "D2 v1.0"
    "Descent II (Europe) (v1.1)"                                   = "D2 v1.1"
    "Descent II (USA)"                                             = "D2 v1.0"
    "Descent II (USA) (Alt)"                                       = "D2 v1.0"
    "Descent II (USA) (Rerelease)"                                 = "D2 v1.1"
    "Descent II (USA) (v1.1)"                                      = "D2 v1.1"
    "Descent II (USA) (3-Level Interactive Preview)"               = "D2 Preview"
    "Descent I and II - The Definitive Collection (Europe) (Disc 2)" = "D2 v1.1"
    "Descent I and II - The Definitive Collection (USA) (Disc 2)"    = "D2 v1.1"

    # CD images/ subfolders -- D2 expansions
    "Descent I and II - The Definitive Collection (Europe) (Disc 3)" = "D2 Vertigo Series"
    "Descent I and II - The Definitive Collection (USA) (Disc 3)"    = "D2 Vertigo Series"
    "Descent II - The Vertigo Series (USA)"                          = "D2 Vertigo Series"
    "Descent II - Destination Quartzon (Europe)"                     = "D2 Destination Quartzon"
    "Descent II - Destination Quartzon (USA)"                        = "D2 Destination Quartzon"
    "Descent II - Destination Quartzon (USA) (Diamond OEM)"          = "D2 Destination Quartzon"
    "Descent II - Destination Quartzon (USA) (Logitech OEM)"        = "D2 Destination Quartzon"
    "Descent-II-Destination-Quartzon_Win_EN_ISO-Version"             = "D2 Destination Quartzon"
    "Descent II - Destination Quartzon 3D (Europe)"                  = "D2 Destination Quartzon 3D"
}

# Key game files that share the same version label across D1 v1.5 sources.
# The Anniversary Edition contains these PLUS many add-on levels; for descent.hog/pig
# the version label is overridden to "D1 v1.5" (see Resolve-Version).
$AnniversaryKeyFiles = @("descent.hog", "descent.pig")

function Get-GogVersion([string]$filename) {
    if ($filename -match '^descent\.(hog|pig)$') { return "D1 v1.4a" }
    return "D2 v1.2"
}

# -- JSON5 helpers ----------------------------------------------------

function ConvertFrom-Json5WithComments([string]$text) {
    $sb = [System.Text.StringBuilder]::new($text.Length)
    $i = 0
    while ($i -lt $text.Length) {
        if ($i + 1 -lt $text.Length -and $text[$i] -eq '/' -and $text[$i + 1] -eq '/') {
            $i += 2
            while ($i -lt $text.Length -and $text[$i] -ne "`n") { $i++ }
        } elseif ($i + 1 -lt $text.Length -and $text[$i] -eq '/' -and $text[$i + 1] -eq '*') {
            $i += 2
            while ($i + 1 -lt $text.Length -and -not ($text[$i] -eq '*' -and $text[$i + 1] -eq '/')) { $i++ }
            $i += 2
        } else {
            [void]$sb.Append($text[$i])
            $i++
        }
    }
    return $sb.ToString()
}

function Read-Json5Versions([string]$path) {
    if (-not (Test-Path $path)) { return @() }
    $raw = Get-Content -Raw $path
    $clean = ConvertFrom-Json5WithComments $raw
    $obj = $clean | ConvertFrom-Json
    return @($obj.versions)
}

function Write-Json5Versions([string]$path, $versions) {
    # Group by version for section comments
    $groups = [ordered]@{}
    foreach ($v in $versions) {
        $key = $v.version
        if (-not $groups.Contains($key)) { $groups[$key] = @() }
        $groups[$key] += $v
    }

    $lines = @()
    $lines += "// Known game asset versions - SHA-256 hashes"
    $lines += "// Maintained by game_data/hash_assets.ps1"
    $lines += "//"
    $lines += "// Each entry: { file, sha256, version }"
    $lines += "// Filenames are lowercase. Multiple versions of the same file are allowed"
    $lines += "{"
    $lines += '  "versions": ['

    $groupKeys = @($groups.Keys)
    for ($gi = 0; $gi -lt $groupKeys.Count; $gi++) {
        $gk = $groupKeys[$gi]
        $entries = $groups[$gk]
        $lines += "    // -- $gk --"
        for ($ei = 0; $ei -lt $entries.Count; $ei++) {
            $e = $entries[$ei]
            $comma = if ($gi -eq $groupKeys.Count - 1 -and $ei -eq $entries.Count - 1) { "" } else { "," }
            $lines += "    { `"file`": `"$($e.file)`", `"sha256`": `"$($e.sha256)`", `"version`": `"$($e.version)`" }$comma"
        }
        if ($gi -lt $groupKeys.Count - 1) { $lines += "" }
    }

    $lines += "  ]"
    $lines += "}"
    $lines -join "`n" | Set-Content -NoNewline $path -Encoding UTF8
}

# -- Scan directories -------------------------------------------------

function Get-GameAssetFiles([string]$baseDir) {
    if (-not (Test-Path $baseDir)) { return @() }
    Get-ChildItem -Path $baseDir -Recurse -File |
        Where-Object { $GameExtensions -contains $_.Extension.ToLower() }
}

function Resolve-Version([string]$folderName, [string]$filename, [string]$source) {
    if ($source -eq "gog") {
        return Get-GogVersion $filename
    }
    $label = $VersionMap[$folderName]
    if (-not $label) {
        Write-Warning "No version mapping for folder: $folderName"
        return $folderName
    }
    # Anniversary Edition: override descent.hog/pig to "D1 v1.5"
    if ($label -eq "D1 Anniversary Edition" -and $AnniversaryKeyFiles -contains $filename) {
        return "D1 v1.5"
    }
    return $label
}

# -- Main -------------------------------------------------------------

if ($Force) {
    Write-Host "Force mode: clearing existing entries, will regenerate all"
    $existing = @()
    $existingSet = @{}
} else {
    Write-Host "Loading existing known_versions.json5..."
    $existing = Read-Json5Versions $Json5File
    $existingSet = @{}
    foreach ($e in $existing) {
        $key = "$($e.file.ToLower())|$($e.sha256.ToLower())"
        $existingSet[$key] = $e.version
    }
    Write-Host "  $($existing.Count) existing entries"
}

$allFiles = @()

# Scan GOG files (game_data_to_copy_to_emulator/)
if (Test-Path $GogDir) {
    Write-Host "`nScanning $GogDir..."
    $files = Get-GameAssetFiles $GogDir
    foreach ($f in $files) {
        if ($f.Length -lt $MinFileSize) { continue }
        $allFiles += @{ File = $f; FolderName = "GOG"; Source = "gog" }
    }
    Write-Host "  Found $($files.Count) game asset files"
}

# Scan extracted/
if (Test-Path $ExtractDir) {
    Write-Host "`nScanning $ExtractDir..."
    $files = Get-GameAssetFiles $ExtractDir
    foreach ($f in $files) {
        if ($f.Length -lt $MinFileSize) { continue }
        $rel = $f.FullName.Substring($ExtractDir.Length).TrimStart('\', '/')
        $parts = $rel -split '[/\\]'
        $folder = if ($parts.Count -ge 2) { $parts[0] } else { "Unknown" }
        $allFiles += @{ File = $f; FolderName = $folder; Source = "extracted" }
    }
    Write-Host "  Found $($files.Count) game asset files"
}

# Scan demo installers/*_extracted/ helper outputs
if (Test-Path $DemoDir) {
    $demoExtractDirs = Get-ChildItem -LiteralPath $DemoDir -Directory -Filter "*_extracted" -ErrorAction SilentlyContinue
    foreach ($demoInfo in $demoExtractDirs) {
        Write-Host "Scanning demo extraction: $($demoInfo.Name)..."
        $files = Get-GameAssetFiles $demoInfo.FullName
        $kept = 0
        foreach ($f in $files) {
            if ($f.Length -lt $MinFileSize) { continue }
            $allFiles += @{ File = $f; FolderName = $demoInfo.Name; Source = "demo" }
            $kept++
        }
        Write-Host "  Found $($files.Count) files, kept $kept (skipped $($files.Count - $kept) stubs)"
    }
}

# Scan CD images/*/data_tracks/
if (Test-Path $CdImgDir) {
    $dtDirs = Get-ChildItem -Path $CdImgDir -Directory | ForEach-Object {
        $dt = Join-Path $_.FullName "data_tracks"
        if (Test-Path $dt) { [PSCustomObject]@{ Path = $dt; Name = $_.Name } }
    }
    foreach ($dtInfo in $dtDirs) {
        Write-Host "Scanning CD: $($dtInfo.Name)..."
        $files = Get-GameAssetFiles $dtInfo.Path
        $kept = 0
        foreach ($f in $files) {
            if ($f.Length -lt $MinFileSize) { continue }
            $allFiles += @{ File = $f; FolderName = $dtInfo.Name; Source = "cd" }
            $kept++
        }
        Write-Host "  Found $($files.Count) files, kept $kept (skipped $($files.Count - $kept) stubs)"
    }
}

if ($allFiles.Count -eq 0) {
    Write-Host "`nNo game asset files found to hash"
    exit 0
}

# Hash files and deduplicate
$newEntries = @()
$added = 0
$skipped = 0

Write-Host "`nHashing $($allFiles.Count) files..."
foreach ($item in $allFiles) {
    $f = $item.File
    $filename = $f.Name.ToLower()
    $version = Resolve-Version $item.FolderName $filename $item.Source

    $sha256 = (Get-FileHash -Path $f.FullName -Algorithm SHA256).Hash.ToLower()
    $key = "$filename|$sha256"

    if ($existingSet.ContainsKey($key)) {
        $skipped++
    } else {
        $newEntries += [PSCustomObject]@{ file = $filename; sha256 = $sha256; version = $version }
        $existingSet[$key] = $version
        $added++
        Write-Host "  + $filename ($version)"
    }
}

# Add version aliases for core game files that are identical across versions.
# These allow the identification system to match files originating from different
# retail/distribution channels even when the game data is byte-identical.
$aliases = @(
    # D1: GOG v1.4a and Definitive Collection v1.5 share the same descent.hog/pig
    @{ From = "D1 v1.4a"; To = "D1 v1.5"; Files = @("descent.hog", "descent.pig") },
    # D2: GOG v1.2 and retail v1.1 CDs share the same descent2.ham/hog
    @{ From = "D2 v1.2"; To = "D2 v1.1"; Files = @("descent2.ham", "descent2.hog") },
    # D2 Mac demo carries the same sound bank hash as D2 v1.2
    @{ From = "D2 v1.2"; To = "D2 Demo (Mac)"; Files = @("descent2.s11") }
)
foreach ($alias in $aliases) {
    foreach ($af in $alias.Files) {
        $match = $newEntries + @($existing) | Where-Object { $_.file -eq $af -and $_.version -eq $alias.From } | Select-Object -First 1
        if ($match) {
            $check = $newEntries + @($existing) | Where-Object { $_.file -eq $af -and $_.version -eq $alias.To }
            if (-not $check) {
                $newEntries += [PSCustomObject]@{ file = $af; sha256 = $match.sha256; version = $alias.To }
                $added++
                Write-Host "  + $af ($($alias.To)) [alias of $($alias.From)]"
            }
        }
    }
}

# Merge and write
if ($newEntries.Count -gt 0) {
    $merged = @($existing) + @($newEntries)
    Write-Json5Versions $Json5File $merged
    Write-Host "`nWrote $($merged.Count) entries to known_versions.json5"
} else {
    Write-Host "`nNo new entries to add"
}

# Report
Write-Host "`n=== Summary ==="
Write-Host "  Files scanned: $($allFiles.Count)"
Write-Host "  New entries:   $added"
Write-Host "  Skipped:       $skipped"
