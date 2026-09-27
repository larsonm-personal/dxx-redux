#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$helperPath = Join-Path $repoRoot "android\get_deps\helpers\Get-DepPlatform.ps1"
. $helperPath

$platform = Get-HostPlatform
if ($platform -eq "Unknown") {
    throw "Current PowerShell host platform was not detected"
}
if ($env:OS -eq "Windows_NT" -and $platform -ne "Windows") {
    throw "Windows host was detected as $platform"
}

$windowsPowerShell = Get-Command powershell.exe -ErrorAction SilentlyContinue
if ($windowsPowerShell) {
    $legacyResult = & $windowsPowerShell.Source -NoProfile -NonInteractive -Command `
        '& { param($Path) Set-StrictMode -Version Latest; . $Path; Get-HostPlatform }' $helperPath
    if ($LASTEXITCODE -ne 0 -or @($legacyResult)[-1] -ne "Windows") {
        throw "Windows PowerShell strict-mode platform detection failed"
    }
}

Write-Host "Dependency platform detection tests passed"

$savedHome = $env:HOME
try {
    $env:HOME = $null
    if ((Get-HomeDirectory) -ne [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)) {
        throw 'Home fallback did not resolve the user profile'
    }
} finally { $env:HOME = $savedHome }

$bash = Get-BashCommandPath
$shellHost = & $bash -c 'test -n "$BASH_VERSION" && uname -s'
if ($LASTEXITCODE -ne 0) { throw 'Selected shell is not Bash' }
if ($platform -eq 'Windows' -and $shellHost -notmatch '^(MINGW|MSYS|CYGWIN|.*_NT)') {
    throw 'Windows dependency installs selected a Linux shell'
}
if ($platform -eq 'Windows') {
    $preflight = Join-Path $repoRoot 'android/get_deps/helpers/assert_install_not_in_use.ps1'
    # The test runner itself holds this directory open, requiring no extra process
    $output = & $windowsPowerShell.Source -NoProfile -NonInteractive -File $preflight -InstallDirectory $PSHOME 2>&1
    if ($LASTEXITCODE -ne 1 -or ($output -join "`n") -notmatch "PID $PID") {
        throw 'Windows preflight failed to identify the running process'
    }
    & $windowsPowerShell.Source -NoProfile -NonInteractive -File $preflight -InstallDirectory "$PSHOME-other"
    if ($LASTEXITCODE -ne 0) { throw 'Windows preflight matched a different directory' }
}
Write-Host 'Native Bash selection and Windows process preflight passed'

# Exercise actual tool selection with a dependency path containing spaces and
# a misleading newer directory, preserving explicit user overrides
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$fixture = Join-Path $repoRoot ('android/temp/java environment-' + [guid]::NewGuid().ToString('N'))
$oldJavaHome = $env:JAVA_HOME
$oldPath = $env:PATH
try {
    $deps = Join-Path $fixture 'managed tools'
    $javaName = (Get-RegressionHostExecutableNames -BaseName 'java')[0]
    foreach ($major in @(21, 99)) {
        $bin = Join-Path $deps "jdk-$major/bin"
        New-Item -ItemType Directory -Path $bin -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $bin $javaName) -Value 'fixture'
    }
    $configDir = Join-Path $fixture 'android/get_deps'
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $configDir 'tool_versions.conf') -Value 'JDK_MAJOR=21'
    Set-Content -LiteralPath (Join-Path $fixture 'dependency_base.txt') -Value $deps
    $env:JAVA_HOME = $null
    Initialize-RegressionJavaEnvironment -RepoRoot $fixture
    $expected = Join-Path $deps 'jdk-21'
    if ($env:JAVA_HOME -ne $expected) { throw 'Java selection ignored the configured major' }
    Initialize-RegressionJavaEnvironment -RepoRoot $fixture
    $parts = @($env:PATH -split [regex]::Escape([IO.Path]::PathSeparator))
    $bin = Join-Path $expected 'bin'
    if ($parts[0] -ne $bin -or @($parts | Where-Object { $_ -eq $bin }).Count -ne 1) {
        throw 'Java PATH setup was not portable and idempotent'
    }
    $env:JAVA_HOME = Join-Path $deps 'jdk-99'
    Initialize-RegressionJavaEnvironment -RepoRoot $fixture
    if ($env:JAVA_HOME -ne (Join-Path $deps 'jdk-99')) { throw 'Explicit JAVA_HOME was replaced' }
    $env:JAVA_HOME = $null
    Remove-Item -LiteralPath $expected -Recurse -Force
    $rejected = $false
    try { Initialize-RegressionJavaEnvironment -RepoRoot $fixture } catch { $rejected = $true }
    if (-not $rejected -or $env:JAVA_HOME) { throw 'Missing configured Java silently selected another major' }
    Initialize-RegressionJavaEnvironment -RepoRoot $fixture -Optional
    Write-Host 'Pinned Java selection, override, PATH, and missing-tool tests passed'
} finally {
    $env:JAVA_HOME = $oldJavaHome
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}

# Run the real movie/mission fixture generator through ordinary host Python
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for the host utility integration test' }
$pythonPrefix = @($python.PrefixArguments)
$fixture = Join-Path $repoRoot ('android/temp/python-fixture-' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixture -DirectoryPrefix 'python-fixture-' -MinimumFreeSpaceGB 0.01
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    $library = Join-Path $fixture 'test movies.mvl'
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('DMVL'))
        $writer.Write([int]1)
        $name = [byte[]]::new(13)
        [Text.Encoding]::ASCII.GetBytes('end.mve').CopyTo($name, 0)
        $writer.Write($name)
        $writer.Write([int]5)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('movie'))
        [IO.File]::WriteAllBytes($library, $stream.ToArray())
    } finally { $writer.Dispose(); $stream.Dispose() }
    $output = Join-Path $fixture 'output with spaces'
    & $python.Path @pythonPrefix (Join-Path $repoRoot 'android/tests/prepare_coop_endgame_fixture.py') --output $output --movie-library $library
    if ($LASTEXITCODE -ne 0) { throw 'Host Python fixture generator failed' }
    if ((Get-Content -LiteralPath (Join-Path $output 'end.mve') -Raw) -cne 'movie' -or
        -not (Test-Path -LiteralPath (Join-Path $output 'coopend.hog'))) {
        throw 'Host Python fixture generator lost arguments or output'
    }
    Write-Host 'Host Python resolution and real movie/mission fixture generation passed'
} finally {
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
