#!/usr/bin/env pwsh
# Exercise the real replay wrapper against controlled child-process outcomes
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$windowsHost = Test-RegressionWindowsHost
$compiler = if ($windowsHost) {
    Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
} else {
    (Get-Command cc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
}
if (-not (Test-Path -LiteralPath $compiler)) { throw "Controlled replay child compiler is missing: $compiler" }
$testRoot = Join-Path $repoRoot ('temp/input_demo_replay_failures_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$nativeSource = Join-Path $testRoot $(if ($windowsHost) { 'child.cs' } else { 'child.c' })
$nativeExe = Join-Path $testRoot (Get-RegressionHostExecutableNames -BaseName 'child')[0]
$fixtureLock = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $testRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    if ($windowsHost) {
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
        if (scenario == "result-hang") { Thread.Sleep(60000); return 0; }
        return scenario == "result-error" ? 17 : 0;
    }
}
'@ | Set-Content -LiteralPath $nativeSource -Encoding ascii
        & $compiler /nologo /target:exe "/out:$nativeExe" $nativeSource
    } else {
        @'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern int fixture_frame_count(void);
static void write_text(const char *path, const char *prefix, const char *value) {
    FILE *file = fopen(path, "w");
    if (!file) exit(25);
    if (fprintf(file, "%s%s", prefix, value) < 0 || fclose(file) != 0) exit(25);
}
static void join_path(char *path, size_t size, const char *directory, const char *name) {
    int length = snprintf(path, size, "%s/%s", directory, name);
    if (length < 0 || (size_t)length >= size) exit(31);
}
static int contents_equal(const char *path, const unsigned char *expected, size_t length) {
    unsigned char contents[1024];
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    size_t count = fread(contents, 1, sizeof(contents), file);
    int failed = ferror(file);
    fclose(file);
    return !failed && count == length && memcmp(contents, expected, length) == 0;
}
static int verify_home(int argc, char **argv) {
    const char *home = getenv("HOME");
    const char *parent = getenv("DXX_REPLAY_TEST_PARENT_HOME");
    const char *relative = getenv("DXX_REPLAY_TEST_PROFILE");
    const char *expected_pilot = getenv("DXX_REPLAY_TEST_PILOT");
    const char *pilot = NULL;
    int players = 0;
    char cwd[8192], profile[8192], path[16384], config[1024];
    if (!home || !parent || !relative || !getcwd(cwd, sizeof(cwd))) return 28;
    if (strcmp(home, parent) == 0) return 28;
    size_t cwd_length = strlen(cwd);
    if (strlen(home) <= cwd_length || strncmp(home, cwd, cwd_length) || home[cwd_length] != '/') return 29;
    join_path(profile, sizeof(profile), home, relative);
    join_path(path, sizeof(path), profile, "descent.cfg");
    FILE *file = fopen(path, "r");
    if (!file) return 30;
    size_t size = fread(config, 1, sizeof(config) - 1, file);
    fclose(file);
    config[size] = 0;
    if (!strstr(config, "ResolutionX=640") || !strstr(config, "ResolutionY=480")) return 30;
    for (const char **name = (const char *[]) { ".plr", ".plx", "unwanted.plr", NULL }; *name; ++name) {
        join_path(path, sizeof(path), profile, *name);
        if (access(path, F_OK) == 0) return 32;
    }
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-pilot") == 0 && i + 1 < argc) pilot = argv[++i];
        else if (strcmp(argv[i], "-use_players_dir") == 0) players = 1;
    }
    if (expected_pilot && *expected_pilot) {
        if (!pilot || strcmp(pilot, expected_pilot)) return 33;
        int nested = strcmp(pilot, "nested.plr") == 0;
        if (players != nested) return 34;
        const unsigned char expected[] = { 0, 255, 17, 34 };
        join_path(path, sizeof(path), profile, nested ? "Players/nested.plr" : "picked.plr");
        if (!contents_equal(path, expected, sizeof(expected))) return 35;
        join_path(path, sizeof(path), profile, nested ? "Players/nested.plx" : "picked.plx");
        const unsigned char extended[] = "selected pilot extensions";
        if (!contents_equal(path, extended, sizeof(extended) - 1)) return 36;
    } else {
        if (pilot || players) return 37;
        join_path(path, sizeof(path), profile, "picked.plr");
        if (access(path, F_OK) == 0) return 38;
    }
    write_text("observed-home.txt", "", home);
    join_path(path, sizeof(path), profile, ".plr");
    write_text(path, "", "new child pilot must not leak into a reused launch");
    return 0;
}
int main(int argc, char **argv) {
    if (fixture_frame_count() != 1) return 27;
    const char *scenario = getenv("DXX_REPLAY_RUNNER_TEST");
    if (!scenario) return 26;
    write_text("gamelog.txt", "startup marker: ", scenario);
    write_text("stderr.txt", "native diagnostic: ", scenario);
    int home_error = verify_home(argc, argv);
    if (home_error) return home_error;
    if (strcmp(scenario, "early") == 0) return 23;
    if (strcmp(scenario, "timeout") == 0) { sleep(60); return 0; }
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "-inputdemo-actual-result") == 0) {
            write_text(argv[i + 1], "", "{\"frame_count\":1}");
            if (strcmp(scenario, "result-hang") == 0) { sleep(60); return 0; }
            return strcmp(scenario, "result-error") == 0 ? 17 : 0;
        }
    }
    return 24;
}
'@ | Set-Content -LiteralPath $nativeSource -Encoding ascii
        $runtimeSource = Join-Path $testRoot 'child_runtime.c'
        $runtimeLibrary = Join-Path $testRoot 'libreplay_fixture.so.1'
        'int fixture_frame_count(void) { return 1; }' | Set-Content -LiteralPath $runtimeSource -Encoding ascii
        & $compiler -shared -fPIC -Wall -Wextra -Werror -o $runtimeLibrary $runtimeSource
        if ($LASTEXITCODE -ne 0) { throw 'Could not build the replay fixture shared library' }
        & $compiler -std=c99 -Wall -Wextra -Werror -o $nativeExe $nativeSource "-L$testRoot" '-l:libreplay_fixture.so.1' '-Wl,-rpath,$ORIGIN'
    }
    if ($LASTEXITCODE -ne 0) { throw 'Could not build the controlled replay child' }
    $data = Join-Path $testRoot 'data'
    New-Item -ItemType Directory -Path $data | Out-Null
    # The child never interprets game archives; these satisfy the wrapper's file inventory
    Set-Content -LiteralPath (Join-Path $data 'descent.hog') -Value ''
    Set-Content -LiteralPath (Join-Path $data 'descent.pig') -Value ''
    foreach ($name in @('descent2.hog', 'descent2.ham', 'groupa.pig')) {
        Set-Content -LiteralPath (Join-Path $data $name) -Value ''
    }
    $pwsh = (Get-Process -Id $PID).Path
    $previousScenario = $env:DXX_REPLAY_RUNNER_TEST
    $savedEnvironment = @{}
    foreach ($name in @('HOME', 'DXX_REPLAY_TEST_PARENT_HOME', 'DXX_REPLAY_TEST_PROFILE', 'DXX_REPLAY_TEST_PILOT')) {
        $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name)
    }
    $parentFiles = @{}
    if (-not $windowsHost) {
        $parentHome = Join-Path $testRoot 'inherited-home'
        $profileNames = @{}
        foreach ($gameName in @('d1', 'd2')) {
            $relative = if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::OSX)) {
                "Library/Preferences/$($gameName.ToUpperInvariant())X Redux"
            } else { ".${gameName}x-redux" }
            $profileNames[$gameName] = $relative
            $profile = Join-Path $parentHome $relative
            New-Item -ItemType Directory -Path (Join-Path $profile 'Players') -Force | Out-Null
            foreach ($name in @('.plr', '.plx', 'unwanted.plr', 'descent.cfg')) {
                [IO.File]::WriteAllText((Join-Path $profile $name), 'stale parent pilot or configuration')
            }
            foreach ($base in @('picked', 'Players/nested')) {
                [IO.File]::WriteAllBytes((Join-Path $profile ($base + '.plr')), [byte[]]@(0, 255, 17, 34))
                [IO.File]::WriteAllText((Join-Path $profile ($base + '.plx')), 'selected pilot extensions')
            }
        }
        foreach ($file in Get-ChildItem -LiteralPath $parentHome -File -Recurse -Force) {
            $parentFiles[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
        $env:HOME = $parentHome
        $env:DXX_REPLAY_TEST_PARENT_HOME = $parentHome
        $env:DXX_REPLAY_TEST_PROFILE = $profileNames.d1
        $env:DXX_REPLAY_TEST_PILOT = ''
    }
    $report = @()
    try {
        foreach ($runner in @('headless-console', 'visual')) {
            foreach ($scenario in @('prelaunch', 'early', 'timeout', 'result-error', 'result-hang', 'mismatch', 'success')) {
                $env:DXX_REPLAY_RUNNER_TEST = $scenario
                $stem = 'runner-' + [guid]::NewGuid().ToString('N')
                $demo = Join-Path $testRoot ($stem + '.dximdemo')
                $expectedFrames = if ($scenario -eq 'mismatch') { 2 } else { 1 }
                @(
                    '{"type":"header","version":4,"game":"d1","mission":"d1","level":1,"start_mode":"new_level","frame_count":1}',
                    '{"type":"frame","f":0,"ft":2621}',
                    ('{"type":"result","result":{"frame_count":' + $expectedFrames + '}}')
                ) | Set-Content -LiteralPath $demo -Encoding utf8
                if ($scenario -eq 'prelaunch') {
                    $headerOnly = Get-Content -LiteralPath $demo -TotalCount 1
                    Set-Content -LiteralPath $demo -Value $headerOnly -Encoding utf8
                }
                $result = Join-Path $testRoot ($runner + '-' + $scenario + '.result.json')
                $log = Join-Path $testRoot ($runner + '-' + $scenario + '.log')
                & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') `
                    -AllowUnverifiedAssets -DemoPath $demo -DataDir $data -ExecutablePath $nativeExe -Runner $runner `
                    -Mode accelerated -TimeoutSeconds 2 -ResultCopyPath $result -StrictComparison -RequireCleanExit *> $log
                $exitCode = $LASTEXITCODE
                $archives = @(Get-ChildItem -LiteralPath $testRoot -Directory | Where-Object Name -Like ($runner + '-' + $scenario + '.result.json.failure_*'))
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
                        $report += [ordered]@{ runner = $runner; scenario = $scenario; wrapper_exit = $exitCode; passed = $true }
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
                    if (@(Get-ChildItem -LiteralPath $archive -File | Where-Object { $_.Name -match '\.(exe|dll|dylib)$|\.so(\.[0-9]+)*$' -or $_.Name -eq [IO.Path]::GetFileName($nativeExe) }).Count) { throw "$scenario archived a staged executable or runtime library" }
                    if ($scenario -eq 'early' -and $failure.native_exit_code -ne 23) { throw 'Early exit code was lost' }
                    if ($scenario -eq 'timeout' -and (-not $failure.forced_stop -or $failure.error -notlike '*Timed out*')) { throw 'Timeout details were lost' }
                    if ($scenario -eq 'result-error' -and ($failure.native_exit_code -ne 17 -or -not (Test-Path -LiteralPath (Join-Path $archive 'result.actual.json')))) {
                        throw 'Result-then-failure was not retained as a failure'
                    }
                    if ($scenario -eq 'result-hang' -and (-not $failure.forced_stop -or $failure.error -notlike '*did not exit cleanly*')) { throw 'Result-then-hang was incorrectly accepted' }
                    if ($scenario -eq 'mismatch' -and $failure.error -notlike '*Result compare failed*') { throw 'Comparison failure details were lost' }
                }
                $report += [ordered]@{ runner = $runner; scenario = $scenario; wrapper_exit = $exitCode; passed = $true }
            }
        }
        if (-not $windowsHost) {
            # Prove the child detects the old inherited-HOME behavior
            $env:DXX_REPLAY_RUNNER_TEST = 'success'
            Push-Location $testRoot
            try {
                & $nativeExe '-inputdemo-actual-result' (Join-Path $testRoot 'inherited-home-must-fail.json')
                if ($LASTEXITCODE -ne 28) { throw 'Controlled child failed to detect inherited parent HOME' }
            } finally { Pop-Location }
            foreach ($gameName in @('d1', 'd2')) {
                $env:DXX_REPLAY_TEST_PROFILE = $profileNames[$gameName]
                foreach ($runner in @('headless-console', 'visual')) {
                    $stem = 'isolated-home-' + [guid]::NewGuid().ToString('N')
                    $demo = Join-Path $testRoot ($stem + '.dximdemo')
                    @(
                        ('{"type":"header","version":4,"game":"' + $gameName + '","mission":"' + $gameName + '","level":1,"start_mode":"new_level","frame_count":1}'),
                        '{"type":"frame","f":0,"ft":2621}', '{"type":"result","result":{"frame_count":1}}'
                    ) | Set-Content -LiteralPath $demo -Encoding utf8
                    $sandbox = Join-Path $repoRoot ('temp/input_demo_runtime_wrapper/' + $gameName + '/' + $stem)
                    $homes = @()
                    try {
                        $pilots = if ($runner -eq 'visual') { @('', '', 'picked', 'nested.plr', 'absent') } else { @('', '') }
                        foreach ($selectedPilot in $pilots) {
                            $env:DXX_REPLAY_TEST_PILOT = $selectedPilot
                            $arguments = @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'),
                                '-DemoPath', $demo, '-DataDir', $data, '-ExecutablePath', $nativeExe, '-Runner', $runner,
                                '-Mode', 'accelerated', '-TimeoutSeconds', '2', '-StrictComparison', '-RequireCleanExit',
                                '-ReuseSandbox', '-KeepSandbox', '-AllowUnverifiedAssets')
                            if ($selectedPilot) { $arguments += @('-Pilot', $selectedPilot) }
                            $log = Join-Path $testRoot ($gameName + '-' + $runner + '-home-' + $homes.Count + '.log')
                            & $pwsh @arguments *> $log
                            if ($selectedPilot -eq 'absent') {
                                if ($LASTEXITCODE -eq 0 -or (Get-Content -LiteralPath $log -Raw) -notlike '*Requested pilot*was not found*') {
                                    throw "Missing explicit pilot silently fell back: $log"
                                }
                                continue
                            }
                            if ($LASTEXITCODE -ne 0) { throw "Native HOME isolation failed: $log" }
                            $childHome = Get-Content -LiteralPath (Join-Path $sandbox 'observed-home.txt') -Raw
                            if ($homes -contains $childHome -or -not $childHome.StartsWith($sandbox + [IO.Path]::DirectorySeparatorChar)) {
                                throw 'Reused sandbox did not create a fresh owned HOME'
                            }
                            $homes += $childHome
                            if ((Get-ChildItem -LiteralPath $childHome -Recurse -File -Force | Where-Object Name -eq 'unwanted.plr').Count) {
                                throw 'An unrequested parent pilot was staged'
                            }
                            $report += [ordered]@{ runner = $runner; scenario = 'isolated-home'; game = $gameName; pilot = $selectedPilot; passed = $true }
                        }
                    } finally {
                        if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
                    }
                }
            }
            foreach ($path in $parentFiles.Keys) {
                if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $parentFiles[$path]) {
                    throw "Replay changed an inherited parent profile: $path"
                }
            }
        }
    } finally {
        $env:DXX_REPLAY_RUNNER_TEST = $previousScenario
        foreach ($name in $savedEnvironment.Keys) {
            [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name])
        }
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $testRoot 'report.json') -Encoding utf8
    Write-Host "Replay process failure integration passed: $testRoot"
} finally {
    if ($fixtureLock) { $fixtureLock.Dispose() }
    Remove-Item -LiteralPath $nativeExe, $nativeSource -Force -ErrorAction SilentlyContinue
    if (-not $windowsHost) {
        Remove-Item -LiteralPath (Join-Path $testRoot 'libreplay_fixture.so.1'), (Join-Path $testRoot 'child_runtime.c') -Force -ErrorAction SilentlyContinue
    }
}
