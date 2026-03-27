# extract_all_gog.ps1 -- Build extract_gog.exe and run it on all GOG installers.
#
# For each .exe/.pkg in game_data/gog installers/:
#   1. Run extract_gog.exe to extract game files
#   2. SHA-256 hash each extracted file
#   3. Cross-reference against known_versions.json5
#
# Idempotent: skips installers with existing extracted/ unless -Force.
#
# Usage: .\extract_all_gog.ps1 [-Force] [-SkipBuild]
param(
    [switch]$Force,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir    = $PSScriptRoot
$RepoRoot     = Split-Path $ScriptDir

# Resolve cmake and other tool paths
. "$RepoRoot\android\test_env.ps1"

$SrcDir       = Join-Path $RepoRoot "android\app\src\main\cpp\extract"
$BuildDir     = Join-Path $RepoRoot "android\tests\build"
$GogDir       = Join-Path $ScriptDir "gog installers"
$ExeName      = "extract_gog.exe"
$ExePath      = Join-Path $BuildDir "Release\$ExeName"
$KnownVerFile = Join-Path $RepoRoot "android\app\src\main\assets\known_versions.json5"

# -- Build ----------------------------------------------------------------

if (-not $SkipBuild) {
    # Kill zombie cl.exe
    Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

    if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
        Write-Host "Configuring cmake..."
        cmake -S $SrcDir -B $BuildDir
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    }
    Write-Host "Building extract_gog..."
    cmake --build $BuildDir --config Release --target extract_gog
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    Write-Host "Build OK: $ExePath"
}

if (-not (Test-Path $ExePath)) {
    Write-Error "extract_gog.exe not found at $ExePath. Run without -SkipBuild"
    exit 1
}

# -- Load known_versions.json5 ------------------------------------------------

function Load-KnownVersions {
    param([string]$Path)
    $text = Get-Content $Path -Raw
    # Strip // comments (not inside strings -- good enough for this format)
    $text = $text -replace '(?m)^\s*//.*$', ''
    $text = $text -replace '//[^"]*$', ''
    $json = $text | ConvertFrom-Json
    $lookup = @{}
    foreach ($entry in $json.versions) {
        $key = "$($entry.file)|$($entry.sha256)"
        if (-not $lookup.ContainsKey($key)) {
            $lookup[$key] = @()
        }
        $lookup[$key] += $entry.version
    }
    return $lookup
}

$knownVersions = Load-KnownVersions $KnownVerFile

function Lookup-Version {
    param([string]$FileName, [string]$Sha256)
    $key = "$($FileName.ToLower())|$($Sha256.ToLower())"
    if ($knownVersions.ContainsKey($key)) {
        return $knownVersions[$key] -join ", "
    }
    return $null
}

# -- Process GOG installers ----------------------------------------------------

if (-not (Test-Path $GogDir)) {
    Write-Error "GOG installers directory not found: $GogDir"
    exit 1
}

$installers = Get-ChildItem -Path $GogDir -File | Where-Object {
    $_.Extension -in '.exe', '.pkg'
} | Sort-Object Name

if ($installers.Count -eq 0) {
    Write-Error "No .exe or .pkg files found in $GogDir"
    exit 1
}

$totalExtracted = 0
$totalErrors    = 0
$totalUnknown   = 0
$allResults     = @()

foreach ($installer in $installers) {
    $name = $installer.Name
    $extractDir = Join-Path $installer.DirectoryName "$($installer.BaseName)\extracted"

    # Skip if already processed
    if ((Test-Path $extractDir) -and -not $Force) {
        Write-Host "`n=== $name === (SKIPPED - use -Force to re-extract)" -ForegroundColor Yellow
        # Still hash and report existing files
    } else {
        # Clean re-extract when -Force
        if ($Force -and (Test-Path $extractDir)) {
            Remove-Item -Recurse -Force -Confirm:$false $extractDir
        }

        Write-Host "`n=== $name ===" -ForegroundColor Cyan

        # Pre-create output directory (extract_gog only does one level)
        New-Item -ItemType Directory -Path $extractDir -Force | Out-Null

        # Run extract_gog.exe -- stdout is JSON file listing, stderr is progress
        $prevEAP = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $output = & $ExePath $installer.FullName $extractDir 2>&1
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $prevEAP

        # Show stderr (progress)
        foreach ($line in $output) {
            $s = "$line"
            if (-not $s.StartsWith("[") -and -not $s.StartsWith("  {") -and -not $s.StartsWith("]")) {
                Write-Host "  $s" -ForegroundColor DarkGray
            }
        }

        if ($exitCode -ne 0) {
            Write-Host "  FAILED (exit code $exitCode)" -ForegroundColor Red
            $totalErrors++
            continue
        }
    }

    # Hash and cross-reference extracted files
    if (-not (Test-Path $extractDir)) {
        Write-Host "  No extracted files found" -ForegroundColor Red
        $totalErrors++
        continue
    }

    $files = Get-ChildItem -Path $extractDir -File | Sort-Object Name
    Write-Host "  Extracted $($files.Count) files:" -ForegroundColor Green

    foreach ($f in $files) {
        $hash = (Get-FileHash -Path $f.FullName -Algorithm SHA256).Hash.ToLower()
        $version = Lookup-Version $f.Name $hash
        $sizeKB = [math]::Round($f.Length / 1024, 1)

        $result = [PSCustomObject]@{
            Installer = $name
            File      = $f.Name
            Size      = $f.Length
            SHA256    = $hash
            Version   = if ($version) { $version } else { "UNKNOWN" }
        }
        $allResults += $result

        if ($version) {
            Write-Host ("    {0,-20} {1,10} KB  {2}" -f $f.Name, $sizeKB, $version) -ForegroundColor Green
        } else {
            Write-Host ("    {0,-20} {1,10} KB  UNKNOWN" -f $f.Name, $sizeKB) -ForegroundColor Yellow
            $totalUnknown++
        }
        $totalExtracted++
    }
}

# -- Summary ------------------------------------------------------------------

Write-Host "`n=== Summary ===" -ForegroundColor Cyan
Write-Host "  Installers processed: $($installers.Count)"
Write-Host "  Files extracted:      $totalExtracted"
Write-Host "  Known versions:       $($totalExtracted - $totalUnknown)"
Write-Host "  Unknown files:        $totalUnknown"
Write-Host "  Errors:               $totalErrors"

if ($totalUnknown -gt 0) {
    Write-Host "`n  Unknown files (not in known_versions.json5):" -ForegroundColor Yellow
    $allResults | Where-Object { $_.Version -eq "UNKNOWN" } | ForEach-Object {
        Write-Host ("    {0}: {1}  sha256={2}" -f $_.Installer, $_.File, $_.SHA256) -ForegroundColor Yellow
    }
}

# -- Write detailed results to JSON --------------------------------------------

$outJson = Join-Path $ScriptDir "gog_extraction_results.json"
$allResults | ConvertTo-Json -Depth 5 | Set-Content $outJson -Encoding UTF8
Write-Host "`nResults saved to: $outJson"
