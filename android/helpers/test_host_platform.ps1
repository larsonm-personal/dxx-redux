#!/usr/bin/env pwsh
# test_host_platform.ps1 -- Host OS helpers for Android regression scripts

if (-not (Test-Path variable:script:_testHostPlatformLoaded) -or -not $script:_testHostPlatformLoaded) {
    $script:_testHostPlatformLoaded = $true

    function Test-RegressionWindowsHost {
        return [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
            [System.Runtime.InteropServices.OSPlatform]::Windows)
    }

    function Join-RegressionPath {
        param(
            [Parameter(Mandatory)][string]$Root,
            [Parameter(ValueFromRemainingArguments = $true)][string[]]$Segments
        )

        $path = $Root
        foreach ($segment in $Segments) {
            if ([string]::IsNullOrWhiteSpace($segment)) {
                continue
            }
            $path = Join-Path $path $segment
        }
        return $path
    }

    function Get-RegressionHostExecutableNames {
        param([Parameter(Mandatory)][string]$BaseName)

        if (Test-RegressionWindowsHost) {
            return @("$BaseName.exe", $BaseName)
        }
        return @($BaseName, "$BaseName.exe")
    }

    function Get-RegressionHomeDirectory {
        if ($env:USERPROFILE) {
            return $env:USERPROFILE
        }
        if ($env:HOME) {
            return $env:HOME
        }
        return [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)
    }

    function Get-RegressionCurrentPwshPath {
        try {
            $current = Get-Process -Id $PID -ErrorAction Stop
            if ($current.Path) {
                return $current.Path
            }
        } catch {}

        $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
        if ($pwsh) {
            return $pwsh.Source
        }
        return "pwsh"
    }

    function Resolve-RegressionAndroidSdkTool {
        param(
            [string]$DepBase,
            [Parameter(Mandatory)][string]$Subdir,
            [Parameter(Mandatory)][string]$ToolName,
            [string]$EnvironmentVariable
        )

        $toolNames = @(Get-RegressionHostExecutableNames -BaseName $ToolName)
        if ($EnvironmentVariable) {
            $envValue = [Environment]::GetEnvironmentVariable($EnvironmentVariable)
            if ($envValue -and (Test-Path -LiteralPath $envValue -PathType Leaf)) {
                return (Resolve-Path -LiteralPath $envValue).Path
            }
        }

        $sdkRoots = @()
        if ($env:ANDROID_HOME) { $sdkRoots += $env:ANDROID_HOME }
        if ($env:ANDROID_SDK_ROOT) { $sdkRoots += $env:ANDROID_SDK_ROOT }
        if ($DepBase) { $sdkRoots += (Join-RegressionPath $DepBase "android-sdk") }
        $sdkRoots = @($sdkRoots | Where-Object { $_ } | Select-Object -Unique)

        foreach ($sdkRoot in $sdkRoots) {
            foreach ($toolName in $toolNames) {
                $candidate = Join-RegressionPath $sdkRoot $Subdir $toolName
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }

        foreach ($toolName in $toolNames) {
            $command = Get-Command $toolName -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($command) {
                return $command.Source
            }
        }

        $fallbackRoot = if ($sdkRoots.Count -gt 0) { $sdkRoots[0] } else { "" }
        if ($fallbackRoot) {
            return Join-RegressionPath $fallbackRoot $Subdir $toolNames[0]
        }
        return $toolNames[0]
    }

    function Resolve-RegressionBuildTool {
        param(
            [Parameter(Mandatory)][string]$Directory,
            [Parameter(Mandatory)][string]$BaseName
        )

        foreach ($toolName in (Get-RegressionHostExecutableNames -BaseName $BaseName)) {
            $candidate = Join-RegressionPath $Directory $toolName
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
        return $null
    }

    function Resolve-RegressionGradleWrapper {
        param([Parameter(Mandatory)][string]$AndroidDir)

        $wrapperNames = if (Test-RegressionWindowsHost) {
            @("gradlew.bat", "gradlew")
        } else {
            @("gradlew", "gradlew.bat")
        }

        foreach ($wrapperName in $wrapperNames) {
            $candidate = Join-RegressionPath $AndroidDir $wrapperName
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
        return (Join-RegressionPath $AndroidDir $wrapperNames[0])
    }

    function Resolve-RegressionCommandPath {
        param([Parameter(Mandatory)][string]$CommandText)

        if ([string]::IsNullOrWhiteSpace($CommandText) -or $CommandText.EndsWith("-NOTFOUND")) {
            return $null
        }

        $command = $CommandText.Trim('"')
        if ([System.IO.Path]::IsPathRooted($command)) {
            if (Test-Path -LiteralPath $command -PathType Leaf) {
                return (Resolve-Path -LiteralPath $command).Path
            }
            return $null
        }

        $found = Get-Command $command -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.Source
        }
        return $null
    }

    function Reset-RegressionCMakeBuildIfMissingTool {
        param([Parameter(Mandatory)][string]$BuildDir)

        $cachePath = Join-RegressionPath $BuildDir "CMakeCache.txt"
        if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
            return $false
        }

        $missing = @()
        foreach ($line in (Get-Content -LiteralPath $cachePath -ErrorAction SilentlyContinue)) {
            if ($line -notmatch '^(CMAKE_(C|CXX)_COMPILER|CMAKE_MAKE_PROGRAM):[^=]*=(.+)$') {
                continue
            }

            $toolPath = $Matches[3].Trim()
            if (-not (Resolve-RegressionCommandPath -CommandText $toolPath)) {
                $missing += "$($Matches[1])=$toolPath"
            }
        }

        if ($missing.Count -eq 0) {
            return $false
        }

        Write-Host "CMake cache references missing tool(s); deleting stale build dir" -ForegroundColor Yellow
        foreach ($entry in $missing) {
            Write-Host "  $entry" -ForegroundColor Yellow
        }
        Remove-Item -Recurse -Force -Confirm:$false -LiteralPath $BuildDir
        return $true
    }
}
