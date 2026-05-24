#!/usr/bin/env pwsh
# fingerprint_music_packs.ps1 -- Extract music archives, fingerprint tracks,
# and look up names on AcoustID.
#
# For each archive in game_data/music/:
#   1. Extracts to game_data/music/<album_name>/ (flattened, no subdirs)
#   2. Fingerprints all mp3/ogg/flac files via fingerprint_audio.exe
#   3. Optionally queries AcoustID for track names
#   4. Writes chromaprint_info.json5 per album
#
# Album name: text before first " - " in the archive filename.
# AcoustID rate limit: 350ms between requests, exponential backoff on errors.

param(
    [switch]$Force,         # Re-extract and re-fingerprint everything
    [switch]$SkipAcoustId,  # Skip AcoustID lookups
    [string]$Album,         # Process only this album (by name)
    [switch]$SkipExtract    # Skip extraction, just fingerprint+lookup
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path

# Resolve cmake and other tool paths
. "$repoRoot\android\test_env.ps1"

$musicDir = Join-Path $PSScriptRoot "music"
$depBase = (Get-Content "$repoRoot/dependency_base.txt" -Raw).Trim()

# ── Parse tool_versions.conf ────────────────────────────────────────

$conf = @{}
Get-Content "$repoRoot/android/get_deps/tool_versions.conf" | ForEach-Object {
    if ($_ -match '^([A-Z_]+)=(.+)$') {
        $conf[$Matches[1]] = $Matches[2]
    }
}

# ── Ensure 7z is available ──────────────────────────────────────────

function Get-7zaPath {
    $dirName = $conf["SEVENZIP_DIR_NAME"]
    if ($dirName) {
        $candidate = Join-Path $depBase "$dirName/7za.exe"
        if (Test-Path $candidate) { return $candidate }
    }
    $onPath = Get-Command 7za -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $onPath = Get-Command 7z -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    # Try to download
    $result = & "$repoRoot/android/get_deps/get_7zip.ps1"
    if ($result -and (Test-Path $result)) { return $result }
    Write-Error "7za.exe not found. Run android/get_deps/get_7zip.ps1 or install 7-Zip"
}

$7za = Get-7zaPath

# ── Ensure fingerprint_audio.exe is built ───────────────────────────

$buildDir = "$repoRoot/android/tests/build"
$fpExe = "$buildDir/Release/fingerprint_audio.exe"

if (-not (Test-Path $fpExe)) {
    Write-Host "Building fingerprint_audio.exe..."
    $srcDir = "$repoRoot/android/app/src/main/cpp/extract"
    if (-not (Test-Path "$buildDir/CMakeCache.txt")) {
        cmake -S $srcDir -B $buildDir 2>&1 | Out-Null
    }
    cmake --build $buildDir --config Release --target fingerprint_audio 2>&1 | ForEach-Object {
        if ($_ -match 'error') { Write-Host $_ }
    }
    if (-not (Test-Path $fpExe)) {
        Write-Error "Failed to build fingerprint_audio.exe"
    }
    Write-Host "Built: $fpExe"
}

# ── Load AcoustID API key ──────────────────────────────────────────

$acoustIdKey = $null
if (-not $SkipAcoustId) {
    $configPath = "$repoRoot/android/acoustid_config.json5"
    if (Test-Path $configPath) {
        $raw = Get-Content $configPath -Raw
        # Strip JSON5 comments
        $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
        try {
            $cfg = $stripped | ConvertFrom-Json
            $acoustIdKey = $cfg.api_key
        } catch {
            Write-Warning "Failed to parse acoustid_config.json5: $_"
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

function Invoke-AcoustIdLookup {
    param(
        [string]$Fingerprint,
        [int]$DurationSec
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
                foreach ($result in $json.results) {
                    if ($result.recordings) {
                        foreach ($rec in $result.recordings) {
                            $title = $rec.title
                            if ($title) {
                                $artists = ""
                                if ($rec.artists) {
                                    $artists = ($rec.artists | ForEach-Object { $_.name }) -join ", "
                                }
                                $trackName = if ($artists) { "$artists - $title" } else { $title }
                                $albumTitle = $null
                                if ($rec.releases) {
                                    $albumTitle = $rec.releases[0].title
                                }
                                return @{ name = $trackName; album = $albumTitle }
                            }
                        }
                    }
                }
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

if ($Album) {
    $archives = $archives | Where-Object {
        $name = $_.BaseName
        if ($name -match '^(.+?) - ') { $name = $Matches[1] }
        $name -eq $Album
    }
    if ($archives.Count -eq 0) {
        Write-Error "No archive found matching album: $Album"
    }
}

Write-Host "Found $($archives.Count) archive(s) to process"

# ── Process each archive ───────────────────────────────────────────

$cleanedAlbumDirs = @{}

foreach ($archive in $archives) {
    $filename = $archive.Name
    # Parse album name: text before first " - "
    if ($filename -match '^(.+?) - ') {
        $albumName = $Matches[1]
    } else {
        $albumName = [System.IO.Path]::GetFileNameWithoutExtension($filename)
    }

    Write-Host "`n=== $albumName ==="
    $albumDir = Join-Path $musicDir $albumName
    $infoFile = Join-Path $albumDir "chromaprint_info.json5"

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
    # Load existing info to avoid re-fingerprinting
    $existingTracks = @{}
    if ((Test-Path $infoFile) -and -not $Force) {
        $raw = Get-Content $infoFile -Raw
        $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
        try {
            $existing = $stripped | ConvertFrom-Json
            foreach ($t in $existing.tracks) {
                $existingTracks[$t.filename] = $t
            }
        } catch {
            Write-Warning "  Failed to parse existing chromaprint_info.json5"
        }
    }

    Write-Host "  Running fingerprint_audio.exe..."
    $fpStdout = Join-Path $albumDir "_fp_stdout.json"
    $fpStderr = Join-Path $albumDir "_fp_stderr.txt"
    $proc = Start-Process -FilePath $fpExe -ArgumentList "`"$albumDir`"" `
        -RedirectStandardOutput $fpStdout -RedirectStandardError $fpStderr `
        -NoNewWindow -Wait -PassThru
    if (Test-Path $fpStderr) {
        Get-Content $fpStderr | ForEach-Object { Write-Host "  $_" }
        Remove-Item $fpStderr -ErrorAction SilentlyContinue
    }
    if ($proc.ExitCode -ne 0 -or -not (Test-Path $fpStdout)) {
        Write-Warning "  fingerprint_audio.exe failed"
        continue
    }
    $fpData = Get-Content $fpStdout -Raw | ConvertFrom-Json
    Remove-Item $fpStdout -ErrorAction SilentlyContinue

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

        # Check if we already have AcoustID result
        $existingEntry = $existingTracks[$fname]
        if ($existingEntry -and $existingEntry.acoustid_name -and -not $Force) {
            $track["acoustid_name"] = $existingEntry.acoustid_name
            if ($existingEntry.acoustid_album) {
                $track["acoustid_album"] = $existingEntry.acoustid_album
            }
            Write-Host "  $fname -> cached: $($existingEntry.acoustid_name)"
        } elseif (-not $SkipAcoustId -and $chromaprint -and $durationMs -gt 0) {
            $durationSec = [math]::Round($durationMs / 1000)
            $result = Invoke-AcoustIdLookup -Fingerprint $chromaprint -DurationSec $durationSec
            if ($result) {
                $track["acoustid_name"] = $result.name
                if ($result.album) {
                    $track["acoustid_album"] = $result.album
                }
                $display = $result.name
                if ($result.album) { $display += " [$($result.album)]" }
                Write-Host "  $fname -> $display"
            } else {
                Write-Host "  $fname -> no AcoustID match"
            }
        } else {
            Write-Host "  $fname -> fingerprinted (no lookup)"
        }

        $tracks += $track
    }

    # ── Write chromaprint_info.json5 ────────────────────────────
    $lines = @()
    $lines += "// chromaprint_info.json5 -- Fingerprint data for album: $albumName"
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
        $lines += "    {`"filename`": `"$fname`", `"chromaprint`": `"$cp`", `"duration_ms`": $dur${extraFields}}$trailing"
    }

    $lines += "  ]"
    $lines += "}"

    $lines -join "`n" | Set-Content $infoFile -Encoding UTF8 -NoNewline
    Write-Host "  Wrote $infoFile"
}

Write-Host "`nAll done"
