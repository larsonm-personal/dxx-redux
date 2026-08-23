#!/usr/bin/env pwsh
# fingerprint_music_packs.ps1 -- Extract music archives, fingerprint tracks,
# and look up names on AcoustID.
#
# For each archive in game_data/music/:
#   1. Extracts to game_data/music/<album_name>/ (flattened, no subdirs)
#   2. Fingerprints all mp3/ogg/flac files via fingerprint_audio.exe
#   3. Optionally queries AcoustID for track names
#   4. Writes chromaprint_info.jsonc per album
#
# Album name: text before first " - " in the archive filename.
# AcoustID rate limit: 350ms between requests, exponential backoff on errors.

param(
    [switch]$Force,         # Re-extract and re-fingerprint everything
    [switch]$SkipAcoustId,  # Skip AcoustID lookups
    [string]$Album,         # Process only this album (by name)
    [string[]]$Albums,
    [switch]$SkipExtract    # Skip extraction, just fingerprint+lookup
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path

# Resolve cmake and other tool paths
. "$repoRoot\android\helpers\test_env.ps1"
. "$repoRoot\android\helpers\fingerprint_audio_results.ps1"
. "$repoRoot\android\helpers\acoustid_title_match.ps1"
. "$repoRoot\android\helpers\atomic_text_file.ps1"
. "$repoRoot\android\helpers\powershell_compat.ps1"
. "$repoRoot\android\helpers\fingerprint_source_identity.ps1"

$musicDir = Join-Path $PSScriptRoot "music"

# ── Ensure 7z is available ──────────────────────────────────────────

function Get-7zaPath {
    $result = & "$repoRoot/android/get_deps/helpers/get_7zip.ps1"
    $candidate = @($result | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Last 1)
    if ($candidate) { return $candidate[0] }
    Write-Error "Verified 7za.exe not found. Run android/get_deps/helpers/get_7zip.ps1"
}

$7za = Get-7zaPath

# ── Ensure fingerprint_audio.exe is built ───────────────────────────

$buildDir = "$repoRoot/android/tests/build"
$fpExe = "$buildDir/Release/fingerprint_audio.exe"

if (-not (Test-Path "$buildDir/CMakeCache.txt")) {
    cmake -S "$repoRoot/android/app/src/main/cpp/extract" -B $buildDir 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "fingerprint_audio CMake configuration failed with exit code $LASTEXITCODE" }
}
Write-Host "Building fingerprint_audio.exe..."
cmake --build $buildDir --config Release --target fingerprint_audio 2>&1 | ForEach-Object {
    if ($_ -match 'error') { Write-Host $_ }
}
if ($LASTEXITCODE -ne 0) { throw "fingerprint_audio build failed with exit code $LASTEXITCODE" }
if (-not (Test-Path $fpExe)) {
    Write-Error "Failed to build fingerprint_audio.exe"
}
Write-Host "Built: $fpExe"

# ── Load AcoustID API key ──────────────────────────────────────────

$acoustIdKey = $null
if (-not $SkipAcoustId) {
    $configPath = "$repoRoot/android/acoustid_config.jsonc"
    if (Test-Path $configPath) {
        $raw = Get-Content $configPath -Raw
        # Strip JSONC comments
        $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
        try {
            $cfg = $stripped | ConvertFrom-Json
            $acoustIdKey = $cfg.api_key
        } catch {
            Write-Warning "Failed to parse acoustid_config.jsonc: $_"
        }
    }
    if (-not $acoustIdKey) {
        Write-Warning "No AcoustID API key found. Skipping lookups"
        $SkipAcoustId = $true
    }
}

# ── AcoustID lookup with rate limiting ──────────────────────────────

$script:lastRequestTime = [datetime]::MinValue
$minDelayMs = 350  # 3 req/s limit with safety margin

