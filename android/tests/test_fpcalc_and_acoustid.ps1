#!/usr/bin/env pwsh
# test_fpcalc_and_acoustid.ps1 -- Validate our fingerprint pipeline against
# fpcalc (reference chromaprint CLI) and AcoustID (online lookup service).
#
# Tests:
#   1. Compare our fingerprint_audio.exe output vs fpcalc on a real D2 MP3
#   2. Verify AcoustID returns matches for D2 redbook tracks (known music)
#
# Prerequisites:
#   - android/tests/build/Release/fingerprint_audio.exe (cmake build)
#   - fpcalc.exe via android/get_deps/helpers/get_fpcalc.ps1
#   - D2 redbook MP3 files in game_data/music/D2 infinite abyss redbook mp3/
#     or game_data/music/D2 redbook mp3 rips/; skips when only metadata is present
#   - AcoustID API key in android/acoustid_config.json5

param(
    [switch]$SkipAcoustId  # Skip online AcoustID tests
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path (Join-Path (Join-Path $repoRoot "android") "helpers") "test_host_platform.ps1")
Set-Location $repoRoot

$testsPassed = 0
$testsFailed = 0

function Test-Pass($name) {
    $script:testsPassed++
    Write-Host "  PASS: $name" -ForegroundColor Green
}

function Test-Fail($name, $detail) {
    $script:testsFailed++
    Write-Host "  FAIL: $name -- $detail" -ForegroundColor Red
}

# ── Locate tools ────────────────────────────────────────────────────

$fpAudio = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $repoRoot "android" "tests" "build" "Release") -BaseName "fingerprint_audio"
if (-not $fpAudio) {
    $fpAudio = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $repoRoot "android" "tests" "build") -BaseName "fingerprint_audio"
}
if (-not $fpAudio) {
    Write-Error "fingerprint_audio not found. Run: cmake --build android/tests/build --config Release"
}

$depBase = (Get-Content "dependency_base.txt" -First 1).Trim()
$fpcalcExe = $null
$fpcalcDirs = Get-ChildItem (Join-RegressionPath $depBase "fpcalc-*") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
foreach ($fpcalcDir in $fpcalcDirs) {
    $fpcalcExe = Resolve-RegressionBuildTool -Directory $fpcalcDir.FullName -BaseName "fpcalc"
    if ($fpcalcExe) { break }
}
if (-not $fpcalcExe -or -not (Test-Path $fpcalcExe)) {
    Write-Host "Downloading fpcalc..."
    $fpcalcExe = & android/get_deps/helpers/get_fpcalc.ps1
}

# ── Find a D2 redbook track to test with ────────────────────────────

# Prefer "D2 infinite abyss redbook mp3" (known to have AcoustID entries),
# fall back to "D2 redbook mp3 rips" if not available
$redbookDir = $null
$testTrack = $null
foreach ($candidate in @(
        "game_data/music/D2 infinite abyss redbook mp3",
        "game_data/music/D2 redbook mp3 rips"
    )) {
    if (-not (Test-Path $candidate)) { continue }
    $candidateTrack = Get-ChildItem $candidate -Filter "*.mp3" | Select-Object -First 1
    if ($candidateTrack) {
        $redbookDir = $candidate
        $testTrack = $candidateTrack
        break
    }
}

if (-not $testTrack) {
    Write-Warning "No D2 redbook MP3 files found; skipping fpcalc/AcoustID audio test"
    exit 0
}
$testMp3 = $testTrack.FullName
Write-Host "Test track: $($testTrack.Name) (from $redbookDir)"
Write-Host ""

# ── Test 1: fpcalc vs our tool comparison ───────────────────────────

Write-Host "=== Test 1: fpcalc vs fingerprint_audio comparison ==="

