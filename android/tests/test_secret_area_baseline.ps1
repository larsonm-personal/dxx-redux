#!/usr/bin/env pwsh
param(
    [ValidateSet('both', 'd1', 'd2')]
    [string]$Game = 'both',
    [string]$BaselinePath,
    [string]$OutputDir,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$D1Exe,
    [string]$D2Exe,
    [switch]$UpdateBaseline,
    [switch]$BuildBeforeRun,
    [switch]$RequireAssets
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_game_data.ps1')

if (-not $BaselinePath) {
    $BaselinePath = Join-RegressionPath $repoRoot 'android' 'test_fixtures' 'secret_area_base_game_baseline.json'
}
if (-not $OutputDir) {
    $OutputDir = Join-RegressionPath $repoRoot 'android' 'temp' 'secret_area_baseline'
}

function Get-SecretAreaGameConfig {
    param([Parameter(Mandatory)][string]$Name)

    switch ($Name) {
        'd1' {
            return @{
                Name = 'd1'
                RequiredFiles = @('DESCENT.HOG', 'DESCENT.PIG')
                RequiredHashes = @(
                    @{ File = 'DESCENT.HOG'; Sha256 = '83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052' },
                    @{ File = 'DESCENT.PIG'; Sha256 = '093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe' }
                )
                DefaultDataDirs = @(
                    (Join-RegressionPath $repoRoot 'game_data' 'extracted' 'd1 mac extracted'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'data'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp')
                )
            }
        }
        'd2' {
            return @{
                Name = 'd2'
                RequiredFiles = @('DESCENT2.HOG', 'DESCENT2.HAM', 'GROUPA.PIG')
                RequiredHashes = @(
                    @{ File = 'DESCENT2.HOG'; Sha256 = 'f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703' },
                    @{ File = 'DESCENT2.HAM'; Sha256 = '5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d' },
                    @{ File = 'GROUPA.PIG'; Sha256 = 'facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b' }
                )
                DefaultDataDirs = @(
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'data'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp'),
                    (Join-RegressionPath $repoRoot 'game_data' 'extracted' 'descent 2 demo 1-0_extracted')
                )
            }
        }
    }

    throw "Unsupported game: $Name"
}

function Get-SecretAreaDefaultExe {
    param([Parameter(Mandatory)][string]$Name)

    $buildDir = if ($Name -eq 'd1') { 'buildd1' } else { 'buildd2' }
    $baseName = "dxx-redux-$Name-secretareas"
    foreach ($exeName in (Get-RegressionHostExecutableNames -BaseName $baseName)) {
        $candidate = Join-RegressionPath $repoRoot $buildDir 'main' $exeName
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return Join-RegressionPath $repoRoot $buildDir 'main' (Get-RegressionHostExecutableNames -BaseName $baseName)[0]
}

function Invoke-SecretAreaTargetBuild {
    param([Parameter(Mandatory)][string]$Name)

    $buildName = if ($Name -eq 'd1') { 'buildd1' } else { 'buildd2' }
    $buildDir = Join-RegressionPath $repoRoot $buildName
    if (-not (Test-Path -LiteralPath $buildDir -PathType Container)) {
        throw "Build directory not found: $buildDir"
    }
    $cmakePath = Resolve-RegressionCMakePath -RepoRoot $repoRoot -BuildDir $buildDir
    if (-not $cmakePath) {
        throw "cmake not found. Install CMake or place Android SDK CMake under dependency_base.txt"
    }
    $targetName = "dxx-redux-$Name-secretareas"
    if (Test-RegressionWindowsHost) {
        $vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat'
        if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
            throw "vcvarsall.bat not found: $vcvars"
        }
        $cmakeDir = Split-Path -Parent $cmakePath
        $cmd = 'call "' + $vcvars + '" x86 && set "PATH=' + $cmakeDir + ';%PATH%" && "' + $cmakePath + '" --build "' + $buildDir + '" --target "' + $targetName + '"'
        & cmd.exe /c $cmd | Out-Host
    } else {
        & $cmakePath --build $buildDir --target $targetName | Out-Host
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Secret area target build failed for $Name"
    }
}

function Resolve-SecretAreaDataDir {
    param(
        [Parameter(Mandatory)][string]$Name,
        [string]$Requested
    )

    try {
        return Resolve-InputDemoDataDir -RepoRoot $repoRoot -Config (Get-SecretAreaGameConfig -Name $Name) -RequestedDataDir $Requested -Purpose 'secret-area baseline'
    } catch {
        if ($RequireAssets) {
            throw
        }
        Write-Host "Secret-area baseline skipped for ${Name}: $($_.Exception.Message)" -ForegroundColor Yellow
        return $null
    }
}

function Invoke-SecretAreaDump {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)][string]$ResolvedDataDir,
        [Parameter(Mandatory)][string]$OutPath
    )

    if ($BuildBeforeRun) {
        Invoke-SecretAreaTargetBuild -Name $Name
    }
    if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
        if ($RequireAssets) {
            throw "Secret-area executable not found: $Exe"
        }
        Write-Host "Secret-area baseline skipped for ${Name}: executable not found: $Exe" -ForegroundColor Yellow
        return $null
    }

    $targetDir = Split-Path -Parent $OutPath
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    Write-Host "Secret-area baseline: dumping $Name"
    & $Exe -hogdir $ResolvedDataDir -secretarea-json-out $OutPath | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Secret-area dump failed for $Name"
    }
    $dump = Get-Content -LiteralPath $OutPath -Raw | ConvertFrom-Json
    Write-CanonicalJsonFile -Value $dump -Path $OutPath
    return $dump
}