function Select-AcoustIdLabel {
    param(
        [object]$Response,
        [string]$MaintainedLabel
    )
    $candidates = @()
    foreach ($result in @($Response.results)) {
        if ([double]$result.score -lt 0.8) { continue }
        foreach ($recording in @($result.recordings)) {
            if (-not $recording.id -or -not $recording.title) { continue }
            $artists = @($recording.artists | ForEach-Object { $_.name } | Where-Object { $_ }) -join ', '
            $name = if ($artists) { "$artists - $($recording.title)" } else { $recording.title }
            if (-not (Test-DxxAcoustIdTitleMatch $MaintainedLabel $name)) { continue }
            $album = @($recording.releases | ForEach-Object { $_.title } | Where-Object { $_ } | Sort-Object)[0]
            $candidates += [PSCustomObject]@{
                name = $name
                album = $album
                score = [double]$result.score
                recording_id = [string]$recording.id
            }
        }
    }
    if ($candidates.Count -eq 0) { return $null }
    $bestScore = ($candidates | Measure-Object -Property score -Maximum).Maximum
    $best = @($candidates | Where-Object { $_.score -eq $bestScore })
    if (@($best | ForEach-Object { $_.name.ToLowerInvariant() } | Select-Object -Unique).Count -ne 1) {
        return $null
    }
    return $best | Sort-Object recording_id | Select-Object -First 1
}

function Invoke-AcoustIdLookup {
    param(
        [string]$Fingerprint,
        [int]$DurationSec,
        [string]$MaintainedLabel
    )

    # Rate limit: wait at least 350ms since last request
    $elapsed = ([datetime]::UtcNow - $script:lastRequestTime).TotalMilliseconds
    if ($elapsed -lt $minDelayMs) {
        Start-Sleep -Milliseconds ([int]($minDelayMs - $elapsed))
    }

    $maxRetries = 3
    $backoffMs = 1000

    for ($attempt = 0; $attempt -le $maxRetries; $attempt++) {
        $script:lastRequestTime = [datetime]::UtcNow
        try {
            $wc = New-Object System.Net.WebClient
            $nvc = New-Object System.Collections.Specialized.NameValueCollection
            $nvc.Add("client", $acoustIdKey)
            $nvc.Add("meta", "recordings releases")
            $nvc.Add("duration", [string]$DurationSec)
            $nvc.Add("fingerprint", $Fingerprint)
            $responseBytes = $wc.UploadValues(
                "https://api.acoustid.org/v2/lookup", "POST", $nvc)
            $responseStr = [System.Text.Encoding]::UTF8.GetString($responseBytes)
            $json = $responseStr | ConvertFrom-Json
            if ($json.status -eq "ok" -and $json.results) {
                return Select-AcoustIdLabel -Response $json -MaintainedLabel $MaintainedLabel
            }
            if ($json.status -eq "error" -and $json.error -and
                $json.error.message -match "rate|limit|too fast") {
                Write-Warning "  AcoustID rate limited, backing off ${backoffMs}ms"
                Start-Sleep -Milliseconds $backoffMs
                $backoffMs *= 2
                continue
            }
            return $null
        } catch {
            $msg = $_.Exception.Message
            if ($msg -match "429|50[0-9]") {
                Write-Warning "  AcoustID HTTP error, backing off ${backoffMs}ms: $msg"
                Start-Sleep -Milliseconds $backoffMs
                $backoffMs *= 2
                continue
            }
            Write-Warning "  AcoustID lookup failed: $msg"
            return $null
        }
    }
    Write-Warning "  AcoustID lookup exhausted retries"
    return $null
}

# ── Discover archives ───────────────────────────────────────────────

$archives = Get-ChildItem $musicDir -File | Where-Object {
    $_.Extension -match '^\.(zip|7z|DXA)$'
}

$requestedAlbums = @(@($Albums) + @($Album) | Where-Object { $_ })
if ($requestedAlbums.Count -gt 0) {
    $archives = $archives | Where-Object {
        $name = $_.BaseName
        if ($name -match '^(.+?) - ') { $name = $Matches[1] }
        $name -in $requestedAlbums
    }
    if ($archives.Count -eq 0) {
        Write-Error "No archive found matching album selection: $($requestedAlbums -join ', ')"
    }
}

