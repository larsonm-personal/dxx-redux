#!/usr/bin/env pwsh
# Build instrumented binaries and run existing engine/corpus tests without rewriting baselines
[CmdletBinding()]
param(
    [ValidateSet('All', 'EngineTests', 'Demos', 'D1InD2Demos', 'Routes', 'LoaderBounds', 'ConfigBounds')]
    [string[]]$Category = @('All'),
    [ValidateSet('both', 'd1', 'd2')][string]$Game = 'both',
    [string[]]$MissionJson,
    [int[]]$Level,
    [switch]$RoutingDevelopmentSet,
    [string[]]$DemoFileName,
    [string]$CTestFilter,
    [ValidateRange(1, 128)][int]$MaxParallel = 4,
    [string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'This runner currently requires Windows MSVC AddressSanitizer; see SANITIZERS.md for CMake usage on other hosts' }
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
if (-not $OutputRoot) { $OutputRoot = Join-Path $repoRoot ('android/temp/engine_sanitizers/' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
$OutputRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
if (Test-Path -LiteralPath $OutputRoot) { throw "Use a new output directory: $OutputRoot" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $OutputRoot
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$categories = if ($Category -contains 'All') { @('EngineTests', 'ConfigBounds', 'LoaderBounds', 'Demos', 'D1InD2Demos', 'Routes') } else { $Category }
$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
if ($categories | Where-Object { $_ -in @('Routes', 'LoaderBounds', 'ConfigBounds', 'D1InD2Demos') }) { $games = @(@($games) + @('d2') | Sort-Object -Unique) }
$results = [Collections.Generic.List[object]]::new()
$pwsh = (Get-Process -Id $PID).Path
$oldAsanOptions = $env:ASAN_OPTIONS
# Fail on the first memory error, with allocation stacks and no user-supplied suppression options
$env:ASAN_OPTIONS = 'halt_on_error=1:allocator_may_return_null=0'

function Invoke-SanitizerStage {
    param([string]$Name, [scriptblock]$Action)
    $log = Join-Path $OutputRoot "$Name.log"
    Write-Host "Sanitizer stage: $Name (log: $log)"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $exitCode = 1
    try {
        & $Action *>&1 | Tee-Object -FilePath $log | Out-Host
        $exitCode = $LASTEXITCODE
    } catch {
        $_ | Out-String | Tee-Object -FilePath $log -Append | Out-Host
    }
    $results.Add([pscustomobject][ordered]@{ name = $Name; exit_code = $exitCode; seconds = [int]$watch.Elapsed.TotalSeconds; log = $log })
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputRoot 'summary.json') -Encoding utf8
    return ($exitCode -eq 0)
}

try {
    foreach ($buildGame in $games) {
        if (-not (Invoke-SanitizerStage "build-$buildGame" {
                    & $pwsh -NoProfile -File (Join-Path $repoRoot 'run-windows-build.ps1') -Target $buildGame -Sanitizer address -MaxParallel $MaxParallel
                })) { throw "Cannot test without a successful instrumented $buildGame build" }
        $buildDir = Join-Path $repoRoot "build$buildGame-asan"
        $cache = Get-Content -LiteralPath (Join-Path $buildDir 'CMakeCache.txt') -Raw
        if ($cache -notmatch '(?m)^DXX_SANITIZERS:STRING=address\r?$') { throw "Build is not instrumented: $buildDir" }
        $manifest = [ordered]@{
            game = $buildGame
            sanitizer = 'address'
            options = $env:ASAN_OPTIONS
            revision = ([string](& git -C $repoRoot rev-parse HEAD)).Trim()
            binaries = @(Get-ChildItem -LiteralPath (Join-Path $buildDir 'main') -File | Where-Object Extension -in @('.exe', '.pdb') | ForEach-Object {
                    [ordered]@{ file = $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
                })
        }
        $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputRoot "build-$buildGame.manifest.json") -Encoding utf8
    }
    $routeExe = Join-Path $repoRoot 'buildd2-asan/main/dxx-redux-d2-headless-route.exe'
    foreach ($stage in $categories) {
        switch ($stage) {
            'EngineTests' {
                foreach ($testGame in $games) {
                    $buildDir = Join-Path $repoRoot "build$testGame-asan"
                    $cmake = Resolve-RegressionCMakePath -RepoRoot $repoRoot -BuildDir $buildDir
                    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
                    $testArgs = @('--test-dir', $buildDir, '--output-on-failure', '--no-tests=error', '--parallel', $MaxParallel, '--timeout', '300')
                    if ($CTestFilter) { $testArgs += @('-R', $CTestFilter) }
                    $null = Invoke-SanitizerStage "ctest-$testGame" { & $ctest @testArgs }
                    $lastTestLog = Join-Path $buildDir 'Testing/Temporary/LastTest.log'
                    if (Test-Path -LiteralPath $lastTestLog) {
                        Copy-Item -LiteralPath $lastTestLog -Destination (Join-Path $OutputRoot "ctest-$testGame-detail.log")
                    }
                }
            }
            { $_ -in @('Demos', 'D1InD2Demos') } {
                # Array-valued filters must be splatted in-process, not flattened through native pwsh argv
                $null = Invoke-SanitizerStage $stage {
                    & (Join-Path $PSScriptRoot 'run_input_demo_regressions.ps1') -Sanitizer address -Mode accelerated -RunMode headless -KeepSandbox -ResultArchiveRoot (Join-Path $OutputRoot $stage) -DemoFileName $DemoFileName -RecordedGame $(if ($stage -eq 'D1InD2Demos') { 'd1' } elseif ($Game -eq 'both') { 'all' } else { $Game }) -D1InD2:($stage -eq 'D1InD2Demos')
                }
            }
            'LoaderBounds' {
                $null = Invoke-SanitizerStage 'loader-bounds' {
                    & $pwsh -NoProfile -File (Join-Path $repoRoot 'android/helpers/test_level_loader_bounds.ps1') -HeadlessExecutable $routeExe -OutputRoot (Join-Path $OutputRoot 'loader-bounds')
                }
            }
            'ConfigBounds' {
                foreach ($variant in @('unterminated', 'concurrent', 'isolated')) {
                    $configArgs = @('-NoProfile', '-File', (Join-Path $repoRoot 'android/helpers/test_headless_config_read_bounds.ps1'), '-Executable', $routeExe, '-HogDir', (Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'), '-OutputRoot', (Join-Path $OutputRoot "config-$variant"))
                    if ($variant -eq 'concurrent') { $configArgs += '-ConcurrentRewrite' }
                    if ($variant -eq 'isolated') { $configArgs += '-IsolatedDirectory' }
                    $null = Invoke-SanitizerStage "config-$variant" { & $pwsh @configArgs }
                }
            }
            'Routes' {
                $null = Invoke-SanitizerStage 'routes' {
                    & (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') -NoBuild -HeadlessExecutable $routeExe -MaxParallel $MaxParallel -MissionJson $MissionJson -Level $Level -RoutingDevelopmentSet:$RoutingDevelopmentSet -OutputRoot (Join-Path $OutputRoot 'routes')
                }
            }
        }
    }
} finally {
    $env:ASAN_OPTIONS = $oldAsanOptions
}
# Native diagnostics are failures even if an expected-error test swallowed its child's exit status
$findings = @(Get-ChildItem -LiteralPath $OutputRoot -Recurse -File -Filter '*.log' | Select-String -Pattern 'ERROR: AddressSanitizer|SUMMARY: AddressSanitizer|AddressSanitizer:DEADLYSIGNAL')
$results | Format-Table name, exit_code, seconds | Out-Host
Write-Host "Sanitizer reports: $OutputRoot"
if ($findings.Count -or @($results | Where-Object exit_code -ne 0).Count) { exit 1 }
exit 0
