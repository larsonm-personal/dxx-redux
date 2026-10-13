#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$d1Executable = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd1'
$d2Executable = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd2'
$normalStamp = Get-InputDemoBuildStampPath -RepoRoot $repoRoot -GameName 'd2'
$script:InputDemoSanitizer = 'address'
foreach ($testGame in @('d1', 'd2')) {
    $asanExecutable = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName $testGame
    Assert-True ($asanExecutable -like "*build$testGame-asan*") 'Sanitizer demos must never select the normal build'
}
Assert-True ((Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName d2 -PreferHeadlessConsole) -like '*buildd2-asan*headless*') 'Console replays must select the instrumented build too'
Assert-True ((Get-InputDemoBuildStampPath -RepoRoot $repoRoot -GameName d2) -ne $normalStamp) 'Normal build stamps must not satisfy sanitizer freshness checks'
$script:InputDemoSanitizer = 'none'
Assert-True ([IO.Path]::GetFileNameWithoutExtension($d1Executable) -eq 'd1x-redux') `
    'D1 input-demo runs must use the public desktop executable name'
Assert-True ([IO.Path]::GetFileNameWithoutExtension($d2Executable) -eq 'd2x-redux') `
    'D2 input-demo runs must use the public desktop executable name'
$testRoot = Join-Path $repoRoot 'temp\test_input_demo_host_build_guard'
$caseRoot = Join-Path $testRoot ([guid]::NewGuid().ToString('N'))

try {
    New-Item -ItemType Directory -Path (Join-Path $caseRoot 'd2') -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $caseRoot 'd2\source.c'), "int value;`n")
    & git -C $caseRoot init --quiet 2>$null
    & git -C $caseRoot config user.email 'input-demo-test@example.invalid' 2>$null
    & git -C $caseRoot config user.name 'Input Demo Test' 2>$null
    & git -C $caseRoot add d2/source.c 2>$null
    & git -C $caseRoot commit --quiet -m 'source' 2>$null

    $executablePath = Join-Path $caseRoot 'buildd2\main\test.exe'
    New-Item -ItemType Directory -Path (Split-Path $executablePath -Parent) -Force | Out-Null
    [System.IO.File]::WriteAllText($executablePath, 'test')
    $cacheDirectory = Join-Path $caseRoot 'buildd2'
    $cachePath = Join-Path $cacheDirectory 'CMakeCache.txt'
    foreach ($configuration in @('Debug', 'RelWithDebInfo')) {
        $libraryPart = if ($configuration -eq 'Debug') { 'debug/lib' } else { 'lib' }
        [IO.File]::WriteAllText($cachePath, "CMAKE_BUILD_TYPE:STRING=$configuration`nVCPKG_TARGET_TRIPLET:STRING=x86-windows`nSDL_MIXER_LIBRARY:FILEPATH=C:/fixture/vcpkg_installed/x86-windows/$libraryPart/SDL_mixer.lib`n")
        Assert-True (-not (Test-RegressionCMakeCacheNeedsFreshConfigure -BuildDir $cacheDirectory -ExpectedTriplet x86-windows -ExpectedBuildType $configuration)) 'Matching build configuration should preserve the CMake cache'
        $other = if ($configuration -eq 'Debug') { 'RelWithDebInfo' } else { 'Debug' }
        Assert-True (Test-RegressionCMakeCacheNeedsFreshConfigure -BuildDir $cacheDirectory -ExpectedTriplet x86-windows -ExpectedBuildType $other) 'Changing build configuration must rediscover SDL libraries'
        [IO.File]::WriteAllText($cachePath, ([IO.File]::ReadAllText($cachePath).Replace("CMAKE_BUILD_TYPE:STRING=$configuration", "CMAKE_BUILD_TYPE:STRING=$other")))
        Assert-True (Test-RegressionCMakeCacheNeedsFreshConfigure -BuildDir $cacheDirectory -ExpectedTriplet x86-windows -ExpectedBuildType $other) 'An already switched cache must reject libraries from the old configuration'
        $runtimePart = if ($configuration -eq 'Debug') { 'debug/bin' } else { 'bin' }
        $runtimeDir = Join-Path $cacheDirectory "vcpkg_installed/x86-windows/$runtimePart"
        New-Item -ItemType Directory -Path $runtimeDir -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $runtimeDir 'SDL.dll'), $configuration)
        foreach ($output in @('main', 'tests')) {
            $outputDir = Join-Path $cacheDirectory $output
            New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
            $deployed = Join-Path $outputDir 'SDL.dll'
            [IO.File]::WriteAllText($deployed, $other)
            (Get-Item -LiteralPath $deployed).LastWriteTime = (Get-Date).AddDays(1)
        }
        Sync-RegressionWindowsRuntimeLibraries -BuildDir $cacheDirectory -Triplet x86-windows -BuildType $configuration
        foreach ($output in @('main', 'tests')) {
            Assert-True ([IO.File]::ReadAllText((Join-Path $cacheDirectory "$output/SDL.dll")) -eq $configuration) 'Configuration switch must replace a newer DLL from the previous configuration'
        }
    }
    $stampPath = Get-InputDemoBuildStampPath -RepoRoot $caseRoot -GameName 'd2'
    New-Item -ItemType Directory -Path (Split-Path $stampPath -Parent) -Force | Out-Null
    $revision = Get-InputDemoSourceRevision -RepoRoot $caseRoot
    [System.IO.File]::WriteAllText($stampPath, $revision)

    $issue = Get-InputDemoExecutableFreshnessIssue -RepoRoot $caseRoot -GameName 'd2' -ExecutablePath $executablePath
    Assert-True ($null -eq $issue) 'Matching revision stamp should accept a fresh executable'

    [System.IO.File]::WriteAllText((Join-Path $caseRoot 'README.md'), "revision only`n")
    & git -C $caseRoot add README.md 2>$null
    & git -C $caseRoot commit --quiet -m 'revision change' 2>$null
    $issue = Get-InputDemoExecutableFreshnessIssue -RepoRoot $caseRoot -GameName 'd2' -ExecutablePath $executablePath
    Assert-True ($null -ne $issue) 'A revision change must reject an executable even when source timestamps are unchanged'
    Assert-True ($issue.SourceRelative -eq 'source revision') 'Revision mismatch should identify its freshness cause'

    Write-Host 'Input demo host build guard tests passed'
} finally {
    $resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
    $resolvedCaseRoot = [System.IO.Path]::GetFullPath($caseRoot)
    if ($resolvedCaseRoot.StartsWith($resolvedTestRoot + [System.IO.Path]::DirectorySeparatorChar) -and
        (Test-Path -LiteralPath $resolvedCaseRoot)) {
        Remove-Item -LiteralPath $resolvedCaseRoot -Recurse -Force
    }
}

# D1 default console selection must reach the guard even if a desktop build exists
$script:GuardedExecutable = $null
function Ensure-InputDemoExecutable {
    param($RepoRoot, $GameName, $ExecutablePath, $Description, [switch]$BuildBeforeRun, [switch]$RequireFreshBuild)
    $script:GuardedExecutable = $ExecutablePath
}
foreach ($game in @('d1', 'd2')) {
    Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $game -PreferHeadlessConsole
    Assert-True ([IO.Path]::GetFileNameWithoutExtension($script:GuardedExecutable) -eq "dxx-redux-$game-headless") `
        'The default console path must validate/build the console executable in both games'
}