$archiveIdentities = @($archives | ForEach-Object {
        $name = $_.BaseName
        if ($name -match '^(.+?) - ') { $name = $Matches[1] }
        [PSCustomObject]@{
            Archive = $_
            AlbumName = $name
            PortableKey = Get-DxxPortableSourceNameKey -Name $name
            SourceId = ConvertTo-DxxFingerprintSourceId -Name $name
        }
    })
$archiveAlbumNames = [Collections.Generic.Dictionary[string, string]]::new(
    [StringComparer]::Ordinal
)
$sourceIdGroups = @{}
foreach ($portableGroup in @($archiveIdentities | Group-Object PortableKey)) {
    $canonicalName = [string]$portableGroup.Group[0].AlbumName
    foreach ($identity in $portableGroup.Group) {
        if ([StringComparer]::Ordinal.Compare([string]$identity.AlbumName, $canonicalName) -lt 0) {
            $canonicalName = [string]$identity.AlbumName
        }
    }
    $sourceId = ConvertTo-DxxFingerprintSourceId -Name $canonicalName
    if ($sourceIdGroups.ContainsKey($sourceId)) {
        throw "Archive album ID collision '$sourceId' between '$($sourceIdGroups[$sourceId])' and '$canonicalName'"
    }
    $sourceIdGroups[$sourceId] = $canonicalName
    foreach ($identity in $portableGroup.Group) {
        $archiveAlbumNames.Add([string]$identity.Archive.FullName, $canonicalName)
    }
}

Write-Host "Found $($archives.Count) archive(s) to process"

# ── Process each archive ───────────────────────────────────────────

$cleanedAlbumDirs = @{}

