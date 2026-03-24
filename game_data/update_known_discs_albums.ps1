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

# ── Ensure fingerprint_match.exe is available ───────────────────────
# Uses the C tool (XOR-popcount with offset alignment) for proper matching.
# String prefix comparison does NOT work across different audio encodings.

$buildDir = Join-Path $repoRoot "android/tests/build"
$matchExe = Join-Path $buildDir "Release/fingerprint_match.exe"

if (-not (Test-Path $matchExe)) {
    Write-Host "Building fingerprint_match.exe..."
    . "$repoRoot\android\test_env.ps1"
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
    Push-Location $buildDir
    & cmake "$repoRoot/android/app/src/main/cpp/extract" -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
    & cmake --build . --config Release --target fingerprint_match -- /p:ErrorLimit=10 2>&1 | Out-Null
    Pop-Location
    if (-not (Test-Path $matchExe)) {
        Write-Error "Failed to build fingerprint_match.exe"
    }
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

# ── Build flat JSON for fingerprint_match.exe ──────────────────────
# Combine CD fingerprints + album fingerprints into a single JSON array

$cdDiscIds = @{}
foreach ($cd in $cdFingerprints) { $cdDiscIds[$cd.DiscId] = $true }

$flatEntries = @()
foreach ($cd in $cdFingerprints) {
    $flatEntries += [ordered]@{
        name        = if ($cd.TrackName) { $cd.TrackName } else { "" }
        disc_id     = $cd.DiscId
        track       = $cd.TrackNum
        duration_ms = $cd.DurationMs
        chromaprint = $cd.Chromaprint
    }
}

# Load album tracks and collect them for the flat JSON + later output
$albumInfos = @()
foreach ($file in $albumFiles) {
    $raw = Get-Content $file.FullName -Raw
    $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
    $info = $stripped | ConvertFrom-Json
    $albumName = $info.album
    $albumId = ($albumName.ToLower() -replace '[^a-z0-9]+', '-').Trim('-')

    $trackNum = 1
    $tracksList = @()
    foreach ($t in $info.tracks) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($t.filename)
        $flatEntries += [ordered]@{
            name        = $baseName
            disc_id     = $albumId
            track       = $trackNum
            duration_ms = $t.duration_ms
            chromaprint = $t.chromaprint
        }
        $tracksList += [PSCustomObject]@{
            TrackNum    = $trackNum
            Filename    = $t.filename
            BaseName    = $baseName
            Chromaprint = $t.chromaprint
            DurationMs  = $t.duration_ms
            AcoustidName = $t.acoustid_name
        }
        $trackNum++
    }
    $albumInfos += [PSCustomObject]@{
        Id     = $albumId
        Label  = $albumName
        Tracks = $tracksList
    }
}

# Write temp flat JSON (no BOM -- the C parser doesn't handle BOM)
$tempJson = Join-Path $repoRoot "temp/dedup_fingerprints.json"
$jsonText = $flatEntries | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText($tempJson, $jsonText, [System.Text.UTF8Encoding]::new($false))
Write-Host "Wrote $($flatEntries.Count) entries to temp JSON for matching"

# ── Run fingerprint_match.exe ──────────────────────────────────────

Write-Host "Running fingerprint_match.exe (threshold $matchThreshold)..."
$matchStderrFile = Join-Path $repoRoot "temp/dedup_match_stderr.txt"
$matchStdoutFile = Join-Path $repoRoot "temp/dedup_match_stdout.json"
$proc = Start-Process -FilePath $matchExe -ArgumentList $tempJson, $matchThreshold `
    -RedirectStandardOutput $matchStdoutFile -RedirectStandardError $matchStderrFile `
    -NoNewWindow -Wait -PassThru
if (Test-Path $matchStderrFile) {
    Get-Content $matchStderrFile | ForEach-Object { Write-Host $_ }
}
if ($proc.ExitCode -ne 0) {
    Write-Error "fingerprint_match.exe exited with code $($proc.ExitCode)"
}
$matchStdout = Get-Content $matchStdoutFile -Raw

$allPairs = $matchStdout | ConvertFrom-Json

# Filter to CD-vs-Album pairs and build lookup: "albumId|trackNum" -> best CD match
$dupLookup = @{}
foreach ($pair in $allPairs) {
    $aIsCd = $cdDiscIds.ContainsKey($pair.a_disc)
    $bIsCd = $cdDiscIds.ContainsKey($pair.b_disc)
    # We want exactly one side to be a CD and the other an album
    if ($aIsCd -eq $bIsCd) { continue }
    if ($aIsCd) {
        $key = "$($pair.b_disc)|$($pair.b_track)"
        $source = "$($pair.a_disc) track $($pair.a_track) ($($pair.a_name))"
        $score = $pair.score
    } else {
        $key = "$($pair.a_disc)|$($pair.a_track)"
        $source = "$($pair.b_disc) track $($pair.b_track) ($($pair.b_name))"
        $score = $pair.score
    }
    # Keep the best match for each album track
    if (-not $dupLookup.ContainsKey($key) -or $dupLookup[$key].Score -lt $score) {
        $dupLookup[$key] = [PSCustomObject]@{ Source = $source; Score = $score }
    }
}
Write-Host "Found $($allPairs.Count) total pairs, $($dupLookup.Count) album tracks matching CD tracks"

# ── Build album entries with duplicate info ─────────────────────────

$albumEntries = @()
$totalTracks = 0
$totalDuplicates = 0

foreach ($albumInfo in $albumInfos) {
    $tracks = @()
    $albumDuplicates = @()

    foreach ($t in $albumInfo.Tracks) {
        $totalTracks++
        $key = "$($albumInfo.Id)|$($t.TrackNum)"
        $isDuplicate = $dupLookup.ContainsKey($key)
        $dupSource = if ($isDuplicate) { $dupLookup[$key].Source } else { $null }

        $trackEntry = [ordered]@{
            track       = $t.TrackNum
            type        = "audio"
            name        = $t.BaseName
            chromaprint = $t.Chromaprint
            duration_ms = $t.DurationMs
        }
        if ($t.AcoustidName) {
            $trackEntry["acoustid_name"] = $t.AcoustidName
        }

        if ($isDuplicate) {
            $totalDuplicates++
            $albumDuplicates += [PSCustomObject]@{
                TrackNum = $t.TrackNum
                Filename = $t.Filename
                Source   = $dupSource
            }
        }

        $tracks += [PSCustomObject]@{
            Entry       = $trackEntry
            IsDuplicate = $isDuplicate
            DupSource   = $dupSource
        }
    }

    $albumEntries += [PSCustomObject]@{
        Id         = $albumInfo.Id
        Label      = $albumInfo.Label
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

    $activeTracks = @($album.Tracks | Where-Object { -not $_.IsDuplicate })
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
        $active = @($album.Tracks | Where-Object { -not $_.IsDuplicate }).Count
        $dupes = $album.Duplicates.Count
        Write-Host "  $($album.Label): $active tracks ($dupes duplicates removed)"
    }
} else {
    $output -join "`n" | Set-Content $dbPath -Encoding UTF8 -NoNewline
    Write-Host "`nWrote $($output.Count) lines to $dbPath"
    Write-Host "Albums added: $($albumEntries.Count)"
    foreach ($album in $albumEntries) {
        $active = @($album.Tracks | Where-Object { -not $_.IsDuplicate }).Count
        $dupes = $album.Duplicates.Count
        Write-Host "  $($album.Label): $active tracks ($dupes duplicates removed)"
    }
}
