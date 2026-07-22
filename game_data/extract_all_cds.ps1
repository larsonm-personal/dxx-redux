#!/usr/bin/env pwsh
# extract_all_cds.ps1 -- Build extract_cd.exe and run it on all CD image folders.
#
# For each subfolder in game_data/CD images/:
#   1. Find the .cue or .iso source file
#   2. Run extract_cd.exe to extract ISO 9660 or Mac HFS data track files and
#      compute track SHA-1s
#   3. Save track hashes to <folder>/track_hashes.json
#
# Idempotent: skips folders with existing data_tracks/ unless -Force.
# Reports errors for non-BIN/CUE formats (e.g., CloneCD .ccd/.img).
# The legacy game_data/extract_mac_cd.ps1 path remains available only for
# regression oracle creation.
#
# Usage: .\extract_all_cds.ps1 [-Force] [-SkipBuild]
param(
    [switch]$Force,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir  = $PSScriptRoot
$RepoRoot   = Split-Path $ScriptDir

# Resolve cmake and other tool paths
. "$RepoRoot\android\helpers\test_env.ps1"
. "$RepoRoot\android\helpers\bounded_extraction.ps1"

$SrcDir     = Join-RegressionPath $RepoRoot "android" "app" "src" "main" "cpp" "extract"
$BuildDir   = Join-RegressionPath $RepoRoot "android" "tests" "build"
$CdImgDir   = Join-Path $ScriptDir "CD images"

function Resolve-ExtractCdTool {
    foreach ($dir in @(
            (Join-RegressionPath $BuildDir "Release"),
            $BuildDir
        )) {
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
            continue
        }
        $tool = Resolve-RegressionBuildTool -Directory $dir -BaseName "extract_cd"
        if ($tool) {
            return $tool
        }
    }
    return $null
}

# -- Build ------------------------------------------------------------

if (-not $SkipBuild) {
    $null = Reset-RegressionCMakeBuildIfMissingTool -BuildDir $BuildDir
    if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
        Write-Host "Configuring cmake..."
        cmake -S $SrcDir -B $BuildDir
    }
    Write-Host "Building extract_cd..."
    cmake --build $BuildDir --config Release --target extract_cd
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

$ExePath = Resolve-ExtractCdTool
if (-not $ExePath) {
    Write-Error "extract_cd not found under $BuildDir. Run without -SkipBuild"
    exit 1
}
Write-Host "Using extract_cd: $ExePath"

# -- Process CD images ------------------------------------------------

if (-not (Test-Path $CdImgDir)) {
    Write-Error "CD images directory not found: $CdImgDir"
    exit 1
}

$folders = Get-ChildItem -Path $CdImgDir -Directory | Sort-Object Name
$successes = @()
$failures = @()
$skipped = @()

foreach ($folder in $folders) {
    $name = $folder.Name
    $dataTracksDir = Join-Path $folder.FullName "data_tracks"
    $hashFile = Join-Path $folder.FullName "track_hashes.json"

    # Skip if already processed
    if ((Test-Path $dataTracksDir) -and (Test-Path $hashFile) -and -not $Force) {
        $skipped += $name
        continue
    }

    # Clean re-extract when -Force
    if ($Force) {
        if (Test-Path $dataTracksDir) { Remove-Item -Recurse -Force -Confirm:$false $dataTracksDir }
        if (Test-Path $hashFile) { Remove-Item -Force -Confirm:$false $hashFile }
    }

    # Find .cue or .iso source file
    $cueFiles = Get-ChildItem -Path $folder.FullName -Filter "*.cue" -File
    $isoFiles = Get-ChildItem -Path $folder.FullName -Filter "*.iso" -File
    if ($cueFiles.Count -gt 0) {
        $sourceFile = $cueFiles[0].FullName
        $sourceLabel = "CUE"
        $sourceName = $cueFiles[0].Name
    }
    elseif ($isoFiles.Count -gt 0) {
        $sourceFile = $isoFiles[0].FullName
        $sourceLabel = "ISO"
        $sourceName = $isoFiles[0].Name
    }
    else {
        $failures += @{ Name = $name; Error = "No .cue or .iso file found" }
        continue
    }

    Write-Host "`n=== $name ===" -ForegroundColor Cyan
    Write-Host "  ${sourceLabel}: $sourceName"

    # Run extract_cd.exe
    $outDir = $dataTracksDir
    try {
        $bounded = Invoke-BoundedExtractor -OutputDirectory $outDir -FilePath $ExePath `
            -ArgumentList @($sourceFile, $outDir)
        $output = $bounded.Output
        $exitCode = $bounded.ExitCode

        # Separate stdout (JSON lines) from stderr (progress)
        $jsonLines = @()
        $stderrLines = @()
        foreach ($line in $output) {
            $s = "$line"
            if ($s.StartsWith("{")) {
                $jsonLines += $s
            } else {
                $stderrLines += $s
                Write-Host "  $s"
            }
        }

        # Save track hashes
        if ($jsonLines.Count -gt 0) {
            $jsonLines = $jsonLines | ForEach-Object { $_ -replace "`r", "" }
            "[`n  " + ($jsonLines -join ",`n  ") + "`n]" | Set-Content -NoNewline $hashFile -Encoding UTF8
            Write-Host "  Saved $($jsonLines.Count) track hashes to track_hashes.json"
        }

        if ($exitCode -ne 0) {
            $failures += @{ Name = $name; Error = "extract_cd returned $exitCode"; Details = ($stderrLines -join "`n") }
        } else {
            $successes += $name
        }
    } catch {
        $failures += @{ Name = $name; Error = $_.Exception.Message }
    }
}

# -- Report -----------------------------------------------------------

Write-Host "`n" -NoNewline
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Extraction Report" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Successful: $($successes.Count)"
Write-Host "  Failed:     $($failures.Count)"
Write-Host "  Skipped:    $($skipped.Count) (already processed)"

if ($skipped.Count -gt 0) {
    Write-Host "`n  Skipped:" -ForegroundColor Yellow
    foreach ($s in $skipped) { Write-Host "    $s" }
}

if ($failures.Count -gt 0) {
    Write-Host "`n  Failures:" -ForegroundColor Red
    foreach ($f in $failures) {
        Write-Host "    $($f.Name): $($f.Error)" -ForegroundColor Red
        if ($f.Details) {
            foreach ($d in ($f.Details -split "`n")) {
                Write-Host "      $d" -ForegroundColor DarkRed
            }
        }
    }
}

if ($successes.Count -gt 0) {
    Write-Host "`n  Successful:" -ForegroundColor Green
    foreach ($s in $successes) { Write-Host "    $s" }
}
