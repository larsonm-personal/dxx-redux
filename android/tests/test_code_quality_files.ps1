#!/usr/bin/env pwsh
# Exercise formatter file selection without invoking formatters
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/code-quality-files.ps1')
$fixture = Join-Path $repoRoot ("temp/code_quality_files_{0}" -f (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts @($fixture) | Out-Null

foreach ($relative in @(
        'android/src/a.kt', 'android/src/nested/b.KT', 'android/src/ignore.c',
        'android/src-other/c.kt', 'android/build/generated.kt', 'android/temp/test.kt',
        'android/app/src/main/cpp/CMakeLists.txt', 'android/app/src/main/cpp/extract/CMakeLists.txt',
        'android/tests/CMakeLists.txt', 'android/tools/etc2tool/CMakeLists.txt',
        'cmake/one.cmake', 'cmake-other/two.cmake', 'd1/main/CMakeLists.txt'
    )) {
    $file = Join-Path $fixture $relative
    New-Item -ItemType Directory -Path (Split-Path $file) -Force | Out-Null
    [IO.File]::WriteAllText($file, '')
}

function Assert-Files {
    param([string]$Case, [object[]]$Actual, [string[]]$Expected)
    $names = @($Actual | ForEach-Object { [IO.Path]::GetRelativePath($fixture, $_.FullName).Replace('\', '/') } | Sort-Object)
    if (($names -join '|') -ne (($Expected | Sort-Object) -join '|')) {
        throw "$Case expected [$($Expected -join ', ')], got [$($names -join ', ')]"
    }
}

$scope = @{ RepoRoot = $fixture; RootPath = (Join-Path $fixture 'android/src'); ValidExtensions = @('.kt') }
$both = @('android/src/a.kt', 'android/src/nested/b.KT')
Assert-Files 'unscoped' @(Get-CodeQualityScopedFiles @scope) $both
Assert-Files 'parent input' @(Get-CodeQualityScopedFiles @scope -InputPaths @('android')) $both
Assert-Files 'directory and duplicate file' @(Get-CodeQualityScopedFiles @scope -InputPaths @('android/src/', 'android/src/a.kt')) $both
Assert-Files 'explicit file' @(Get-CodeQualityScopedFiles @scope -InputPaths @('android/src/a.kt')) @('android/src/a.kt')
Assert-Files 'absolute file' @(Get-CodeQualityScopedFiles @scope -InputPaths @((Join-Path $fixture 'android/src/a.kt'))) @('android/src/a.kt')
Assert-Files 'sibling prefix' @(Get-CodeQualityScopedFiles @scope -InputPaths @('android/src-other')) @()
Assert-Files 'missing inputs' @(Get-CodeQualityScopedFiles @scope -InputPaths @('absent', ' ')) @()
Assert-Files 'wrong extension' @(Get-CodeQualityScopedFiles @scope -InputPaths @('android/src/ignore.c')) @()
if ($env:OS -eq 'Windows_NT') {
    Assert-Files 'Windows case' @(Get-CodeQualityScopedFiles @scope -InputPaths @('ANDROID/SRC/A.KT')) @('android/src/a.kt')
}
$scope.RootPath = Join-Path $fixture 'android'
Assert-Files 'excluded generated paths' @(Get-CodeQualityScopedFiles @scope -ExcludePattern '[\\/](build)[\\/]') ($both + @('android/src-other/c.kt', 'android/temp/test.kt'))

$cmake = @('android/app/src/main/cpp/CMakeLists.txt', 'android/app/src/main/cpp/extract/CMakeLists.txt',
    'android/tests/CMakeLists.txt', 'android/tools/etc2tool/CMakeLists.txt', 'cmake/one.cmake')
Assert-Files 'CMake allowlist' @(Get-CodeQualityCmakeFiles -RepoRoot $fixture) $cmake
Assert-Files 'CMake directory' @(Get-CodeQualityCmakeFiles -RepoRoot $fixture -InputPaths @('cmake')) @('cmake/one.cmake')
Assert-Files 'CMake inherited exclusion' @(Get-CodeQualityCmakeFiles -RepoRoot $fixture -InputPaths @('d1')) @()
Assert-Files 'CMake nonexistent input' @(Get-CodeQualityCmakeFiles -RepoRoot $fixture -InputPaths @('absent')) @()
$allCmake = @(Get-ChildItem -LiteralPath $fixture -Recurse -File -Filter '*.cmake')
Assert-Files 'filter sibling prefix' @(Select-CodeQualityInputFiles -RepoRoot $fixture -AllFiles $allCmake -InputPaths @('cmake')) @('cmake/one.cmake')
Write-Host 'PASS: code quality file selection'
