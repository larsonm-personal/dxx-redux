#!/usr/bin/env pwsh
# Exercise the real replay wrapper against controlled child-process outcomes
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'This process integration test requires the Windows .NET Framework compiler' }
$testRoot = Join-Path $repoRoot ('temp/input_demo_replay_failures_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$nativeSource = Join-Path $testRoot 'child.cs'
$nativeExe = Join-Path $testRoot 'child.exe'
@'
using System;
using System.IO;
using System.Threading;
class ReplayChild {
    static int Main(string[] args) {
        string scenario = Environment.GetEnvironmentVariable("DXX_REPLAY_RUNNER_TEST");
        File.WriteAllText("gamelog.txt", "startup marker: " + scenario);
        File.WriteAllText("stderr.txt", "native diagnostic: " + scenario);
        if (scenario == "early") return 23;
        if (scenario == "timeout") { Thread.Sleep(60000); return 0; }
        int index = Array.IndexOf(args, "-inputdemo-actual-result");
        if (index < 0 || index + 1 >= args.Length) return 24;
        File.WriteAllText(args[index + 1], "{\"frame_count\":1}");
        return scenario == "result-error" ? 17 : 0;
    }
}
'@ | Set-Content -LiteralPath $nativeSource -Encoding ascii
& $compiler /nologo /target:exe "/out:$nativeExe" $nativeSource
if ($LASTEXITCODE -ne 0) { throw 'Could not build the controlled replay child' }
$data = Join-Path $testRoot 'data'
New-Item -ItemType Directory -Path $data | Out-Null
# The child never interprets game archives; these satisfy the wrapper's file inventory
Set-Content -LiteralPath (Join-Path $data 'descent.hog') -Value ''
Set-Content -LiteralPath (Join-Path $data 'descent.pig') -Value ''
$pwsh = (Get-Process -Id $PID).Path
$previousScenario = $env:DXX_REPLAY_RUNNER_TEST
$report = @()
try {
    foreach ($scenario in @('prelaunch', 'early', 'timeout', 'result-error', 'mismatch', 'success')) {
        $env:DXX_REPLAY_RUNNER_TEST = $scenario
        $stem = 'runner-' + [guid]::NewGuid().ToString('N')
        $demo = Join-Path $testRoot ($stem + '.dximdemo')
        $expectedFrames = if ($scenario -eq 'mismatch') { 2 } else { 1 }
        @(
            '{"type":"header","version":4,"game":"d1","mission":"d1","level":1,"start_mode":"level_start","frame_count":1}',
            '{"type":"frame","f":0,"ft":2621}',
            ('{"type":"result","result":{"frame_count":' + $expectedFrames + '}}')
        ) | Set-Content -LiteralPath $demo -Encoding utf8
        if ($scenario -eq 'prelaunch') {
            $headerOnly = Get-Content -LiteralPath $demo -TotalCount 1
            Set-Content -LiteralPath $demo -Value $headerOnly -Encoding utf8
        }
        $result = Join-Path $testRoot ($scenario + '.result.json')
        $log = Join-Path $testRoot ($scenario + '.log')
        & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') `
            -DemoPath $demo -DataDir $data -ExecutablePath $nativeExe -Runner windowed-no-present `
            -Mode accelerated -TimeoutSeconds 2 -ResultCopyPath $result -StrictComparison *> $log
        $exitCode = $LASTEXITCODE
        $archives = @(Get-ChildItem -LiteralPath $testRoot -Directory | Where-Object Name -Like ($scenario + '.result.json.failure_*'))
        $sandbox = Join-Path $repoRoot ('temp/input_demo_runtime_wrapper/d1/' + $stem)
        if (Test-Path -LiteralPath $sandbox) { throw "$scenario left its staged engine package behind" }
        if ($scenario -eq 'success') {
            if ($exitCode -ne 0 -or $archives.Count -ne 0 -or -not (Test-Path -LiteralPath $result)) {
                throw "Successful replay was not accepted and cleaned: $log"
            }
        } else {
            if ($exitCode -eq 0 -or $archives.Count -ne 1) { throw "Missing failure verdict or diagnostics for ${scenario}: $log" }
            $archive = $archives[0].FullName
            $failure = Get-Content -LiteralPath (Join-Path $archive 'failure.json') -Raw | ConvertFrom-Json
            if ($scenario -eq 'prelaunch') {
                if ($null -ne $failure.process_id -or $null -ne $failure.native_exit_code -or $failure.error -notlike '*does not end with a result record*') {
                    throw 'Prelaunch failure invented a process outcome or lost its cause'
                }
                $report += [ordered]@{ scenario = $scenario; wrapper_exit = $exitCode; passed = $true }
                continue
            }
            if (Get-Process -Id $failure.process_id -ErrorAction SilentlyContinue) { throw "$scenario left its replay child running" }
            if ($failure.source_executable -ne $nativeExe -or $failure.executable_sha256 -ne (Get-FileHash -LiteralPath $nativeExe -Algorithm SHA256).Hash) {
                throw "$scenario lost the executed binary's identity"
            }
            if ((Get-Content -LiteralPath (Join-Path $archive 'gamelog.txt') -Raw) -ne ('startup marker: ' + $scenario) -or
                (Get-Content -LiteralPath (Join-Path $archive 'stderr.txt') -Raw) -ne ('native diagnostic: ' + $scenario)) {
                throw "$scenario lost native diagnostics"
            }
            if (@(Get-ChildItem -LiteralPath $archive -Filter '*.exe').Count) { throw "$scenario archived the staged executable" }
            if ($scenario -eq 'early' -and $failure.native_exit_code -ne 23) { throw 'Early exit code was lost' }
            if ($scenario -eq 'timeout' -and (-not $failure.forced_stop -or $failure.error -notlike '*Timed out*')) { throw 'Timeout details were lost' }
            if ($scenario -eq 'result-error' -and ($failure.native_exit_code -ne 17 -or -not (Test-Path -LiteralPath (Join-Path $archive 'result.actual.json')))) {
                throw 'Result-then-failure was not retained as a failure'
            }
            if ($scenario -eq 'mismatch' -and $failure.error -notlike '*Result compare failed*') { throw 'Comparison failure details were lost' }
        }
        $report += [ordered]@{ scenario = $scenario; wrapper_exit = $exitCode; passed = $true }
    }
} finally {
    $env:DXX_REPLAY_RUNNER_TEST = $previousScenario
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $testRoot 'report.json') -Encoding utf8
Write-Host "Replay process failure integration passed: $testRoot"
