#!/usr/bin/env pwsh
# update_known_discs_albums.ps1 -- Merge per-album chromaprint_info.json5 files
# into the standalone known_albums.json5 fingerprint database.
#
# Reads all game_data/music/*/chromaprint_info.json5 files, creates album entries,
# and deduplicates against physical CD tracks using chromaprint similarity.
#
# Physical CD entries are the deduplication authority. Albums are emitted alphabetically.
# Duplicate tracks are commented out with notes about the CD source.

param(
    [switch]$DryRun,       # Show what would change without writing
    [switch]$Force         # Regenerate even if the album database already exists
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
$musicDir = Join-Path $PSScriptRoot "music"
$dbPath = "$repoRoot/android/app/src/main/assets/known_discs.json5"
$albumDbPath = "$repoRoot/android/app/src/main/assets/known_albums.json5"
$configPath = "$repoRoot/android/app/src/main/assets/fingerprint_config.json5"
. "$repoRoot/android/helpers/fingerprint_config.ps1"
. "$repoRoot/android/helpers/acoustid_title_match.ps1"
. "$repoRoot/android/helpers/fingerprint_source_identity.ps1"

if ((Test-Path -LiteralPath $albumDbPath) -and -not $Force -and -not $DryRun) {
    Write-Host "Album database already exists at $albumDbPath. Use -Force to regenerate"
    exit 0
}

# Load match threshold from fingerprint_config.json5

$fingerprintConfig = Get-DxxFingerprintMatchingConfig -Path $configPath
$matchThreshold = $fingerprintConfig.MatchThreshold
$matchThresholdArgument = $matchThreshold.ToString('R', [Globalization.CultureInfo]::InvariantCulture)
Write-Host "Match threshold: $matchThreshold"

# Ensure fingerprint_match.exe is available
# Uses the C tool (XOR-popcount with offset alignment) for proper matching.
# String prefix comparison does NOT work across different audio encodings.

$buildDir = Join-Path $repoRoot "android/tests/build"
$matchExe = Join-Path $buildDir "Release/fingerprint_match.exe"

if (-not (Test-Path $matchExe)) {
    Write-Host "Building fingerprint_match.exe..."
    . "$repoRoot\android\helpers\test_env.ps1"
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
    & cmake -S "$repoRoot/android/app/src/main/cpp/extract" -B $buildDir -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
    & cmake --build $buildDir --config Release --target fingerprint_match -- /p:ErrorLimit=10 2>&1 | Out-Null
    if (-not (Test-Path $matchExe)) {
        Write-Error "Failed to build fingerprint_match.exe"
    }
}

# Load existing known_discs.json5

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

# Discover album .json5 files

$albumFiles = Get-ChildItem "$musicDir/*/chromaprint_info.json5" -ErrorAction SilentlyContinue
if ($albumFiles.Count -eq 0) {
    Write-Host "No album chromaprint_info.json5 files found. Run fingerprint_music_packs.ps1 first"
    exit 0
}
Write-Host "Found $($albumFiles.Count) album info files"

# Sort alphabetically by album name
$albumFiles = $albumFiles | Sort-Object { $_.Directory.Name }

# Build flat JSON for fingerprint_match.exe
# Combine CD fingerprints + album fingerprints into a single JSON array

$cdDiscIds = @{}
foreach ($cd in $cdFingerprints) { $cdDiscIds[$cd.DiscId] = $true }

$physicalSources = @($db.discs | Where-Object { $_.type -ne 'album' } | ForEach-Object {
    [PSCustomObject]@{ Id = [string]$_.id; Label = [string]$_.label }
})
$albumSources = @($albumFiles | ForEach-Object {
    $sourceRaw = Get-Content $_.FullName -Raw
    $sourceInfo = ($sourceRaw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', '') | ConvertFrom-Json
    [PSCustomObject]@{
        Id = ConvertTo-DxxFingerprintSourceId -Name ([string]$sourceInfo.album)
        Label = [string]$sourceInfo.album
    }
})
Assert-DxxUniqueFingerprintSourceIds -Sources $albumSources -ReservedSources $physicalSources

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
    $albumId = ConvertTo-DxxFingerprintSourceId -Name $albumName

    $trackNum = 1
    $tracksList = @()
    foreach ($t in $info.tracks) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($t.filename)
        # New lookups carry score and recording ID, but older checked-in cache
        # entries predate those fields. Keep a fingerprint-stable legacy result
        # when its label still matches the maintained filename.
        $hasReviewedAcoustId = $t.acoustid_name -and
            $t.name_source -ne 'tracklist' -and
            (Test-DxxAcoustIdTitleMatch $t.filename $t.acoustid_name)
        $flatEntries += [ordered]@{
            name        = $baseName
            disc_id     = $albumId
            track       = $trackNum
            duration_ms = $t.duration_ms
            chromaprint = $t.chromaprint
        }
        $tracksList += [PSCustomObject]@{
            TrackNum     = $trackNum
            Filename     = $t.filename
            BaseName     = $baseName
            Chromaprint  = $t.chromaprint
            DurationMs   = $t.duration_ms
            AcoustidName = if ($hasReviewedAcoustId) { $t.acoustid_name } else { $null }
            AcoustidAlbum = if ($hasReviewedAcoustId) { $t.acoustid_album } else { $null }
            AcoustidScore = if ($hasReviewedAcoustId) { $t.acoustid_score } else { $null }
            AcoustidRecordingId = if ($hasReviewedAcoustId) { $t.acoustid_recording_id } else { $null }
            TracklistName = $t.tracklist_name
            NameSource   = $t.name_source
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

# Run fingerprint_match.exe

Write-Host "Running fingerprint_match.exe (threshold $matchThreshold)..."
$matchStderrFile = Join-Path $repoRoot "temp/dedup_match_stderr.txt"
$matchStdoutFile = Join-Path $repoRoot "temp/dedup_match_stdout.json"
$proc = Start-Process -FilePath $matchExe -ArgumentList $tempJson, $matchThresholdArgument `
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

# Compare encoded fingerprints directly as well. This catches exact collisions
# even when the diagnostic matcher cannot load an unusually large fixture set.
$exactFingerprints = @{}
foreach ($albumInfo in $albumInfos) {
    foreach ($track in $albumInfo.Tracks) {
        $candidate = [PSCustomObject]@{
            Disc = $albumInfo.Id
            Track = $track.TrackNum
            Name = $track.BaseName
            DurationMs = $track.DurationMs
        }
        $key = [string]$track.Chromaprint
        if ($exactFingerprints.ContainsKey($key)) {
            foreach ($prior in $exactFingerprints[$key]) {
                $allPairs += [PSCustomObject]@{
                    a_disc = $prior.Disc
                    a_track = $prior.Track
                    a_name = $prior.Name
                    a_duration_ms = $prior.DurationMs
                    b_disc = $candidate.Disc
                    b_track = $candidate.Track
                    b_name = $candidate.Name
                    b_duration_ms = $candidate.DurationMs
                    score = 1.0
                }
            }
            $exactFingerprints[$key] += $candidate
        } else {
            $exactFingerprints[$key] = @($candidate)
        }
    }
}

# CD priority hierarchy for stable match selection
# When an album track matches multiple CDs, pick the highest-priority CD.
# Tier 0: base game discs, Mac discs, Definitive Collection, Vertigo/Infinite Abyss
# Tier 1: all other CDs (alphabetically)
# Within the same tier, lower alphabetical ID wins.

$tier0Ids = @(
    "descent-usa",
    "descent-europe",
    "descent-mac-macplay",
    "d1-mac-2nd-bincue",
    "descent-ii-usa",
    "descent-ii-usa-v11",
    "descent-ii-europe",
    "descent-ii-europe-v11",
    "d2-gog-v1.2",
    "d2-mac",
    "descent-i-and-ii-the-definitive-collection-usa-disc-1",
    "descent-i-and-ii-the-definitive-collection-usa-disc-2",
    "descent-i-and-ii-the-definitive-collection-usa-disc-3",
    "descent-i-and-ii-the-definitive-collection-europe-disc-1",
    "descent-i-and-ii-the-definitive-collection-europe-disc-2",
    "descent-i-and-ii-the-definitive-collection-europe-disc-3",
    "descent-ii-the-vertigo-series-usa"
)
$tier0Set = @{}
foreach ($id in $tier0Ids) { $tier0Set[$id] = $true }

function Compare-DiscMatch($a, $b) {
    # Returns $true if $a is a better match than $b.
    # Tier 0 always beats tier 1. Within the same tier, higher score wins.
    # Same tier + same score: lower alphabetical disc ID wins (stable tiebreak).
    if ($a.Tier -ne $b.Tier) { return $a.Tier -lt $b.Tier }
    if ($a.Score -ne $b.Score) { return $a.Score -gt $b.Score }
    return $a.DiscId -lt $b.DiscId
}

# Filter to CD-vs-Album pairs and build lookup: "albumId|trackNum" -> best CD match
# "Best" = highest-tier CD first, then highest score within that tier.
$dupLookup = @{}
$ambiguousLookup = @{}
foreach ($pair in $allPairs) {
    $aIsCd = $cdDiscIds.ContainsKey($pair.a_disc)
    $bIsCd = $cdDiscIds.ContainsKey($pair.b_disc)
    if (-not $aIsCd -and -not $bIsCd) {
        $sameIdentity = $pair.a_disc -eq $pair.b_disc -and
            $pair.a_track -eq $pair.b_track -and $pair.a_name -eq $pair.b_name
        $durationRatio = [double]$pair.a_duration_ms / [double]$pair.b_duration_ms
        $durationCompatible = $durationRatio -ge (1.0 - $fingerprintConfig.DurationTolerance) -and
            $durationRatio -le (1.0 + $fingerprintConfig.DurationTolerance)
        $indistinguishableFingerprint = [double]$pair.score -ge (1.0 - 1.0e-6)
        if ($sameIdentity -or -not $durationCompatible -or -not $indistinguishableFingerprint) { continue }
        $aKey = "$($pair.a_disc)|$($pair.a_track)"
        $bKey = "$($pair.b_disc)|$($pair.b_track)"
        $aIdentity = "$($pair.a_disc) track $($pair.a_track) ($($pair.a_name))"
        $bIdentity = "$($pair.b_disc) track $($pair.b_track) ($($pair.b_name))"
        $ambiguousLookup[$aKey] = "$bIdentity at score $($pair.score)"
        $ambiguousLookup[$bKey] = "$aIdentity at score $($pair.score)"
        continue
    }
    # Physical-disc pairs are already maintained outside this album generator
    if ($aIsCd -and $bIsCd) { continue }
    if ($aIsCd) {
        $key = "$($pair.b_disc)|$($pair.b_track)"
        $cdDisc = $pair.a_disc
        $cdTrack = $pair.a_track
        $cdName = $pair.a_name
    } else {
        $key = "$($pair.a_disc)|$($pair.a_track)"
        $cdDisc = $pair.b_disc
        $cdTrack = $pair.b_track
        $cdName = $pair.b_name
    }
    $tier = if ($tier0Set.ContainsKey($cdDisc)) { 0 } else { 1 }
    $candidate = [PSCustomObject]@{
        Source = "$cdDisc track $cdTrack ($cdName)"
        Score  = $pair.score
        Tier   = $tier
        DiscId = $cdDisc
    }
    if (-not $dupLookup.ContainsKey($key) -or (Compare-DiscMatch $candidate $dupLookup[$key])) {
        $dupLookup[$key] = $candidate
    }
}
Write-Host "Found $($allPairs.Count) total pairs, $($dupLookup.Count) CD duplicates, $($ambiguousLookup.Count) ambiguous album tracks"

# Build album entries with duplicate info

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
        $isAmbiguous = $ambiguousLookup.ContainsKey($key)
        $ambiguousSource = if ($isAmbiguous) { $ambiguousLookup[$key] } else { $null }

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
        if ($t.AcoustidAlbum) {
            $trackEntry["acoustid_album"] = $t.AcoustidAlbum
        }
        if ($t.AcoustidScore) {
            $trackEntry["acoustid_score"] = $t.AcoustidScore
        }
        if ($t.AcoustidRecordingId) {
            $trackEntry["acoustid_recording_id"] = $t.AcoustidRecordingId
        }
        if ($t.TracklistName) {
            $trackEntry["tracklist_name"] = $t.TracklistName
        }
        if ($t.NameSource) {
            $trackEntry["name_source"] = $t.NameSource
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
            IsAmbiguous = $isAmbiguous
            AmbiguousSource = $ambiguousSource
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

# Generate the album-only database. Physical-disc records remain maintained in
# known_discs.json5 and are never rewritten by this generator.
$output = @(
    "// Generated fingerprint albums from game_data/music/*/chromaprint_info.json5"
    "// Physical-disc hashes are maintained separately in known_discs.json5"
    "{"
    "  `"albums`": ["
)

for ($ai = 0; $ai -lt $albumEntries.Count; $ai++) {
    $album = $albumEntries[$ai]
    $output += "    // -- $($album.Label)"

    # Note any duplicates in comments
    foreach ($dup in $album.Duplicates) {
        $output += "    // duplicate: track $($dup.TrackNum) ($($dup.Filename)) matches $($dup.Source)"
    }
    foreach ($track in @($album.Tracks | Where-Object IsAmbiguous)) {
        $output += "    // ambiguous: track $($track.Entry.track) ($($track.Entry.name)) conflicts with $($track.AmbiguousSource)"
    }

    $output += "    {"
    $output += "      `"id`": `"$($album.Id)`","
    $output += "      `"label`": `"$($album.Label)`","
    $output += "      `"tracks`": ["

    $activeTracks = @($album.Tracks | Where-Object { -not $_.IsDuplicate -and -not $_.IsAmbiguous })
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
        if ($e.acoustid_album) {
            $albumEscaped = $e.acoustid_album -replace '"', '\"'
            $line += ", `"acoustid_album`": `"$albumEscaped`""
        }
        if ($e.acoustid_score) {
            $line += ", `"acoustid_score`": $($e.acoustid_score)"
        }
        if ($e.acoustid_recording_id) {
            $recordingIdEscaped = $e.acoustid_recording_id -replace '"', '\"'
            $line += ", `"acoustid_recording_id`": `"$recordingIdEscaped`""
        }
        if ($e.tracklist_name) {
            $tracklistEscaped = $e.tracklist_name -replace '"', '\"'
            $line += ", `"tracklist_name`": `"$tracklistEscaped`""
        }
        if ($e.name_source) {
            $sourceEscaped = $e.name_source -replace '"', '\"'
            $line += ", `"name_source`": `"$sourceEscaped`""
        }
        $line += "}$comma"
        $output += $line
    }

    $output += "      ]"
    $trailingComma = if ($ai -lt $albumEntries.Count - 1) { "," } else { "" }
    $output += "    }$trailingComma"
}
$output += "  ]"
$output += "}"

if ($DryRun) {
    Write-Host "`n--- DRY RUN: would write $($output.Count) lines to $albumDbPath ---"
    Write-Host "Albums to add: $($albumEntries.Count)"
    foreach ($album in $albumEntries) {
        $active = @($album.Tracks | Where-Object { -not $_.IsDuplicate -and -not $_.IsAmbiguous }).Count
        $dupes = $album.Duplicates.Count
        Write-Host "  $($album.Label): $active tracks ($dupes duplicates removed)"
    }
} else {
    $output -join "`n" | Set-Content $albumDbPath -Encoding UTF8
    Write-Host "`nWrote $($output.Count) lines to $albumDbPath"
    Write-Host "Albums added: $($albumEntries.Count)"
    foreach ($album in $albumEntries) {
        $active = @($album.Tracks | Where-Object { -not $_.IsDuplicate -and -not $_.IsAmbiguous }).Count
        $dupes = $album.Duplicates.Count
        Write-Host "  $($album.Label): $active tracks ($dupes duplicates removed)"
    }
}
