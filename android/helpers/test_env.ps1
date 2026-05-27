#!/usr/bin/env pwsh
# test_env.ps1 -- Shared environment setup for test scripts.
#
# Dot-source this to resolve JAVA_HOME, cmake, cargo, and other tool paths.
# Safe to source multiple times (idempotent).
#
# Usage:
#   . "$PSScriptRoot\helpers\test_env.ps1"  # from android/
#   . "$PSScriptRoot\..\helpers\test_env.ps1" # from android/tests/

# -- Resolve repo root + DEP_BASE if not already set -------------------------

if (-not (Test-Path variable:script:_testEnvLoaded) -or -not $script:_testEnvLoaded) {
    $script:_testEnvLoaded = $true

    # Find repo root from this file's location (android/helpers/test_env.ps1 -> repo root)
    $_envScriptDir = $PSScriptRoot
    if (-not $_envScriptDir) { $_envScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
    . (Join-Path $_envScriptDir "test_host_platform.ps1")
    $_envAndroidRoot = Split-Path $_envScriptDir
    $_envRepoRoot = Split-Path $_envAndroidRoot

    $_depBaseFile = Join-Path $_envRepoRoot "dependency_base.txt"
    if (Test-Path $_depBaseFile) {
        $script:_ENV_DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
    }

    # -- JAVA_HOME ---------------------------------------------------------------

    if (-not $env:JAVA_HOME) {
        $found = $false
        # 1. Check DEP_BASE\jdk-* (project convention)
        if ($script:_ENV_DEP_BASE) {
            $jdk = Get-ChildItem (Join-RegressionPath $script:_ENV_DEP_BASE "jdk-*") -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending | Select-Object -First 1
            if ($jdk) {
                $env:JAVA_HOME = $jdk.FullName
                $found = $true
            }
        }
        # 2. Check Android Studio bundled JBR
        if (-not $found -and (Test-RegressionWindowsHost)) {
            $studioJbr = Join-RegressionPath $env:ProgramFiles "Android" "Android Studio" "jbr"
            if (Test-Path (Join-RegressionPath $studioJbr "bin" "java.exe")) {
                $env:JAVA_HOME = $studioJbr
                $found = $true
            }
        }
        # 3. Check registry (Oracle/Adoptium)
        if (-not $found -and (Test-RegressionWindowsHost)) {
            foreach ($regPath in @(
                    "HKLM:\SOFTWARE\JavaSoft\JDK",
                    "HKLM:\SOFTWARE\Eclipse Adoptium\JDK"
                )) {
                if (Test-Path $regPath) {
                    $ver = Get-ChildItem $regPath -ErrorAction SilentlyContinue |
                        Sort-Object PSChildName -Descending | Select-Object -First 1
                    if ($ver) {
                        $javaHome = (Get-ItemProperty $ver.PSPath -ErrorAction SilentlyContinue).JavaHome
                        if ($javaHome -and (Test-Path (Join-RegressionPath $javaHome "bin" "java.exe"))) {
                            $env:JAVA_HOME = $javaHome
                            $found = $true
                            break
                        }
                    }
                }
            }
        }
    }

    # -- CMAKE -------------------------------------------------------------------

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $cmakeCandidates = @()
        if (Test-RegressionWindowsHost) {
            $cmakeCandidates += @(
                (Join-RegressionPath $env:ProgramFiles "CMake" "bin"),
                (Join-RegressionPath ${env:ProgramFiles(x86)} "CMake" "bin"),
                (Join-RegressionPath $env:ProgramFiles "Microsoft Visual Studio" "2022" "Community" "Common7" "IDE" "CommonExtensions" "Microsoft" "CMake" "CMake" "bin"),
                (Join-RegressionPath $env:ProgramFiles "Microsoft Visual Studio" "2022" "Professional" "Common7" "IDE" "CommonExtensions" "Microsoft" "CMake" "CMake" "bin"),
                (Join-RegressionPath $env:ProgramFiles "Microsoft Visual Studio" "2022" "BuildTools" "Common7" "IDE" "CommonExtensions" "Microsoft" "CMake" "CMake" "bin")
            )
        }
        # Also check Android SDK cmake (multiple versions, pick newest)
        if ($script:_ENV_DEP_BASE) {
            $sdkCmake = Get-ChildItem (Join-RegressionPath $script:_ENV_DEP_BASE "android-sdk" "cmake" "*" "bin") -Directory -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending | Select-Object -First 1
            if ($sdkCmake) { $cmakeCandidates = @($sdkCmake.FullName) + $cmakeCandidates }
        }
        $cmakeName = (Get-RegressionHostExecutableNames -BaseName "cmake")[0]
        foreach ($dir in $cmakeCandidates) {
            if ($dir -and (Test-Path (Join-RegressionPath $dir $cmakeName))) {
                $env:PATH = "$dir$([System.IO.Path]::PathSeparator)$env:PATH"
                break
            }
        }
    }

    if (-not $env:CMAKE_GENERATOR -and (Get-Command ninja -ErrorAction SilentlyContinue)) {
        $env:CMAKE_GENERATOR = "Ninja"
    }

    # -- CARGO -------------------------------------------------------------------

    if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
        $cargoDir = Join-RegressionPath (Get-RegressionHomeDirectory) ".cargo" "bin"
        $cargoName = (Get-RegressionHostExecutableNames -BaseName "cargo")[0]
        if (Test-Path (Join-RegressionPath $cargoDir $cargoName)) {
            $env:PATH = "$cargoDir$([System.IO.Path]::PathSeparator)$env:PATH"
        }
    }
}
