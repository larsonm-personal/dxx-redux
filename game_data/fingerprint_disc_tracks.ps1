# fingerprint_disc_tracks.ps1 -- Build fingerprint_cd.exe and run it on all CD images.
#
# For each subfolder in game_data/CD images/:
#   1. Find the .cue file
#   2. Run fingerprint_cd.exe to compute SHA-1 + Chromaprint for each track
#   3. Save results to <folder>/track_fingerprints.json
#
# Idempotent: skips folders with existing track_fingerprints.json unless -Force.
#
# Usage: .\fingerprint_disc_tracks.ps1 [-Force] [-SkipBuild]
param(
    [switch]$Force,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot  = Split-Path $ScriptDir
$SrcDir    = Join-Path $RepoRoot "android\app\src\main\cpp\extract"
$BuildDir  = Join-Path $RepoRoot "android\tests\build"
$CdImgDir  = Join-Path $ScriptDir "CD images"
$ExeName   = "fingerprint_cd.exe"
$ExePath   = Join-Path $BuildDir "Release\$ExeName"

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

# -- Process CD images ------------------------------------------------

if (-not (Test-Path $CdImgDir)) {
    Write-Error "CD images directory not found: $CdImgDir"
    exit 1
}

$folders   = Get-ChildItem -Path $CdImgDir -Directory | Sort-Object Name
$successes = @()
$failures  = @()
$skipped   = @()

foreach ($folder in $folders) {
    $name = $folder.Name
    $fpFile = Join-Path $folder.FullName "track_fingerprints.json"

    if ((Test-Path $fpFile) -and -not $Force) {
        $skipped += $name
        continue
    }

    # Find .cue file
    $cueFiles = Get-ChildItem -Path $folder.FullName -Filter "*.cue" -File
    if ($cueFiles.Count -eq 0) {
        $failures += @{ Name = $name; Error = "No .cue file found" }
        continue
    }
    $cueFile = $cueFiles[0].FullName

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

    if ($jsonLines.Count -gt 0) {
        "[`n  " + ($jsonLines -join ",`n  ") + "`n]" | Set-Content -NoNewline $fpFile -Encoding UTF8
        Write-Host "  Saved $($jsonLines.Count) track fingerprints" -ForegroundColor Green
        $successes += $name
    } elseif ($exitCode -ne 0) {
        $failures += @{ Name = $name; Error = "fingerprint_cd failed (exit $exitCode)" }
    } else {
        $failures += @{ Name = $name; Error = "No JSON output" }
    }
}

# -- Report -----------------------------------------------------------

Write-Host "`n--- Summary ---"
Write-Host "  OK:      $($successes.Count)"
Write-Host "  Skipped: $($skipped.Count)"
Write-Host "  Failed:  $($failures.Count)"
foreach ($f in $failures) {
    Write-Host "    $($f.Name): $($f.Error)" -ForegroundColor Red
}
