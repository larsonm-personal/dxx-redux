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
    if ($env:JAVA_HOME) {
        $javaBin = Join-RegressionPath $env:JAVA_HOME "bin"
        $javaName = (Get-RegressionHostExecutableNames -BaseName "java")[0]
        if (Test-Path (Join-RegressionPath $javaBin $javaName)) {
            $pathParts = @($env:PATH -split [regex]::Escape([System.IO.Path]::PathSeparator) | Where-Object { $_ })
            if (-not ($pathParts | Where-Object { $_ -eq $javaBin })) {
                $env:PATH = "$javaBin$([System.IO.Path]::PathSeparator)$env:PATH"
            }
        }
    }

    # -- CMAKE -------------------------------------------------------------------

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        $cmakePath = Resolve-RegressionCMakePath -RepoRoot $_envRepoRoot
        if ($cmakePath) {
            $cmakeDir = Split-Path -Parent $cmakePath
            $env:PATH = "$cmakeDir$([System.IO.Path]::PathSeparator)$env:PATH"
        }
    }

    if (-not $env:CMAKE_GENERATOR -and (Get-Command ninja -ErrorAction SilentlyContinue)) {
        $env:CMAKE_GENERATOR = "Ninja"
    }

    # -- CARGO -------------------------------------------------------------------

    $cargoDir = Join-RegressionPath (Get-RegressionHomeDirectory) ".cargo" "bin"
    $cargoName = (Get-RegressionHostExecutableNames -BaseName "cargo")[0]
    if (Test-Path (Join-RegressionPath $cargoDir $cargoName)) {
        $pathParts = @($env:PATH -split [regex]::Escape([System.IO.Path]::PathSeparator) | Where-Object { $_ })
        if (-not ($pathParts | Where-Object { $_ -eq $cargoDir })) {
            $env:PATH = "$cargoDir$([System.IO.Path]::PathSeparator)$env:PATH"
        }
    }
}
