#!/usr/bin/env pwsh
# extract_all_cds.ps1 -- Build extract_cd.exe and run it on all CD image folders.
#
# For each subfolder in game_data/CD images/:
#   1. Require one unambiguous .iso or .cue source set and hash all of its files
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
    [switch]$SkipBuild,
    [string]$SpecListPath
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
if ($SpecListPath) {
    $selectedSpecs = @(Get-Content -LiteralPath $SpecListPath | ForEach-Object { [IO.Path]::GetFullPath($_) })
    $folders = @($folders | Where-Object {
            $selectedSpecs -contains [IO.Path]::GetFullPath((Join-Path $_.FullName 'extract_regression.json5'))
        })
}
$successes = @()
$failures = @()
$skipped = @()

foreach ($folder in $folders) {
    $name = $folder.Name
    $dataTracksDir = Join-Path $folder.FullName "data_tracks"
    $hashFile = Join-Path $folder.FullName "track_hashes.json"
    $boundHashFile = Join-Path $dataTracksDir ".track_hashes.json"

    try {
        $source = Resolve-DiscExtractionSource -Directory $folder.FullName
        $sourceFile = $source.Primary.FullName
        $sourceName = $source.Primary.Name
        $sourceLabel = $source.Primary.Extension.TrimStart('.').ToUpperInvariant()
        $sourceIdentities = @($source.Files | ForEach-Object {
                Get-ExtractionPathIdentity -Path $_.FullName -Name $_.Name
            })
        $provenance = New-ExtractionProvenance -Policy 'extract-all-cds-v1' `
            -Sources $sourceIdentities -Tools @(
                (Get-ExtractionPathIdentity -Path $ExePath -Name 'extract_cd'),
                (Get-ExtractionPathIdentity -Path $PSCommandPath -Name 'extract_all_cds.ps1')
            )
    } catch {
        $failures += @{ Name = $name; Error = $_.Exception.Message }
        continue
    }

    if ((Test-ExtractionCompletionManifest -Directory $dataTracksDir -ExpectedProvenance $provenance) -and
        (Test-Path -LiteralPath $boundHashFile -PathType Leaf) -and -not $Force) {
        if (-not (Test-Path -LiteralPath $hashFile -PathType Leaf) -or
            (Get-FileHash -LiteralPath $hashFile -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $boundHashFile -Algorithm SHA256).Hash) {
            Copy-Item -LiteralPath $boundHashFile -Destination $hashFile -Force
        }
        $skipped += $name
        continue
    }

    Write-Host "`n=== $name ===" -ForegroundColor Cyan
    Write-Host "  ${sourceLabel}: $sourceName"

    # Run extract_cd.exe
    $outDir = Join-Path $folder.FullName ".data_tracks-$([Guid]::NewGuid().ToString('N'))"
    try {
        New-Item -ItemType Directory -Path $outDir | Out-Null
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

        $jsonProblem = $null
        foreach ($line in $jsonLines) {
            try {
                $record = $line | ConvertFrom-Json -ErrorAction Stop
            } catch {
                $jsonProblem = "extract_cd emitted invalid JSON: $line"
                break
            }
            if ($null -ne $record.PSObject.Properties['error'] -and $record.error) {
                $jsonProblem = "extract_cd reported an error: $($record.error)"
                break
            }
            $filesystemProperty = $record.PSObject.Properties['filesystem']
            if ($null -ne $filesystemProperty -and $filesystemProperty.Value -eq 'hfs' -and
                ($null -eq $record.PSObject.Properties['files_extracted'] -or $record.files_extracted -le 0)) {
                $jsonProblem = 'extract_cd reported HFS success without a positive extracted-file count'
                break
            }
        }

        $stagedFiles = @(Get-ChildItem -LiteralPath $outDir -File -Recurse)
        if ($exitCode -ne 0 -or $jsonLines.Count -eq 0 -or $stagedFiles.Count -eq 0 -or $jsonProblem) {
            $errorMessage = if ($jsonProblem) { $jsonProblem } else { "extract_cd returned $exitCode" }
            $failures += @{ Name = $name; Error = $errorMessage; Details = ($stderrLines -join "`n") }
        } else {
            $jsonLines = @($jsonLines | ForEach-Object { $_ -replace "`r", "" })
            "[`n  " + ($jsonLines -join ",`n  ") + "`n]" |
                Set-Content -NoNewline -LiteralPath (Join-Path $outDir ".track_hashes.json") -Encoding UTF8
            Write-ExtractionCompletionManifest -Directory $outDir -Provenance $provenance
            Publish-ExtractionDirectory -StagingDirectory $outDir -DestinationDirectory $dataTracksDir
            $hashTemp = Join-Path $folder.FullName ".track_hashes-$([Guid]::NewGuid().ToString('N')).json"
            try {
                Copy-Item -LiteralPath (Join-Path $dataTracksDir ".track_hashes.json") -Destination $hashTemp
                Move-Item -LiteralPath $hashTemp -Destination $hashFile -Force
            } finally {
                Remove-Item -LiteralPath $hashTemp -Force -ErrorAction SilentlyContinue
            }
            Write-Host "  Saved $($jsonLines.Count) track hashes to track_hashes.json"
            $successes += $name
        }
    } catch {
        $failures += @{ Name = $name; Error = $_.Exception.Message }
    } finally {
        if (Test-Path -LiteralPath $outDir) {
            Remove-Item -LiteralPath $outDir -Recurse -Force -ErrorAction SilentlyContinue
        }
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
        if ($f.ContainsKey('Details') -and $f.Details) {
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

if ($failures.Count -gt 0) {
    exit 1
}
