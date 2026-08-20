#!/usr/bin/env pwsh
# fingerprint_disc_tracks.ps1 -- Build fingerprint_cd.exe and run it on all CD images.
#
# For each subfolder in game_data/CD images/:
#   1. Find the .cue file
#   2. Run fingerprint_cd.exe to compute SHA-1 + Chromaprint for each track
#   3. Query AcoustID for track/album names (audio tracks only)
#   4. Save results to <folder>/track_fingerprints.json
#
# Idempotent: skips folders with existing track_fingerprints.json unless -Force.
# AcoustID: skips tracks that already have acoustid_name unless -Force.
#
# Usage: .\fingerprint_disc_tracks.ps1 [-Force] [-SkipBuild] [-SkipAcoustId]
param(
    [switch]$Force,
    [switch]$SkipBuild,
    [switch]$SkipAcoustId,
    [string]$CdImageDir,
    [string]$FingerprintExePath,
    [string[]]$FolderNames
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot  = Split-Path $ScriptDir

# Resolve cmake and other tool paths
. "$RepoRoot\android\helpers\test_env.ps1"

$SrcDir    = Join-Path $RepoRoot "android\app\src\main\cpp\extract"
$BuildDir  = Join-Path $RepoRoot "android\tests\build"
$CdImgDir  = if ($CdImageDir) { $CdImageDir } else { Join-Path $ScriptDir "CD images" }
$ExeName   = "fingerprint_cd.exe"
$ExePath   = if ($FingerprintExePath) { $FingerprintExePath } else { Join-Path $BuildDir "Release\$ExeName" }

function Get-CueTrackDefinitions {
    param([Parameter(Mandatory)][string]$Path)

    $tracks = @()
    $seen = [Collections.Generic.HashSet[int]]::new()
    $text = [IO.File]::ReadAllText($Path)
    foreach ($match in [regex]::Matches($text, '(?im)^\s*TRACK\s+([0-9]+)\s+([^\s]+)')) {
        $number = [int]$match.Groups[1].Value
        if (-not $seen.Add($number)) { throw "Duplicate CUE track number: $number" }
        $tracks += [PSCustomObject]@{
            track = $number
            type = if ($match.Groups[2].Value -ieq 'AUDIO') { 'audio' } else { 'data' }
        }
    }
    if ($tracks.Count -eq 0) { throw "CUE contains no tracks: $Path" }
    return $tracks
}

function Assert-DiscFingerprintResults {
    param(
        [Parameter(Mandatory)][array]$ExpectedTracks,
        [Parameter(Mandatory)][array]$Results
    )

    if ($Results.Count -ne $ExpectedTracks.Count) {
        throw "Expected $($ExpectedTracks.Count) fingerprint records, got $($Results.Count)"
    }
    $expected = @{}
    foreach ($track in $ExpectedTracks) { $expected[[int]$track.track] = [string]$track.type }
    $seen = [Collections.Generic.HashSet[int]]::new()
    foreach ($result in $Results) {
        $number = [int]$result.track
        if (-not $expected.ContainsKey($number)) { throw "Unexpected fingerprint track: $number" }
        if (-not $seen.Add($number)) { throw "Duplicate fingerprint track: $number" }
        if ([string]$result.type -cne $expected[$number]) {
            throw "Track $number type mismatch: expected $($expected[$number]), got $($result.type)"
        }
        if ($result.PSObject.Properties['error'] -and -not [string]::IsNullOrWhiteSpace([string]$result.error)) {
            throw "Track $number reported an error: $($result.error)"
        }
        if ([string]$result.sha1 -cnotmatch '^[0-9a-f]{40}$') {
            throw "Track $number has an invalid SHA-1"
        }
        if ($expected[$number] -eq 'audio' -and
            ([string]::IsNullOrWhiteSpace([string]$result.chromaprint) -or [long]$result.duration_ms -le 0)) {
            throw "Track $number has an incomplete audio fingerprint"
        }
    }
}

function Read-ValidatedDiscFingerprintManifest {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][array]$ExpectedTracks
    )

    $raw = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    if (-not $raw.TrimStart().StartsWith('[')) {
        throw 'Fingerprint manifest root must be a JSON array'
    }
    $results = @($raw | ConvertFrom-Json)
    Assert-DiscFingerprintResults -ExpectedTracks $ExpectedTracks -Results $results
    return $results
}

