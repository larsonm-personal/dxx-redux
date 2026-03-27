#!/usr/bin/env pwsh
# test_env.ps1 -- Shared environment setup for test scripts.
#
# Dot-source this to resolve JAVA_HOME, cmake, cargo, and other tool paths.
# Safe to source multiple times (idempotent).
#
# Usage:
#   . "$PSScriptRoot\test_env.ps1"          # from android/
#   . "$PSScriptRoot\..\test_env.ps1"       # from android/tests/

# -- Resolve repo root + DEP_BASE if not already set -------------------------

if (-not (Test-Path variable:script:_testEnvLoaded) -or -not $script:_testEnvLoaded) {
    $script:_testEnvLoaded = $true

    # Find repo root from this file's location (android/test_env.ps1 -> repo root)
    $_envScriptDir = $PSScriptRoot
    if (-not $_envScriptDir) { $_envScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
    $_envRepoRoot = Split-Path $_envScriptDir

    $_depBaseFile = Join-Path $_envRepoRoot "dependency_base.txt"
    if (Test-Path $_depBaseFile) {
        $script:_ENV_DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
    }

    # -- JAVA_HOME ---------------------------------------------------------------

    if (-not $env:JAVA_HOME) {
        $found = $false
        # 1. Check DEP_BASE\jdk-* (project convention)
        if ($script:_ENV_DEP_BASE) {
            $jdk = Get-ChildItem "$($script:_ENV_DEP_BASE)\jdk-*" -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending | Select-Object -First 1
            if ($jdk) {
                $env:JAVA_HOME = $jdk.FullName
                $found = $true
            }
        }
        # 2. Check Android Studio bundled JBR
        if (-not $found) {
            $studioJbr = "$env:ProgramFiles\Android\Android Studio\jbr"
            if (Test-Path "$studioJbr\bin\java.exe") {
                $env:JAVA_HOME = $studioJbr
                $found = $true
            }
        }
        # 3. Check registry (Oracle/Adoptium)
        if (-not $found) {
            foreach ($regPath in @(
                    "HKLM:\SOFTWARE\JavaSoft\JDK",
                    "HKLM:\SOFTWARE\Eclipse Adoptium\JDK"
                )) {
                if (Test-Path $regPath) {
                    $ver = Get-ChildItem $regPath -ErrorAction SilentlyContinue |
                        Sort-Object PSChildName -Descending | Select-Object -First 1
                    if ($ver) {
                        $javaHome = (Get-ItemProperty $ver.PSPath -ErrorAction SilentlyContinue).JavaHome
                        if ($javaHome -and (Test-Path "$javaHome\bin\java.exe")) {
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
        # Search common Windows locations
        $cmakeCandidates = @(
            "$env:ProgramFiles\CMake\bin",
            "${env:ProgramFiles(x86)}\CMake\bin",
            "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
            "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
            "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
        )
        # Also check Android SDK cmake (multiple versions, pick newest)
        if ($script:_ENV_DEP_BASE) {
            $sdkCmake = Get-ChildItem "$($script:_ENV_DEP_BASE)\android-sdk\cmake\*\bin" -Directory -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending | Select-Object -First 1
            if ($sdkCmake) { $cmakeCandidates = @($sdkCmake.FullName) + $cmakeCandidates }
        }
        foreach ($dir in $cmakeCandidates) {
            if (Test-Path "$dir\cmake.exe") {
                $env:PATH = "$dir;$env:PATH"
                break
            }
        }
    }

    # -- CARGO -------------------------------------------------------------------

    if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
        $cargoDir = "$env:USERPROFILE\.cargo\bin"
        if (Test-Path "$cargoDir\cargo.exe") {
            $env:PATH = "$cargoDir;$env:PATH"
        }
    }
}
