# extract_all_cds.ps1 -- Build extract_cd.exe and run it on all CD image folders.
#
# For each subfolder in game_data/CD images/:
#   1. Find the .cue file
#   2. Run extract_cd.exe to extract data track files and compute track SHA-1s
#   3. Save track hashes to <folder>/track_hashes.json
#
# Idempotent: skips folders with existing data_tracks/ unless -Force.
# Reports errors for non-BIN/CUE formats (e.g., CloneCD .ccd/.img).
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
. "$RepoRoot\android\test_env.ps1"

$SrcDir     = Join-Path $RepoRoot "android\app\src\main\cpp\extract"
$BuildDir   = Join-Path $RepoRoot "android\tests\build"
$CdImgDir   = Join-Path $ScriptDir "CD images"
$ExeName    = "extract_cd.exe"
$ExePath    = Join-Path $BuildDir "Release\$ExeName"

# -- Build ------------------------------------------------------------

if (-not $SkipBuild) {
    if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
        Write-Host "Configuring cmake..."
        cmake -S $SrcDir -B $BuildDir
    }
    Write-Host "Building extract_cd..."
    cmake --build $BuildDir --config Release --target extract_cd
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Write-Host "Build OK: $ExePath"
}

if (-not (Test-Path $ExePath)) {
    Write-Error "extract_cd.exe not found at $ExePath. Run without -SkipBuild"
    exit 1
}

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

    # Find .cue file
    $cueFiles = Get-ChildItem -Path $folder.FullName -Filter "*.cue" -File
    if ($cueFiles.Count -eq 0) {
        $failures += @{ Name = $name; Error = "No .cue file found" }
        continue
    }
    $cueFile = $cueFiles[0].FullName

    Write-Host "`n=== $name ===" -ForegroundColor Cyan
    Write-Host "  CUE: $($cueFiles[0].Name)"

    # Run extract_cd.exe
    $outDir = $dataTracksDir
    try {
        # Run with $ErrorActionPreference relaxed so stderr doesn't throw
        $prevEAP = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $output = & $ExePath $cueFile $outDir 2>&1
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $prevEAP

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
            # Check if the failure is an ISO listing error (e.g. Mac HFS disc)
            $isoFailed = $jsonLines | Where-Object { $_ -match '"error":\s*"ISO listing failed"' }
            if ($isoFailed) {
                Write-Host "  ISO listing failed -- trying Mac (HFS) extraction..." -ForegroundColor Yellow
                $macScript = Join-Path $ScriptDir "extract_mac_cd.ps1"
                if (Test-Path $macScript) {
                    try {
                        & $macScript -CdFolder $folder.FullName -Force
                        if ($LASTEXITCODE -eq 0 -and (Test-Path $dataTracksDir) -and
                            (Get-ChildItem $dataTracksDir -File).Count -gt 0) {
                            Write-Host "  Mac extraction OK" -ForegroundColor Green
                            $successes += $name
                        } else {
                            $failures += @{ Name = $name; Error = "Mac extraction produced no files" }
                        }
                    } catch {
                        $failures += @{ Name = $name; Error = "Mac extraction failed: $($_.Exception.Message)" }
                    }
                } else {
                    $failures += @{ Name = $name; Error = "extract_cd returned $exitCode (ISO failed, no Mac extractor found)"; Details = ($stderrLines -join "`n") }
                }
            } else {
                $failures += @{ Name = $name; Error = "extract_cd returned $exitCode"; Details = ($stderrLines -join "`n") }
            }
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
