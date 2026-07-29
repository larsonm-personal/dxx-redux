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
function Resolve-RegressionBuildTool { return 'fixture-extract-cd' }
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
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'fixture.bin'), 'fixture')
    return [pscustomobject]@{
        Output = @('{"track":1}', 'Done: 1 data tracks extracted, 1 total files, 0 errors')
        ExitCode = 0
    }
}
function Test-ExtractionCompletionManifest { return $false }
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

    $missingSourceDir = Join-Path $gameDataDir 'CD images\missing-source'
    New-Item -ItemType Directory -Path $missingSourceDir -Force | Out-Null
    $failureOutput = @(& $powerShellPath -NoProfile -NonInteractive -File $scriptPath -SkipBuild -Force 2>&1)
    Assert-True ($LASTEXITCODE -eq 1) 'A missing disc source should make the batch fail'
    $failureText = $failureOutput -join "`n"
    Assert-True ($failureText -match 'No \.cue or \.iso file found') 'Failure report should retain the original diagnostic'
    Assert-True ($failureText -notmatch "property 'Details'") 'Failure report should not require optional Details'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'extract_all_cds batch tests passed' -ForegroundColor Green
exit 0
