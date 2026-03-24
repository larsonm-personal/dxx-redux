# update_known_discs_albums.ps1 -- Merge per-album chromaprint_info.json5 files
# into the overall known_discs.json5 database.
#
# Reads all game_data/music/*/chromaprint_info.json5 files, creates album entries
# in the discs array with type: "album", deduplicates against existing CD tracks
# using chromaprint similarity, and appends to known_discs.json5.
#
# CD entries are primary (unchanged). Album entries are appended alphabetically.
# Duplicate tracks are commented out with notes about the CD source.

param(
    [switch]$DryRun,       # Show what would change without writing
    [switch]$Force          # Regenerate even if album entries already exist
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
$musicDir = Join-Path $PSScriptRoot "music"
$dbPath = "$repoRoot/android/app/src/main/assets/known_discs.json5"
$configPath = "$repoRoot/android/app/src/main/assets/fingerprint_config.json5"

# ── Load match threshold from fingerprint_config.json5 ──────────────

$matchThreshold = 0.4
if (Test-Path $configPath) {
    $raw = Get-Content $configPath -Raw
    $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
    try {
        $cfg = $stripped | ConvertFrom-Json
        if ($cfg.match_threshold) { $matchThreshold = $cfg.match_threshold }
    } catch {
        Write-Warning "Failed to parse fingerprint_config.json5, using default threshold $matchThreshold"
    }
}
Write-Host "Match threshold: $matchThreshold"

# ── Ensure fingerprint matching tool is built ───────────────────────
# We need fingerprint_cd.exe for chromaprint decoding (to compare fingerprints).
# Instead of reimplementing XOR-popcount in PowerShell, we use a simpler approach:
# compare base64 chromaprint strings for exact matches (same source = same fp),
# and use duration as a secondary filter.
#
# For true similarity matching, we'd need the C code. For now, we detect
# exact duplicates (same chromaprint string) and near-duplicates by comparing
# the first 100 chars of the chromaprint + duration within 10%.

function Test-FingerprintDuplicate {
    param(
        [string]$AlbumFp,
        [int]$AlbumDurationMs,
        [string]$CdFp,
        [int]$CdDurationMs
    )
    # Exact match
    if ($AlbumFp -eq $CdFp) { return $true }

    # Duration must be within tolerance
    if ($AlbumDurationMs -le 0 -or $CdDurationMs -le 0) { return $false }
    $ratio = [double]$AlbumDurationMs / [double]$CdDurationMs
    if ($ratio -lt 0.90 -or $ratio -gt 1.10) { return $false }

    # Compare fingerprint prefix (first 100 chars which encode ~25 frames)
    # Same encoding of the same audio produces identical chromaprints
    $prefixLen = [math]::Min(100, [math]::Min($AlbumFp.Length, $CdFp.Length))
    if ($prefixLen -lt 20) { return $false }
    $ap = $AlbumFp.Substring(0, $prefixLen)
    $cp = $CdFp.Substring(0, $prefixLen)
    return $ap -eq $cp
}

# ── Load existing known_discs.json5 ────────────────────────────────

Write-Host "Loading $dbPath"
$dbRaw = Get-Content $dbPath -Raw
$dbStripped = $dbRaw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
$db = $dbStripped | ConvertFrom-Json

# Build lookup of existing CD track fingerprints
$cdFingerprints = @()
foreach ($disc in $db.discs) {
    if ($disc.type -eq "album") { continue }  # skip existing album entries
    foreach ($track in $disc.tracks) {
        if ($track.type -ne "audio") { continue }
        if (-not $track.chromaprint) { continue }
        $cdFingerprints += [PSCustomObject]@{
            DiscId      = $disc.id
            DiscLabel   = $disc.label
            TrackNum    = $track.track
            TrackName   = $track.name
            Chromaprint = $track.chromaprint
            DurationMs  = $track.duration_ms
        }
    }
}
Write-Host "Loaded $($cdFingerprints.Count) CD audio track fingerprints"

# ── Discover album .json5 files ────────────────────────────────────

$albumFiles = Get-ChildItem "$musicDir/*/chromaprint_info.json5" -ErrorAction SilentlyContinue
if ($albumFiles.Count -eq 0) {
    Write-Host "No album chromaprint_info.json5 files found. Run fingerprint_music_packs.ps1 first"
    exit 0
}
Write-Host "Found $($albumFiles.Count) album info files"

# Sort alphabetically by album name
$albumFiles = $albumFiles | Sort-Object { $_.Directory.Name }

# Process albums
$albumEntries = @()
$duplicateNotes = @()
$totalTracks = 0
$totalDuplicates = 0

foreach ($file in $albumFiles) {
    $raw = Get-Content $file.FullName -Raw
    $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
    $info = $stripped | ConvertFrom-Json

    $albumName = $info.album
    # Slugify for ID: lowercase, replace non-alphanum with hyphen, collapse dashes
    $albumId = ($albumName.ToLower() -replace '[^a-z0-9]+', '-').Trim('-')

    $tracks = @()
    $trackNum = 1
    $albumDuplicates = @()

    foreach ($t in $info.tracks) {
        $totalTracks++

        # Check for duplicate against CD tracks
        $isDuplicate = $false
        $dupSource = $null
        foreach ($cd in $cdFingerprints) {
            if (Test-FingerprintDuplicate -AlbumFp $t.chromaprint -AlbumDurationMs $t.duration_ms `
                -CdFp $cd.Chromaprint -CdDurationMs $cd.DurationMs) {
                $isDuplicate = $true
                $dupSource = "$($cd.DiscLabel) track $($cd.TrackNum) ($($cd.TrackName))"
                break
            }
        }

        # Track name: filename without extension
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($t.filename)

        $trackEntry = [ordered]@{
            track       = $trackNum
            type        = "audio"
            name        = $baseName
            chromaprint = $t.chromaprint
            duration_ms = $t.duration_ms
        }
        if ($t.acoustid_name) {
            $trackEntry["acoustid_name"] = $t.acoustid_name
        }

        if ($isDuplicate) {
            $totalDuplicates++
            $albumDuplicates += [PSCustomObject]@{
                TrackNum = $trackNum
                Filename = $t.filename
                Source   = $dupSource
            }
        }

        $tracks += [PSCustomObject]@{
            Entry       = $trackEntry
            IsDuplicate = $isDuplicate
            DupSource   = $dupSource
        }
        $trackNum++
    }

    $albumEntries += [PSCustomObject]@{
        Id         = $albumId
        Label      = $albumName
        Tracks     = $tracks
        Duplicates = $albumDuplicates
    }
}

Write-Host "`nSummary: $($albumEntries.Count) albums, $totalTracks tracks, $totalDuplicates duplicates"

# ── Generate output ─────────────────────────────────────────────────

# Read the original file and find where to insert album entries.
# Strategy: find the last closing ']' of the discs array, insert before the
# final ']}' structure. We need to be careful to preserve existing content.

$lines = Get-Content $dbPath

# Find the line with the closing of the discs array (last "  ]" before "}")
$lastDiscArrayClose = -1
$lastBrace = -1
for ($i = $lines.Count - 1; $i -ge 0; $i--) {
    if ($lines[$i].Trim() -eq "}" -and $lastBrace -eq -1) {
        $lastBrace = $i
    }
    if ($lines[$i].Trim() -eq "]" -and $lastDiscArrayClose -eq -1 -and $lastBrace -ne -1) {
        $lastDiscArrayClose = $i
        break
    }
}

if ($lastDiscArrayClose -eq -1) {
    Write-Error "Could not find discs array closing bracket in known_discs.json5"
}

# Remove any existing album entries (lines between a "// -- Album entries" marker and the array close)
$albumMarkerLine = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '// -- Album entries') {
        $albumMarkerLine = $i
        break
    }
}

if ($albumMarkerLine -ne -1 -and -not $Force) {
    Write-Host "Album entries already exist (line $albumMarkerLine). Use -Force to regenerate"
    if (-not $DryRun) { exit 0 }
}

# Build the prefix: everything up to (but not including) the album marker or the last disc entry's close
if ($albumMarkerLine -ne -1) {
    # Remove old album section: keep lines 0..(albumMarkerLine-1)
    # But we need to re-add the array/object close
    $prefix = $lines[0..($albumMarkerLine - 1)]
    # The line before the album marker should end the last CD entry. Make sure
    # the last CD entry's closing line has a trailing comma for the new entries
} else {
    # No existing album section. Take everything up to and including the last
    # disc entry, then we'll add album entries before the array close.
    # Find the last '}' that closes a disc entry (indented, before the array close)
    $lastDiscClose = -1
    for ($i = $lastDiscArrayClose - 1; $i -ge 0; $i--) {
        if ($lines[$i].Trim() -match '^}') {
            $lastDiscClose = $i
            break
        }
    }
    if ($lastDiscClose -eq -1) {
        Write-Error "Could not find last disc entry closing brace"
    }
    $prefix = $lines[0..$lastDiscClose]
}

# Ensure the last line of prefix (a disc entry close '}') has a trailing comma
$lastPrefixLine = $prefix[$prefix.Count - 1]
if ($lastPrefixLine.Trim() -eq "}") {
    $prefix[$prefix.Count - 1] = $lastPrefixLine.TrimEnd() + ","
}

# Build album entry lines
$albumLines = @()
$albumLines += "    // -- Album entries (from music packs, generated by update_known_discs_albums.ps1) --"

for ($ai = 0; $ai -lt $albumEntries.Count; $ai++) {
    $album = $albumEntries[$ai]
    $albumLines += "    // -- $($album.Label)"

    # Note any duplicates in comments
    foreach ($dup in $album.Duplicates) {
        $albumLines += "    // duplicate: track $($dup.TrackNum) ($($dup.Filename)) matches $($dup.Source)"
    }

    $albumLines += "    {"
    $albumLines += "      `"id`": `"$($album.Id)`","
    $albumLines += "      `"label`": `"$($album.Label)`","
    $albumLines += "      `"type`": `"album`","
    $albumLines += "      `"tracks`": ["

    $activeTracks = $album.Tracks | Where-Object { -not $_.IsDuplicate }
    for ($ti = 0; $ti -lt $activeTracks.Count; $ti++) {
        $t = $activeTracks[$ti]
        $e = $t.Entry
        $comma = if ($ti -lt $activeTracks.Count - 1) { "," } else { "" }

        $nameEscaped = $e.name -replace '"', '\"'
        $line = "        {`"track`": $($e.track), `"type`": `"audio`", `"name`": `"$nameEscaped`""
        $line += ", `"chromaprint`": `"$($e.chromaprint)`""
        $line += ", `"duration_ms`": $($e.duration_ms)"
        if ($e.acoustid_name) {
            $acoustidEscaped = $e.acoustid_name -replace '"', '\"'
            $line += ", `"acoustid_name`": `"$acoustidEscaped`""
        }
        $line += "}$comma"
        $albumLines += $line
    }

    $albumLines += "      ]"
    $trailingComma = if ($ai -lt $albumEntries.Count - 1) { "," } else { "" }
    $albumLines += "    }$trailingComma"
}

# Build the full output
$output = @()
$output += $prefix
$output += $albumLines
$output += "  ]"
$output += "}"

if ($DryRun) {
    Write-Host "`n--- DRY RUN: would write $($output.Count) lines to $dbPath ---"
    Write-Host "Albums to add: $($albumEntries.Count)"
    foreach ($album in $albumEntries) {
        $active = ($album.Tracks | Where-Object { -not $_.IsDuplicate }).Count
        $dupes = $album.Duplicates.Count
        Write-Host "  $($album.Label): $active tracks ($dupes duplicates removed)"
    }
} else {
    $output -join "`n" | Set-Content $dbPath -Encoding UTF8 -NoNewline
    Write-Host "`nWrote $($output.Count) lines to $dbPath"
    Write-Host "Albums added: $($albumEntries.Count)"
    foreach ($album in $albumEntries) {
        $active = ($album.Tracks | Where-Object { -not $_.IsDuplicate }).Count
        $dupes = $album.Duplicates.Count
        Write-Host "  $($album.Label): $active tracks ($dupes duplicates removed)"
    }
}