foreach ($archive in $archives) {
    $filename = $archive.Name
    $albumName = $archiveAlbumNames[[string]$archive.FullName]

    Write-Host "`n=== $albumName ==="
    $albumDir = Join-Path $musicDir $albumName
    $infoFile = Join-Path $albumDir "chromaprint_info.jsonc"

    # ── Extract ─────────────────────────────────────────────────
    if (-not $SkipExtract) {
        $hasFiles = (Test-Path $albumDir) -and
        @(Get-ChildItem $albumDir -File | Where-Object {
                $_.Extension -match '^\.(mp3|ogg|flac)$'
            }).Count -gt 0

        if ($hasFiles -and -not $Force) {
            Write-Host "  Already extracted, skipping"
        } else {
            Write-Host "  Extracting $filename..."
            if (-not (Test-Path $albumDir)) {
                New-Item -ItemType Directory -Path $albumDir -Force | Out-Null
            }

            # On re-extraction, clean old audio files to prevent _2 duplicates.
            # Track cleaned dirs so multi-archive albums (e.g. SC55) only clean once.
            $albumKey = $albumDir.ToLower()
            if ($Force -and -not $cleanedAlbumDirs.ContainsKey($albumKey)) {
                Get-ChildItem $albumDir -File | Where-Object {
                    $_.Extension -match '^\.(mp3|ogg|flac)$'
                } | Remove-Item -Force
                $cleanedAlbumDirs[$albumKey] = $true
            }

            # Extract to a temp dir first, then flatten
            $tempExtract = Join-Path $albumDir "_extract_temp"
            if (Test-Path $tempExtract) {
                Remove-Item $tempExtract -Recurse -Force
            }
            New-Item -ItemType Directory -Path $tempExtract -Force | Out-Null

            $archivePath = $archive.FullName
            $ext = $archive.Extension.ToLower()

            if ($ext -eq ".zip" -or $ext -eq ".dxa") {
                try {
                    Expand-Archive -Path $archivePath -DestinationPath $tempExtract -Force
                } catch {
                    Write-Host "  Expand-Archive failed, trying 7z..."
                    & $7za x "-o$tempExtract" -y $archivePath 2>&1 | Out-Null
                }
            } elseif ($ext -eq ".7z") {
                & $7za x "-o$tempExtract" -y $archivePath 2>&1 | Out-Null
            }

            # Flatten: move all audio files to album dir root
            $audioFiles = Get-ChildItem $tempExtract -Recurse -File | Where-Object {
                $_.Extension -match '^\.(mp3|ogg|flac)$'
            }
            $count = 0
            foreach ($f in $audioFiles) {
                $dest = Join-Path $albumDir $f.Name
                # Handle name collisions by prefixing
                if (Test-Path $dest) {
                    $base = [System.IO.Path]::GetFileNameWithoutExtension($f.Name)
                    $fext = $f.Extension
                    $i = 2
                    do {
                        $dest = Join-Path $albumDir "${base}_${i}${fext}"
                        $i++
                    } while (Test-Path $dest)
                }
                Move-Item $f.FullName $dest
                $count++
            }
            Remove-Item $tempExtract -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "  Extracted $count audio files"
        }
    }

    if (-not (Test-Path $albumDir)) {
        Write-Warning "  Album dir does not exist: $albumDir"
        continue
    }

    # ── Fingerprint ─────────────────────────────────────────────
    # Load existing metadata even during a forced refresh so transient lookup
    # failures cannot erase a previously reviewed result.
    $existingTracks = @{}
    if (Test-Path $infoFile) {
        $raw = Get-Content $infoFile -Raw
        $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
        try {
            $existing = $stripped | ConvertFrom-Json
            foreach ($t in $existing.tracks) {
                $existingTracks[$t.filename] = $t
            }
        } catch {
            Write-Warning "  Failed to parse existing chromaprint_info.jsonc"
        }
    }

    Write-Host "  Running fingerprint_audio.exe..."
    $expectedAudioNames = @(
        Get-ChildItem -LiteralPath $albumDir -File |
            Where-Object { $_.Extension -in @('.mp3', '.ogg', '.flac') } |
            ForEach-Object { $_.Name }
    )
    $fpStdout = Join-Path $albumDir "_fp_stdout.json"
    $fpStderr = Join-Path $albumDir "_fp_stderr.txt"
    $proc = Start-Process -FilePath $fpExe -ArgumentList "`"$albumDir`"" `
        -RedirectStandardOutput $fpStdout -RedirectStandardError $fpStderr `
        -NoNewWindow -Wait -PassThru
    if (Test-Path $fpStderr) {
        Get-Content $fpStderr | ForEach-Object { Write-Host "  $_" }
        Remove-Item $fpStderr -ErrorAction SilentlyContinue
    }
    if ($proc.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $fpStdout -PathType Leaf)) {
        Write-Warning "  fingerprint_audio.exe failed"
        continue
    }
    $fpData = @(ConvertFrom-CompatibleJsonItems -Json (Get-Content -LiteralPath $fpStdout -Raw))
    Remove-Item $fpStdout -ErrorAction SilentlyContinue
    Assert-DxxFingerprintAudioResults -ExpectedNames $expectedAudioNames -Results $fpData

    if ($fpData.Count -eq 0) {
        Write-Warning "  No audio files fingerprinted"
        continue
    }
    Write-Host "  Fingerprinted $($fpData.Count) files"

    # ── AcoustID lookup ─────────────────────────────────────────
    $tracks = @()
    foreach ($fp in $fpData) {
        $fname = $fp.filename
        $chromaprint = $fp.chromaprint
        $durationMs = $fp.duration_ms

        $track = [ordered]@{
            filename    = $fname
            chromaprint = $chromaprint
            duration_ms = $durationMs
        }

        $existingEntry = $existingTracks[$fname]
        # A refresh still applies the maintained-label policy to every new result.
        # Preserve older successful metadata on lookup failure when the audio
        # fingerprint is unchanged, including records created before score and
        # recording ID fields were stored.
        $cachedMetadata = Get-DxxReusableAcoustIdMetadata -Existing $existingEntry `
            -Chromaprint $chromaprint
        if ($cachedMetadata -and -not $Force) {
            foreach ($field in @('acoustid_name', 'acoustid_album', 'acoustid_score', 'acoustid_recording_id')) {
                if ($cachedMetadata.Contains($field)) {
                    $track[$field] = $cachedMetadata[$field]
                }
            }
            Write-Host "  $fname -> cached: $($cachedMetadata.acoustid_name)"
        } elseif (-not $SkipAcoustId -and $chromaprint -and $durationMs -gt 0) {
            $durationSec = [math]::Round($durationMs / 1000)
            $result = Invoke-AcoustIdLookup -Fingerprint $chromaprint -DurationSec $durationSec `
                -MaintainedLabel $fname
            if ($result) {
                $track["acoustid_name"] = $result.name
                $track["acoustid_score"] = $result.score
                $track["acoustid_recording_id"] = $result.recording_id
                if ($result.album) {
                    $track["acoustid_album"] = $result.album
                }
                $display = $result.name
                if ($result.album) { $display += " [$($result.album)]" }
                Write-Host "  $fname -> $display"
            } elseif ($cachedMetadata) {
                foreach ($field in @('acoustid_name', 'acoustid_album', 'acoustid_score', 'acoustid_recording_id')) {
                    if ($cachedMetadata.Contains($field)) {
                        $track[$field] = $cachedMetadata[$field]
                    }
                }
                Write-Host "  $fname -> lookup returned no usable result; preserved cached metadata"
            } else {
                Write-Host "  $fname -> no AcoustID match"
            }
        } elseif ($cachedMetadata) {
            foreach ($field in @('acoustid_name', 'acoustid_album', 'acoustid_score', 'acoustid_recording_id')) {
                if ($cachedMetadata.Contains($field)) {
                    $track[$field] = $cachedMetadata[$field]
                }
            }
            Write-Host "  $fname -> lookup skipped; preserved cached metadata"
        } else {
            Write-Host "  $fname -> fingerprinted (no lookup)"
        }

        $tracks += $track
    }

    # ── Write chromaprint_info.jsonc ────────────────────────────
    $lines = @()
    $lines += "// chromaprint_info.jsonc -- Fingerprint data for album: $albumName"
    $lines += "// Generated by fingerprint_music_packs.ps1"
    $lines += "{"
    $lines += "  `"album`": `"$albumName`","
    $lines += "  `"tracks`": ["

    for ($i = 0; $i -lt $tracks.Count; $i++) {
        $t = $tracks[$i]
        $fname = $t.filename -replace '\\', '\\' -replace '"', '\"'
        $cp = $t.chromaprint
        $dur = $t.duration_ms
        $trailing = if ($i -lt $tracks.Count - 1) { "," } else { "" }

        $extraFields = ""
        if ($t.acoustid_name) {
            $aname = $t.acoustid_name -replace '"', '\"'
            $extraFields += ", `"acoustid_name`": `"$aname`""
        }
        if ($t.acoustid_album) {
            $aalbum = $t.acoustid_album -replace '"', '\"'
            $extraFields += ", `"acoustid_album`": `"$aalbum`""
        }
        if ($t.acoustid_score) {
            $extraFields += ", `"acoustid_score`": $($t.acoustid_score)"
        }
        if ($t.acoustid_recording_id) {
            $recordingId = $t.acoustid_recording_id -replace '"', '\"'
            $extraFields += ", `"acoustid_recording_id`": `"$recordingId`""
        }
        $lines += "    {`"filename`": `"$fname`", `"chromaprint`": `"$cp`", `"duration_ms`": $dur${extraFields}}$trailing"
    }

    $lines += "  ]"
    $lines += "}"

    Write-Utf8NoBomTextAtomically -Path $infoFile -Text ($lines -join "`n")
    Write-Host "  Wrote $infoFile"
}

Write-Host "`nAll done"
