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

# Run the actual entry point in an isolated repository with recording tool stubs
$fixtureHelpers = Join-Path $fixture 'android/helpers'
New-Item -ItemType Directory -Path $fixtureHelpers -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'android/run-code-quality.ps1') -Destination (Join-Path $fixture 'android/run-code-quality.ps1')
Copy-Item -LiteralPath (Join-Path $repoRoot 'android/helpers/powershell_compat.ps1') -Destination $fixtureHelpers
$toolLog = Join-Path $fixture 'android/tool-calls.jsonl'
$toolStub = @'
param([switch]$Check, [string[]]$Paths)
[ordered]@{ tool = [IO.Path]::GetFileName($PSCommandPath); scoped = $PSBoundParameters.ContainsKey('Paths'); paths = @($Paths) } | ConvertTo-Json -Compress | Add-Content -LiteralPath (Join-Path (Split-Path $PSScriptRoot) 'tool-calls.jsonl')
exit 0
'@
foreach ($tool in @('run-clang-format', 'run-ktlint', 'run-psscriptanalyzer', 'run-shellcheck', 'run-shfmt', 'run-cmake-format', 'run-cmake-lint')) {
    [IO.File]::WriteAllText((Join-Path $fixtureHelpers "$tool.ps1"), $toolStub)
}
[IO.File]::WriteAllText((Join-Path $fixture '.hidden'), 'hidden fixture')
[IO.File]::WriteAllText((Join-Path $fixture 'second file.md'), 'second fixture')
$powershell = (Get-Process -Id $PID).Path
$runner = Join-Path $fixture 'android/run-code-quality.ps1'
try {
    $output = & $powershell -NoProfile -File $runner -Fix -Paths '.hidden' 'second file.md' 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw "Multi-path scope failed: $output" }
    $calls = @(Get-Content -LiteralPath $toolLog | ForEach-Object { $_ | ConvertFrom-Json })
    if ($calls.Count -ne 7) { throw 'Not all formatter stages received the scope' }
    foreach ($call in $calls) {
        if (-not $call.scoped -or $call.paths.Count -ne 2 -or
            (Join-Path $fixture '.hidden') -notin $call.paths -or
            (Join-Path $fixture 'second file.md') -notin $call.paths) { throw 'Space-separated or hidden paths were dropped from formatter scope' }
    }
    Remove-Item -LiteralPath $toolLog
    $output = & $powershell -NoProfile -File $runner -Fix -Paths '.hidden' 'missing-file' 2>&1 | Out-String
    if ($LASTEXITCODE -ne 1 -or $output -notmatch 'Code-quality path not found' -or (Test-Path -LiteralPath $toolLog)) { throw 'Invalid scope reached formatters instead of stopping' }
    $output = & $powershell -NoProfile -File $runner -Fix -Paths '' 2>&1 | Out-String
    if ($LASTEXITCODE -ne 1 -or (Test-Path -LiteralPath $toolLog)) { throw 'Empty explicit scope reached formatters' }
    $output = & $powershell -NoProfile -File $runner 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw "Default unscoped invocation regressed: $output" }
    $calls = @(Get-Content -LiteralPath $toolLog | ForEach-Object { $_ | ConvertFrom-Json })
    if ($calls.Count -ne 7 -or @($calls | Where-Object scoped).Count) { throw 'Default unscoped invocation passed an unexpected explicit path' }
    Write-Host 'PASS: formatter entry-point scope binding, hidden paths, invalid-scope rejection and default mode'
} finally {
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