# Run fpcalc (raw mode for similarity comparison)
$fpcalcOut = & $fpcalcExe -raw $testMp3 2>&1 | Out-String
$fpcalcDuration = 0
$fpcalcRaw = @()
foreach ($line in ($fpcalcOut -split "`n")) {
    $line = $line.Trim()
    if ($line -match '^DURATION=(\d+)$') { $fpcalcDuration = [int]$Matches[1] }
    if ($line -match '^FINGERPRINT=(.+)$') {
        $fpcalcRaw = $Matches[1] -split ',' | ForEach-Object { [uint32]$_ }
    }
}

# Also run fpcalc in encoded mode for AcoustID test later
$fpcalcEncOut = & $fpcalcExe $testMp3 2>&1 | Out-String
$fpcalcEncoded = ""
foreach ($line in ($fpcalcEncOut -split "`n")) {
    $line = $line.Trim()
    if ($line -match '^FINGERPRINT=(.+)$') { $fpcalcEncoded = $Matches[1] }
}

# Run our tool on the directory, parse JSON output
$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "fpcalc_test_$(Get-Random)"
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
Copy-Item $testMp3 $tempDir
$savedPref = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    $ourJsonRaw = & $fpAudio $tempDir 2>$null | Out-String
} finally {
    $ErrorActionPreference = $savedPref
    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

$ourDuration = 0
$ourEncoded = ""
try {
    # The JSON is on stdout but stderr messages may be mixed in on non-piped output;
    # extract just the JSON part (lines between [ and ])
    $jsonLines = @()
    $inJson = $false
    foreach ($line in ($ourJsonRaw -split "`n")) {
        if ($line.Trim().StartsWith('[')) { $inJson = $true }
        if ($inJson) { $jsonLines += $line }
        if ($line.Trim().StartsWith(']')) { break }
    }
    $jsonText = $jsonLines -join "`n"
    $allEntries = $jsonText | ConvertFrom-Json
    if ($allEntries.Count -gt 0) {
        $entry = $allEntries[0]
        $ourDuration = $entry.duration_ms
        $ourEncoded = $entry.chromaprint
    }
} catch {
    Write-Warning "Failed to parse fingerprint_audio output: $_"
}

Write-Host "  fpcalc: duration=${fpcalcDuration}s, ${fpcalcRaw.Count} raw values"
Write-Host "  ours:   duration=$([math]::Round($ourDuration/1000))s (${ourDuration}ms), encoded length=$($ourEncoded.Length)"

# Duration comparison (within 1 second)
$ourDurationSec = [math]::Round($ourDuration / 1000)
if ([math]::Abs($ourDurationSec - $fpcalcDuration) -le 1) {
    Test-Pass "Duration matches (fpcalc=${fpcalcDuration}s, ours=${ourDurationSec}s)"
} else {
    Test-Fail "Duration mismatch" "fpcalc=${fpcalcDuration}s, ours=${ourDurationSec}s"
}

# Encoded fingerprint similarity -- both use chromaprint default algorithm,
# so the base64 strings should be very close. Use a length comparison as a
# sanity check (exact match is unlikely due to decoder differences)
if ($ourEncoded.Length -gt 0 -and $fpcalcEncoded.Length -gt 0) {
    $lenRatio = [math]::Min($ourEncoded.Length, $fpcalcEncoded.Length) / `
        [math]::Max($ourEncoded.Length, $fpcalcEncoded.Length)
    Write-Host "  Encoded length ratio: $([math]::Round($lenRatio, 3)) (ours=$($ourEncoded.Length), fpcalc=$($fpcalcEncoded.Length))"
    if ($lenRatio -ge 0.90) {
        Test-Pass "Encoded fingerprint length ratio $([math]::Round($lenRatio, 3)) >= 0.90"
    } else {
        Test-Fail "Encoded fingerprint length" "ratio $([math]::Round($lenRatio, 3)) < 0.90"
    }
} else {
    Test-Fail "Fingerprint generation" "ours=$($ourEncoded.Length) chars, fpcalc=$($fpcalcEncoded.Length) chars"
}

# ── Test 2: AcoustID lookup ─────────────────────────────────────────

if (-not $SkipAcoustId) {
    Write-Host ""
    Write-Host "=== Test 2: AcoustID lookup ==="

    # Load API key
    $configPath = "android/acoustid_config.json5"
    $apiKey = $null
    if (Test-Path $configPath) {
        $raw = Get-Content $configPath -Raw
        $cleaned = $raw -replace '//[^\n]*' -replace '/\*[\s\S]*?\*/'
        try {
            $cfg = $cleaned | ConvertFrom-Json
            $apiKey = $cfg.api_key
        } catch {
            Write-Warning "Failed to parse acoustid config: $_"
        }
    }

    if (-not $apiKey) {
        Write-Warning "No AcoustID API key found, skipping lookup test"
    } else {
        # Try up to 5 tracks -- not all tracks are in AcoustID
        $tracks = Get-ChildItem $redbookDir -Filter "*.mp3" | Select-Object -First 5
        $anyMatch = $false

        foreach ($track in $tracks) {
            Write-Host "  Looking up: $($track.Name) via fpcalc fingerprint"

            # Use fpcalc for the lookup (known-good reference implementation)
            $fOut = & $fpcalcExe $track.FullName 2>$null | Out-String
            $tDur = 0; $tFp = ""
            foreach ($line in ($fOut -split "`n")) {
                $line = $line.Trim()
                if ($line -match '^DURATION=(\d+)$') { $tDur = [int]$Matches[1] }
                if ($line -match '^FINGERPRINT=(.+)$') { $tFp = $Matches[1] }
            }
            if (-not $tFp) { Write-Host "    Skipping (no fingerprint)"; continue }

            try {
                Start-Sleep -Milliseconds 400
                $wc = New-Object System.Net.WebClient
                $nvc = New-Object System.Collections.Specialized.NameValueCollection
                $nvc.Add("client", $apiKey)
                $nvc.Add("meta", "recordings releases")
                $nvc.Add("duration", [string]$tDur)
                $nvc.Add("fingerprint", $tFp)
                $respBytes = $wc.UploadValues(
                    "https://api.acoustid.org/v2/lookup", "POST", $nvc)
                $respStr = [System.Text.Encoding]::UTF8.GetString($respBytes)
                $json = $respStr | ConvertFrom-Json

                if ($json.status -eq "ok") {
                    $matchCount = 0; $bestTitle = ""; $bestAlbum = ""
                    foreach ($result in $json.results) {
                        if ($result.recordings) {
                            foreach ($rec in $result.recordings) {
                                $matchCount++
                                if (-not $bestTitle -and $rec.title) {
                                    $a = if ($rec.artists) { ($rec.artists | ForEach-Object { $_.name }) -join ", " } else { "" }
                                    $bestTitle = if ($a) { "$a - $($rec.title)" } else { $rec.title }
                                    if ($rec.releases -and $rec.releases[0].title) {
                                        $bestAlbum = $rec.releases[0].title
                                    }
                                }
                            }
                        }
                    }
                    $display = "$matchCount recording(s)"
                    if ($bestTitle) { $display += ": $bestTitle" }
                    if ($bestAlbum) { $display += " [$bestAlbum]" }
                    Write-Host "    $display"
                    if ($matchCount -gt 0) { $anyMatch = $true; break }
                }
            } catch {
                Write-Warning "    Request failed: $_"
            }
        }

        if ($anyMatch) {
            Test-Pass "AcoustID returned matches for D2 redbook"
        } else {
            Test-Fail "AcoustID lookup" "No matches for any of $($tracks.Count) tested tracks from $redbookDir"
        }
    }
} else {
    Write-Host ""
    Write-Host "=== Skipping AcoustID test (use without -SkipAcoustId to enable) ==="
}

# ── Summary ─────────────────────────────────────────────────────────

Write-Host ""
Write-Host "=== Results: $testsPassed passed, $testsFailed failed ==="
if ($testsFailed -gt 0) {
    exit 1
}
