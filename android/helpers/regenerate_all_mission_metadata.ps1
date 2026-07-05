#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $androidRoot
$jdkHome = "C:\local\jdk-21"
$zipDir = Join-Path $repoRoot "game_data\mission_files"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outDir = Join-Path $androidRoot "temp\mission_zip_batch\regen_all_metadata_only_$stamp"
$gradle = Join-Path $androidRoot "gradlew.bat"
$batch = Join-Path $scriptDir "run_mission_zip_batch.ps1"

function Write-Status {
    param([string]$Message, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Message" -ForegroundColor $Color
}

Set-Location $repoRoot

if (-not (Test-Path -LiteralPath $zipDir -PathType Container)) {
    throw "Mission metadata source directory not found: $zipDir"
}
if (-not (Test-Path -LiteralPath $gradle -PathType Leaf)) {
    throw "Gradle wrapper not found: $gradle"
}
if (-not (Test-Path -LiteralPath $batch -PathType Leaf)) {
    throw "Mission metadata batch runner not found: $batch"
}

if (Test-Path -LiteralPath $jdkHome -PathType Container) {
    $env:JAVA_HOME = $jdkHome
    $env:Path = "$env:JAVA_HOME\bin;$env:Path"
    Write-Status "Using JAVA_HOME=$env:JAVA_HOME"
} else {
    Write-Status "JDK 21 not found at $jdkHome, using current Java environment" "Yellow"
}

Write-Status "Building debug APK"
Push-Location $androidRoot
try {
    & $gradle assembleDebug
    if ($LASTEXITCODE -ne 0) {
        throw "Debug APK build failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Status "Regenerating mission metadata JSON"
Write-Status "Output: $outDir"
& $batch -MetadataOnly -Install -ZipDir $zipDir -OutDir $outDir -TimeoutSeconds 900
$batchExit = $LASTEXITCODE
if ($batchExit -ne 0) {
    Write-Status "Mission metadata regeneration failed with exit code $batchExit" "Red"
    exit $batchExit
}

Write-Status "Mission metadata regeneration complete" "Green"
Write-Status "Output: $outDir" "Green"

