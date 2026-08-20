#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$sourceScript = Join-Path $repoRoot 'game_data\extract_all_gog.ps1'
$tempRoot = Join-Path $repoRoot 'android\temp\extract_all_gog_batch_test'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}

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
    $output = @(& $powerShellPath -NoProfile -NonInteractive `
            -File (Join-Path $gameDataDir 'extract_all_gog.ps1') -SkipBuild 2>&1)
    Assert-True ($LASTEXITCODE -eq 1) 'Same-basename EXE and PKG installers should fail preflight'
    Assert-True (($output -join "`n") -match 'ambiguous extensionless basenames.*same\.exe, same\.pkg') `
        'The collision failure should identify both ambiguous installers'
    Write-Host 'extract_all_gog batch tests passed' -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0
