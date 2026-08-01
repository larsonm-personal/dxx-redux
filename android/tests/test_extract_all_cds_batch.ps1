#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$sourceScript = Join-Path $repoRoot 'game_data\extract_all_cds.ps1'
$tempRoot = Join-Path $repoRoot 'android\temp\extract_all_cds_batch_test'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

try {
    $gameDataDir = Join-Path $tempRoot 'game_data'
    $helpersDir = Join-Path $tempRoot 'android\helpers'
    $buildDir = Join-Path $tempRoot 'android\tests\build\Release'
    $discDir = Join-Path $gameDataDir 'CD images\single-track'
    New-Item -ItemType Directory -Path $helpersDir, $buildDir, $discDir -Force | Out-Null
    Copy-Item -LiteralPath $sourceScript -Destination (Join-Path $gameDataDir 'extract_all_cds.ps1')
    [System.IO.File]::WriteAllText(
        (Join-Path $buildDir 'fixture-extract-cd'),
        'tool',
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        (Join-Path $discDir 'single.iso'),
        'fixture',
        [System.Text.UTF8Encoding]::new($false)
    )
    $testEnvironment = @'
function Join-RegressionPath {
    param([Parameter(ValueFromRemainingArguments)][string[]]$Segments)
    return [System.IO.Path]::Combine($Segments)
}
function Reset-RegressionCMakeBuildIfMissingTool { return $false }
function Resolve-RegressionBuildTool {
    param([string]$Directory)
    return (Join-Path $Directory 'fixture-extract-cd')
}
'@
    [System.IO.File]::WriteAllText(
        (Join-Path $helpersDir 'test_env.ps1'),
        $testEnvironment,
        [System.Text.UTF8Encoding]::new($false)
    )

    $boundedExtraction = @'
Set-StrictMode -Version Latest
function Invoke-BoundedExtractor {
    param([string]$OutputDirectory, [string]$FilePath, [string[]]$ArgumentList)
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path (Split-Path $OutputDirectory -Parent) 'selected_source.txt'),
        $ArgumentList[0]
    )
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'fixture.bin'), 'fixture')
    return [pscustomobject]@{
        Output = @('{"track":1}', 'Done: 1 data tracks extracted, 1 total files, 0 errors')
        ExitCode = 0
    }
}
function Get-ExtractionPathIdentity {
    param([string]$Path, [string]$Name)
    return [pscustomobject]@{ name = $Name; sha256 = 'fixture' }
}
function New-ExtractionProvenance {
    param([string]$Policy, [object[]]$Sources, [object[]]$Tools)
    return [pscustomobject]@{ policy = $Policy; sources = $Sources; tools = $Tools }
}
function Resolve-DiscExtractionSource {
    param([string]$Directory)
    $sources = @(Get-ChildItem -LiteralPath $Directory -File | Where-Object { $_.Extension -in '.cue', '.iso' })
    if ($sources.Count -ne 1) { throw "Expected exactly one CUE or ISO descriptor in $Directory, found $($sources.Count)" }
    return [pscustomobject]@{ Primary = $sources[0]; Files = @($sources[0]) }
}
function Test-ExtractionCompletionManifest { param([string]$Directory, [object]$ExpectedProvenance); return $false }
function Write-ExtractionCompletionManifest {
    param([string]$Directory, [object]$Provenance)
    Set-Content -LiteralPath (Join-Path $Directory '.extraction-complete.json') -Value '{}' -NoNewline
}
function Publish-ExtractionDirectory {
    param([string]$StagingDirectory, [string]$DestinationDirectory)
    Move-Item -LiteralPath $StagingDirectory -Destination $DestinationDirectory
}
'@
    [System.IO.File]::WriteAllText(
        (Join-Path $helpersDir 'bounded_extraction.ps1'),
        $boundedExtraction,
        [System.Text.UTF8Encoding]::new($false)
    )

    $scriptPath = Join-Path $gameDataDir 'extract_all_cds.ps1'
    $powerShellPath = (Get-Process -Id $PID).Path
    $firstOutput = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -SkipBuild 2>&1)
    Assert-True ($LASTEXITCODE -eq 0) "Single-track extraction failed: $($firstOutput -join "`n")"
    Assert-True (($firstOutput -join "`n") -match 'Saved 1 track hashes') 'Single-track result should retain array Count behavior'
    $selectedSource = Get-Content -LiteralPath (Join-Path $discDir 'selected_source.txt') -Raw
    Assert-True ($selectedSource.EndsWith('single.iso')) 'CD extraction should use the sole descriptor'

    $missingSourceDir = Join-Path $gameDataDir 'CD images\missing-source'
    New-Item -ItemType Directory -Path $missingSourceDir -Force | Out-Null
    $failureOutput = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -SkipBuild -Force 2>&1)
    Assert-True ($LASTEXITCODE -eq 1) 'A missing disc source should make the batch fail'
    $failureText = $failureOutput -join "`n"
    Assert-True ($failureText -match 'Expected exactly one CUE or ISO descriptor') `
        'Failure report should explain the unambiguous descriptor requirement'
    Assert-True ($failureText -notmatch "property 'Details'") 'Failure report should not require optional Details'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'extract_all_cds batch tests passed' -ForegroundColor Green
exit 0
