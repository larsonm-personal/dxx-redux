#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$sourceScript = Join-Path $repoRoot 'game_data\extract_all_gog.ps1'
$tempRoot = Join-Path $repoRoot ('android/temp/test_extract_all_gog_batch/run_' + [guid]::NewGuid().ToString('N'))

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $tempRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $tempRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

try {
    $gameDataDir = Join-Path $tempRoot 'game_data'
    $gogDir = Join-Path $gameDataDir 'gog installers'
    $helpersDir = Join-Path $tempRoot 'android\helpers'
    $buildDir = Join-Path $tempRoot 'android\tests\build\Release'
    $assetsDir = Join-Path $tempRoot 'android\app\src\main\assets'
    New-Item -ItemType Directory -Path $gogDir, $helpersDir, $buildDir, $assetsDir -Force | Out-Null
    Copy-Item -LiteralPath $sourceScript -Destination (Join-Path $gameDataDir 'extract_all_gog.ps1')
    Set-Content -LiteralPath (Join-Path $gogDir 'same.exe') -Value 'exe' -NoNewline
    Set-Content -LiteralPath (Join-Path $gogDir 'same.pkg') -Value 'pkg' -NoNewline
    Set-Content -LiteralPath (Join-Path $buildDir 'fixture-extract-gog') -Value 'tool' -NoNewline
    Set-Content -LiteralPath (Join-Path $assetsDir 'known_versions.jsonc') `
        -Value '{"versions":[]}' -NoNewline

    @'
function Reset-RegressionCMakeBuildIfMissingTool { return $false }
function Resolve-RegressionBuildTool {
    param([string]$Directory)
    return (Join-Path $Directory 'fixture-extract-gog')
}
'@ | Set-Content -LiteralPath (Join-Path $helpersDir 'test_env.ps1') -NoNewline
    @'
function Publish-ExtractionDirectory {}
function Test-ExtractionCompletionManifest { return $false }
'@ | Set-Content -LiteralPath (Join-Path $helpersDir 'bounded_extraction.ps1') -NoNewline

    $powerShellPath = (Get-Process -Id $PID).Path
    # Windows PowerShell wraps native stderr as errors even for expected failures
    $ErrorActionPreference = 'Continue'
    $output = @(& $powerShellPath -NoProfile -NonInteractive `
            -File (Join-Path $gameDataDir 'extract_all_gog.ps1') -SkipBuild 2>&1)
    $batchExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    Assert-True ($batchExit -eq 1) 'Same-basename EXE and PKG installers should fail preflight'
    Assert-True (($output -join "`n") -match 'ambiguous extensionless basenames.*same\.exe, same\.pkg') `
        'The collision failure should identify both ambiguous installers'

    Remove-Item -LiteralPath (Join-Path $gogDir 'same.pkg')
    @'
function Test-ExtractionCompletionManifest { return $false }
function Get-ExtractionPathIdentity { param($Path, $Name) return @{ name = $Name } }
function New-ExtractionProvenance { param($Policy, $Sources, $Tools) return @{ policy = $Policy } }
function Write-ExtractionCompletionManifest {
    param($Directory, $Provenance)
    Set-Content -LiteralPath (Join-Path $Directory '.extraction-complete.json') -Value '{}'
}
function Invoke-BoundedExtractor {
    param($OutputDirectory, $FilePath, $ArgumentList)
    if ($env:DXX_GOG_FIXTURE_MODE -eq 'nested') {
        $nested = New-Item -ItemType Directory -Path (Join-Path $OutputDirectory '{app}/MISSIONS') -Force
        Set-Content -LiteralPath (Join-Path $nested.FullName 'mission.hog') -Value 'nested mission payload' -NoNewline
    }
    return @{ Output = @('fixture extraction'); ExitCode = $(if ($env:DXX_GOG_FIXTURE_MODE -eq 'failed') { 7 } else { 0 }) }
}
function Publish-ExtractionDirectory {
    param($StagingDirectory, $DestinationDirectory)
    foreach ($file in Get-ChildItem -LiteralPath $StagingDirectory -File -Recurse) {
        $relative = $file.FullName.Substring($StagingDirectory.Length).TrimStart('\', '/')
        $target = Join-Path $DestinationDirectory $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
    }
}
'@ | Set-Content -LiteralPath (Join-Path $helpersDir 'bounded_extraction.ps1') -NoNewline
    $previousFixtureMode = $env:DXX_GOG_FIXTURE_MODE
    try {
        foreach ($mode in @('nested', 'empty', 'failed')) {
            $env:DXX_GOG_FIXTURE_MODE = $mode
            $ErrorActionPreference = 'Continue'
            $output = @(& $powerShellPath -NoProfile -NonInteractive `
                    -File (Join-Path $gameDataDir 'extract_all_gog.ps1') -SkipBuild -Force 2>&1)
            $batchExit = $LASTEXITCODE
            $ErrorActionPreference = 'Stop'
            $expectedExit = if ($mode -eq 'nested') { 0 } else { 1 }
            Assert-True ($batchExit -eq $expectedExit) "$mode extraction returned the wrong batch exit code"
            $resultsText = Get-Content -LiteralPath (Join-Path $gameDataDir 'gog_extraction_results.json') -Raw
            Assert-True ($resultsText.TrimStart().StartsWith('[')) 'Extraction results must always be a JSON array'
            $results = @($resultsText | ConvertFrom-Json)
            if ($mode -eq 'nested') {
                Assert-True ($results.Count -eq 1 -and $results[0].File -eq '{app}/MISSIONS/mission.hog') `
                    'Nested extraction payload must be published and hashed with its relative path'
                Assert-True ($results[0].SHA256 -eq (Get-FileHash -LiteralPath (Join-Path $gogDir 'same/extracted/{app}/MISSIONS/mission.hog')).Hash.ToLowerInvariant()) `
                    'Nested extraction report hash does not match the published payload'
            } else {
                Assert-True (($output -join "`n") -match 'Errors:\s+1') 'Failed extraction must be included in the error summary'
                Assert-True ((Get-Content -LiteralPath (Join-Path $gogDir 'same/extracted/{app}/MISSIONS/mission.hog') -Raw) -eq 'nested mission payload') `
                    'Failed extraction must preserve the previous published payload'
            }
        }
    } finally {
        $env:DXX_GOG_FIXTURE_MODE = $previousFixtureMode
    }
    Write-Host 'extract_all_gog batch tests passed' -ForegroundColor Green
} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0