function Write-CanonicalJsonFile {
    param(
        [Parameter(Mandatory)]$Value,
        [Parameter(Mandatory)][string]$Path
    )

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    $json = ($Value | ConvertTo-Json -Depth 100) -replace "`r`n", "`n"
    [System.IO.File]::WriteAllText($Path, $json + "`n", [System.Text.UTF8Encoding]::new($false))
}

$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
$actualGames = @()

foreach ($gameName in $games) {
    if ($gameName -eq 'd1') {
        $requestedDataDir = if ($D1DataDir) { $D1DataDir } else { $DataDir }
    } else {
        $requestedDataDir = if ($D2DataDir) { $D2DataDir } else { $DataDir }
    }
    $resolvedDataDir = Resolve-SecretAreaDataDir -Name $gameName -Requested $requestedDataDir
    if (-not $resolvedDataDir) {
        continue
    }

    if ($gameName -eq 'd1') {
        $exe = if ($D1Exe) { $D1Exe } else { Get-SecretAreaDefaultExe -Name 'd1' }
    } else {
        $exe = if ($D2Exe) { $D2Exe } else { Get-SecretAreaDefaultExe -Name 'd2' }
    }
    $gameJsonPath = Join-Path $OutputDir "$gameName.json"
    $dump = Invoke-SecretAreaDump -Name $gameName -Exe $exe -ResolvedDataDir $resolvedDataDir -OutPath $gameJsonPath
    if ($dump) {
        $actualGames += $dump
    }
}

if ($actualGames.Count -ne $games.Count) {
    if ($RequireAssets) {
        throw "Secret-area baseline did not run every requested game"
    }
    Write-Host "Secret-area baseline skipped because not all requested games could run" -ForegroundColor Yellow
    exit 0
}

$actual = [ordered]@{
    schema = 'dxx-secret-area-regression-v1'
    games = $actualGames
}
$actualPath = Join-Path $OutputDir 'actual_secret_area_base_game_baseline.json'
Write-CanonicalJsonFile -Value $actual -Path $actualPath

if ($UpdateBaseline) {
    Write-CanonicalJsonFile -Value $actual -Path $BaselinePath
    Write-Host "Secret-area baseline updated: $BaselinePath"
    exit 0
}

if (-not (Test-Path -LiteralPath $BaselinePath -PathType Leaf)) {
    throw "Secret-area baseline missing: $BaselinePath. Run android/tests/update_secret_area_baseline.ps1"
}

$expectedText = (Get-Content -LiteralPath $BaselinePath -Raw).Trim()
$actualText = (Get-Content -LiteralPath $actualPath -Raw).Trim()
if ($expectedText -ne $actualText) {
    throw "Secret-area baseline changed`nExpected: $BaselinePath`nActual: $actualPath"
}

Write-Host "Secret-area baseline matched: $BaselinePath"
