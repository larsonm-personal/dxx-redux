#!/usr/bin/env pwsh

param(
    [ValidateRange(0.000001, 1.0)][double]$SampleFraction = 1.0,
    [ValidateRange(0, [int]::MaxValue)][int]$SampleSeed = 0,
    [string]$SampleStatePath,
    [switch]$MissingOnly
)

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
$hostBatch = Join-Path $scriptDir "regenerate_all_mission_metadata_host.ps1"
. (Join-Path $scriptDir 'runtime_targeted_sampling.ps1')
. (Join-Path $scriptDir 'cd_level_metadata_sources.ps1')
. (Join-Path $scriptDir 'mission_archive_sources.ps1')

function Write-Status {
    param([string]$Message, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Message" -ForegroundColor $Color
}

$archiveSources = @(Get-AvailableMissionArchiveSources -Sources (Get-MissionArchiveSources -RepoRoot $repoRoot))
if (-not (Test-Path -LiteralPath $gradle -PathType Leaf)) {
    throw "Gradle wrapper not found: $gradle"
}
if (-not (Test-Path -LiteralPath $batch -PathType Leaf)) {
    throw "Mission metadata batch runner not found: $batch"
}
if (-not $MissingOnly -and -not (Test-Path -LiteralPath $hostBatch -PathType Leaf)) {
    throw "Host mission metadata batch runner not found: $hostBatch"
}

$archiveItemsBySource = @{}
foreach ($source in $archiveSources) {
    $archiveItemsBySource[$source.Id] = if ($MissingOnly) {
        @(Get-MissingMissionMetadataArchives -Source $source)
    } else {
        @(Get-MissionArchives -Source $source)
    }
}
$eligibleArchiveCount = @($archiveItemsBySource.Values | ForEach-Object { $_ }).Count
if ($MissingOnly -and $eligibleArchiveCount -eq 0) {
    Write-Status "All mission archives already have regression JSON files" "Green"
    exit 0
}

if (Test-Path -LiteralPath $jdkHome -PathType Container) {
    $env:JAVA_HOME = $jdkHome
    $env:Path = "$env:JAVA_HOME\bin;$env:Path"
    Write-Status "Using JAVA_HOME=$env:JAVA_HOME"
} else {
    Write-Status "JDK 21 not found at $jdkHome, using current Java environment" "Yellow"
}

Write-Status "Building debug APK"
& $gradle -p $androidRoot assembleDebug
if ($LASTEXITCODE -ne 0) {
    throw "Debug APK build failed with exit code $LASTEXITCODE"
}

Write-Status "Regenerating mission metadata JSON"
Write-Status "Output: $outDir"
$selectedArchivesBySource = @{}
$selectedCdSourceIds = $null
if ($SampleFraction -lt 1.0) {
    if ($SampleSeed -eq 0) { $SampleSeed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
    $zipItemCount = 0
    foreach ($source in $archiveSources) {
        $zipItems = @($archiveItemsBySource[$source.Id])
        $selectedArchivesBySource[$source.Id] = @(Select-RuntimeHashRingFractionItems -Items $zipItems -Fraction $SampleFraction `
                -Seed ($SampleSeed -bxor 401) -StatePath $SampleStatePath `
                -RingName "regenerate:metadata:archives:$($source.Id)" | Select-Object -ExpandProperty Name)
        $zipItemCount += $zipItems.Count
    }
    $cdItems = @()
    if (-not $MissingOnly) {
        $cdManifest = Join-Path $zipDir 'cd_level_metadata_sources.jsonc'
        $cdItems = @(Resolve-CdLevelMetadataSources -RepoRoot $repoRoot -ManifestPath $cdManifest -OutputDir $zipDir |
                ForEach-Object { [pscustomobject]@{ Name = $_.Id } })
        $selectedCdSourceIds = @(Select-RuntimeHashRingFractionItems -Items $cdItems -Fraction $SampleFraction `
                -Seed ($SampleSeed -bxor 402) -StatePath $SampleStatePath `
                -RingName 'regenerate:metadata:cd-sources' | Select-Object -ExpandProperty Name)
    }
    $selectedZipCount = @($selectedArchivesBySource.Values | ForEach-Object { $_ }).Count
    $sampleDescription = if ($MissingOnly) {
        "missing archives {0}/{1} ({2:P1}), seed {3}" -f
        $selectedZipCount, $zipItemCount, $SampleFraction, $SampleSeed
    } else {
        "archives {0}/{1}, CD sources {2}/{3} ({4:P1}), seed {5}" -f
        $selectedZipCount, $zipItemCount, $selectedCdSourceIds.Count, $cdItems.Count,
        $SampleFraction, $SampleSeed
    }
    Write-Status "Metadata sample: $sampleDescription"
}

$sourceIndex = 0
$archiveBatchExit = 0
foreach ($source in $archiveSources) {
    $patterns = if ($SampleFraction -lt 1.0) {
        @($selectedArchivesBySource[$source.Id])
    } elseif ($MissingOnly) {
        @($archiveItemsBySource[$source.Id] | Select-Object -ExpandProperty Name)
    } else {
        @("*.zip", "*.7z")
    }
    if ($patterns.Count -eq 0) {
        $reason = if ($MissingOnly) { "no missing regression JSON files" } else { "empty metadata sample" }
        Write-Status "Skipping $($source.Directory): $reason" "Yellow"
        continue
    }
    $batchArgs = @{
        MetadataOnly = $true
        Install = ($sourceIndex -eq 0)
        ZipDir = $source.Directory
        OutDir = Join-Path $outDir $source.Id
        Pattern = $patterns
        TimeoutSeconds = 900
    }
    Write-Status "Mission archive source: $($source.Directory)"
    & $batch @batchArgs
    $batchExit = $LASTEXITCODE
    if ($batchExit -ne 0) {
        Write-Status "Mission metadata regeneration failed with exit code $batchExit" "Red"
        if ($archiveBatchExit -eq 0) { $archiveBatchExit = $batchExit }
    }
    $sourceIndex++
}

if (-not $MissingOnly) {
    Write-Status "Regenerating CD mission metadata JSON"
    $hostArgs = @{ CdSourcesOnly = $true }
    if ($null -ne $selectedCdSourceIds) { $hostArgs.CdSourceIds = $selectedCdSourceIds }
    & $hostBatch @hostArgs
    $cdBatchExit = $LASTEXITCODE
    if ($cdBatchExit -ne 0) {
        Write-Status "CD mission metadata regeneration failed with exit code $cdBatchExit" "Red"
        if ($archiveBatchExit -eq 0) { $archiveBatchExit = $cdBatchExit }
    }
}

if ($archiveBatchExit -ne 0) {
    Write-Status "Mission metadata regeneration completed with failures" "Red"
    exit $archiveBatchExit
}

Write-Status "Mission metadata regeneration complete" "Green"
Write-Status "Output: $outDir" "Green"