function Write-AtomicFingerprintManifest {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][array]$Results
    )

    $tempPath = Join-Path (Split-Path -Parent $Path) ".$(Split-Path -Leaf $Path).$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        $json = (ConvertTo-Json -InputObject @($Results) -Depth 10) -replace "`r`n", "`n"
        [IO.File]::WriteAllText($tempPath, $json, [Text.UTF8Encoding]::new($false))
        [IO.File]::Move($tempPath, $Path, $true)
    } finally {
        Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
    }
}

# -- Build ------------------------------------------------------------

if (-not $SkipBuild) {
    if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
        Write-Host "Configuring cmake..."
        cmake -S $SrcDir -B $BuildDir
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    }
    Write-Host "Building fingerprint_cd..."
    cmake --build $BuildDir --config Release --target fingerprint_cd
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Write-Host "Build OK: $ExePath"
}

if (-not (Test-Path $ExePath)) {
    Write-Error "fingerprint_cd.exe not found at $ExePath -- run without -SkipBuild"
    exit 1
}

# -- AcoustID setup ---------------------------------------------------

$acoustIdKey = $null
if (-not $SkipAcoustId) {
    $configPath = "$RepoRoot/android/acoustid_config.jsonc"
    if (Test-Path $configPath) {
        $raw = Get-Content $configPath -Raw
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

$script:lastRequestTime = [datetime]::MinValue
$minDelayMs = 350

function Invoke-AcoustIdLookup {
    param([string]$Fingerprint, [int]$DurationSec)

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
                                if ($rec.releases) { $albumTitle = $rec.releases[0].title }
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

# -- Process CD images ------------------------------------------------

if (-not (Test-Path $CdImgDir)) {
    Write-Error "CD images directory not found: $CdImgDir"
    exit 1
}

$folders   = Get-ChildItem -Path $CdImgDir -Directory | Sort-Object Name
if ($FolderNames) { $folders = @($folders | Where-Object Name -in $FolderNames) }
$successes = @()
$failures  = @()
$skipped   = @()

foreach ($folder in $folders) {
    $name = $folder.Name
    $fpFile = Join-Path $folder.FullName "track_fingerprints.json"

    # Find .cue file
    $cueFiles = Get-ChildItem -Path $folder.FullName -Filter "*.cue" -File
    if ($cueFiles.Count -eq 0) {
        $failures += @{ Name = $name; Error = "No .cue file found" }
        continue
    }
    $cueFile = $cueFiles[0].FullName
    try {
        $expectedTracks = @(Get-CueTrackDefinitions -Path $cueFile)
    } catch {
        $failures += @{ Name = $name; Error = $_.Exception.Message }
        continue
    }
    if ((Test-Path $fpFile) -and -not $Force) {
        try {
            $null = Read-ValidatedDiscFingerprintManifest -Path $fpFile -ExpectedTracks $expectedTracks
            $skipped += $name
            continue
        } catch {
            Write-Warning "  Existing manifest is incomplete, reprocessing: $($_.Exception.Message)"
        }
    }

    Write-Host "`n=== $name ===" -ForegroundColor Cyan
    Write-Host "  CUE: $($cueFiles[0].Name)"

    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $output = & $ExePath $cueFile 2>&1
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $prevEAP

    # Separate JSON (stdout) from progress (stderr)
    $jsonLines = @()
    foreach ($line in $output) {
        $s = "$line"
        if ($s.StartsWith("{")) {
            $jsonLines += $s
        } else {
            Write-Host "  $s"
        }
    }

    if ($exitCode -ne 0) {
        $failures += @{ Name = $name; Error = "fingerprint_cd failed (exit $exitCode)" }
        continue
    }
    if ($jsonLines.Count -eq 0) {
        $failures += @{ Name = $name; Error = "No JSON output" }
        continue
    }
    try {
        $results = @($jsonLines | ForEach-Object { ($_ -replace "`r", "") | ConvertFrom-Json })
        Assert-DiscFingerprintResults -ExpectedTracks $expectedTracks -Results $results
        Write-AtomicFingerprintManifest -Path $fpFile -Results $results
        Write-Host "  Saved $($results.Count) track fingerprints" -ForegroundColor Green
        $successes += $name
    } catch {
        $failures += @{ Name = $name; Error = $_.Exception.Message }
    }
}

# -- Report -----------------------------------------------------------

Write-Host "`n--- Fingerprint Summary ---"
Write-Host "  OK:      $($successes.Count)"
Write-Host "  Skipped: $($skipped.Count)"
Write-Host "  Failed:  $($failures.Count)"
foreach ($f in $failures) {
    Write-Host "    $($f.Name): $($f.Error)" -ForegroundColor Red
}

if ($failures.Count -gt 0) { exit 1 }

# -- Phase 2: AcoustID lookup -----------------------------------------

if (-not $SkipAcoustId) {
    Write-Host "`n--- AcoustID Lookups ---"
    $acoustTotal = 0
    $acoustNew = 0
    $acoustSkipped = 0

    foreach ($folder in $folders) {
        $fpFile = Join-Path $folder.FullName "track_fingerprints.json"
        if (-not (Test-Path $fpFile)) { continue }

        $tracks = Get-Content $fpFile -Raw -Encoding UTF8 | ConvertFrom-Json
        $updated = $false

        foreach ($t in $tracks) {
            if ($t.type -ne "audio" -or -not $t.chromaprint -or -not $t.duration_ms) { continue }
            $acoustTotal++

            # Skip if already has acoustid_name (unless -Force)
            if ($t.PSObject.Properties["acoustid_name"] -and $t.acoustid_name -and -not $Force) {
                $acoustSkipped++
                continue
            }

            $durationSec = [math]::Round($t.duration_ms / 1000)
            $result = Invoke-AcoustIdLookup -Fingerprint $t.chromaprint -DurationSec $durationSec
            if ($result) {
                $t | Add-Member -NotePropertyName "acoustid_name" -NotePropertyValue $result.name -Force
                if ($result.album) {
                    $t | Add-Member -NotePropertyName "acoustid_album" -NotePropertyValue $result.album -Force
                }
                $updated = $true
                $acoustNew++
                Write-Host "  [$($folder.Name)] Track $($t.track): $($result.name)" -ForegroundColor Green
            }
        }

        if ($updated) {
            Write-AtomicFingerprintManifest -Path $fpFile -Results @($tracks)
            Write-Host "  Updated $fpFile" -ForegroundColor Yellow
        }
    }

    Write-Host "`n--- AcoustID Summary ---"
    Write-Host "  Audio tracks: $acoustTotal"
    Write-Host "  New lookups:  $acoustNew"
    Write-Host "  Skipped:      $acoustSkipped"
}
