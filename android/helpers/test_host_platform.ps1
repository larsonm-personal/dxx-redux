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

    function Get-RegressionDependencyBase {
        param([string]$RepoRoot)

        if (-not $RepoRoot) {
            return $null
        }
        $depBaseFile = Join-RegressionPath $RepoRoot "dependency_base.txt"
        if (-not (Test-Path -LiteralPath $depBaseFile -PathType Leaf)) {
            return $null
        }

        $firstLine = Get-Content -LiteralPath $depBaseFile -First 1
        if (-not $firstLine) {
            return $null
        }
        return $firstLine.Trim()
    }

    function Get-RegressionAndroidSdkCMakeBinDirs {
        param([string]$DepBase)

        if (-not $DepBase) {
            return @()
        }
        $sdkCMakeRoot = Join-RegressionPath $DepBase "android-sdk" "cmake"
        if (-not (Test-Path -LiteralPath $sdkCMakeRoot -PathType Container)) {
            return @()
        }

        return @(
            Get-ChildItem -LiteralPath $sdkCMakeRoot -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object { Join-RegressionPath $_.FullName "bin" } |
                Where-Object {
                    $cmakeName = (Get-RegressionHostExecutableNames -BaseName "cmake")[0]
                    Test-Path -LiteralPath (Join-RegressionPath $_ $cmakeName) -PathType Leaf
                }
        )
    }

    function Resolve-RegressionCMakePath {
        param(
            [string]$RepoRoot,
            [string]$BuildDir
        )

        $cmakeName = (Get-RegressionHostExecutableNames -BaseName "cmake")[0]
        if ($BuildDir) {
            $cachePath = Join-RegressionPath $BuildDir "CMakeCache.txt"
            if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
                $cacheCommand = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_COMMAND:INTERNAL=(.+)$' |
                    Select-Object -First 1
                if ($cacheCommand) {
                    $cachedPath = $cacheCommand.Matches[0].Groups[1].Value
                    if ($cachedPath -and -not $cachedPath.EndsWith("-NOTFOUND") -and
                        (Test-Path -LiteralPath $cachedPath -PathType Leaf)) {
                        return (Resolve-Path -LiteralPath $cachedPath).Path
                    }
                }
            }
        }

        $command = Get-Command $cmakeName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) {
            return $command.Source
        }

        $candidateDirs = @()
        $candidateDirs += Get-RegressionAndroidSdkCMakeBinDirs (Get-RegressionDependencyBase -RepoRoot $RepoRoot)
        if (Test-RegressionWindowsHost) {
            $vsRoots = @(
                (Join-RegressionPath $env:ProgramFiles "Microsoft Visual Studio"),
                (Join-RegressionPath ${env:ProgramFiles(x86)} "Microsoft Visual Studio")
            )
            foreach ($vsRoot in $vsRoots) {
                if (-not (Test-Path -LiteralPath $vsRoot -PathType Container)) {
                    continue
                }
                foreach ($yearDir in Get-ChildItem -LiteralPath $vsRoot -Directory -ErrorAction SilentlyContinue) {
                    foreach ($editionDir in Get-ChildItem -LiteralPath $yearDir.FullName -Directory -ErrorAction SilentlyContinue) {
                        $candidateDirs += Join-RegressionPath $editionDir.FullName "Common7" "IDE" "CommonExtensions" "Microsoft" "CMake" "CMake" "bin"
                    }
                }
            }
            $candidateDirs += @(
                (Join-RegressionPath $env:ProgramFiles "CMake" "bin"),
                (Join-RegressionPath ${env:ProgramFiles(x86)} "CMake" "bin")
            )
        }

        foreach ($dir in ($candidateDirs | Where-Object { $_ } | Select-Object -Unique)) {
            $candidate = Join-RegressionPath $dir $cmakeName
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
        return $null
    }

    function Get-RegressionVcpkgTripletForVcVarsArch {
        param([Parameter(Mandatory)][string]$Arch)

        switch ($Arch) {
            "x86" { return "x86-windows" }
            "x64" { return "x64-windows" }
            "arm" { return "arm-windows" }
            "arm64" { return "arm64-windows" }
            "x86_x64" { return "x64-windows" }
            "x64_x86" { return "x86-windows" }
            "x86_arm" { return "arm-windows" }
            "x64_arm" { return "arm-windows" }
            "x86_arm64" { return "arm64-windows" }
            "x64_arm64" { return "arm64-windows" }
            default { throw "Cannot map vcvars architecture '$Arch' to a vcpkg triplet" }
        }
    }

    function Test-RegressionCMakeCacheNeedsFreshConfigure {
        param(
            [Parameter(Mandatory)][string]$BuildDir,
            [Parameter(Mandatory)][string]$ExpectedTriplet
        )

        $cachePath = Join-RegressionPath $BuildDir "CMakeCache.txt"
        if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
            return $false
        }

        $cacheText = Get-Content -Raw -LiteralPath $cachePath
        if ($cacheText -match "VCPKG_TARGET_TRIPLET:STRING=([^\r\n]+)" -and $matches[1] -ne $ExpectedTriplet) {
            Write-Host "Refreshing CMake cache: vcpkg triplet is '$($matches[1])', expected '$ExpectedTriplet'"
            return $true
        }
        if ($cacheText -match "SDL_MIXER_(INCLUDE_DIR|LIBRARY):[^=]+=SDL_MIXER_[^\r\n]+-NOTFOUND") {
            Write-Host "Refreshing CMake cache: stale SDL_mixer NOTFOUND entries detected"
            return $true
        }
        if ($cacheText -match "CMAKE_MAKE_PROGRAM:FILEPATH=CMAKE_MAKE_PROGRAM-NOTFOUND") {
            Write-Host "Refreshing CMake cache: stale Ninja NOTFOUND entry detected"
            return $true
        }

        return $false
    }

    function Invoke-RegressionCMakeConfigure {
        param(
            [Parameter(Mandatory)][string]$CMakePath,
            [Parameter(Mandatory)][string]$Preset,
            [Parameter(Mandatory)][string]$BuildType,
            [Parameter(Mandatory)][string]$SourceDir,
            [Parameter(Mandatory)][string]$BuildDir,
            [Parameter(Mandatory)][string]$Triplet,
            [switch]$Fresh
        )

        $cmakeArgs = @()
        if ($Fresh) {
            $cmakeArgs += "--fresh"
        }
        $cmakeArgs += "--preset=$Preset"
        $cmakeArgs += "-D"
        $cmakeArgs += "CMAKE_BUILD_TYPE=$BuildType"
        $cmakeArgs += "-D"
        $cmakeArgs += "VCPKG_TARGET_TRIPLET=$Triplet"
        $cmakeArgs += "-S"
        $cmakeArgs += $SourceDir
        $cmakeArgs += "-B"
        $cmakeArgs += $BuildDir

        & $CMakePath @cmakeArgs
        $script:LastRegressionCMakeConfigureExitCode = $LASTEXITCODE
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

    function Resolve-RegressionOpenSslTool {
        param([string]$DepBase)

        $envValue = [Environment]::GetEnvironmentVariable("OPENSSL")
        if ($envValue -and (Test-Path -LiteralPath $envValue -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $envValue).Path
        }

        foreach ($toolName in (Get-RegressionHostExecutableNames -BaseName "openssl")) {
            $command = Get-Command $toolName -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($command) {
                return $command.Source
            }
        }

        if ($DepBase) {
            foreach ($candidate in @(
                    (Join-RegressionPath $DepBase "git" "usr" "bin" "openssl.exe"),
                    (Join-RegressionPath $DepBase "openssl" "bin" "openssl.exe"),
                    (Join-RegressionPath $DepBase "openssl" "bin" "openssl")
                )) {
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    return (Resolve-Path -LiteralPath $candidate).Path
                }
            }
        }

        return "openssl"
    }

    function Test-RegressionTcpPortListening {
        param(
            [string]$Address = "127.0.0.1",
            [Parameter(Mandatory)][int]$Port
        )

        $client = $null
        try {
            $client = [System.Net.Sockets.TcpClient]::new()
            $connect = $client.BeginConnect($Address, $Port, $null, $null)
            if (-not $connect.AsyncWaitHandle.WaitOne(500)) {
                return $false
            }
            $client.EndConnect($connect)
            return $true
        } catch {
            return $false
        } finally {
            if ($client) {
                try { $client.Close() } catch {}
            }
        }
    }

    function Get-RegressionPortOwningProcessIds {
        param(
            [Parameter(Mandatory)][int]$Port,
            [ValidateSet("TCP", "UDP")][string]$Protocol = "TCP"
        )

        $pids = @()
        if (Test-RegressionWindowsHost) {
            if ($Protocol -eq "TCP") {
                $cmd = Get-Command Get-NetTCPConnection -ErrorAction SilentlyContinue
                if ($cmd) {
                    $pids += Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
                        ForEach-Object { $_.OwningProcess }
                }
            } else {
                $cmd = Get-Command Get-NetUDPEndpoint -ErrorAction SilentlyContinue
                if ($cmd) {
                    $pids += Get-NetUDPEndpoint -LocalPort $Port -ErrorAction SilentlyContinue |
                        ForEach-Object { $_.OwningProcess }
                }
            }
        } else {
            $ss = Get-Command ss -ErrorAction SilentlyContinue
            if ($ss) {
                $stateFlag = if ($Protocol -eq "TCP") { "-ltnp" } else { "-lunp" }
                $filter = "sport = :$Port"
                $output = & $ss.Source -H $stateFlag $filter 2>$null
                foreach ($line in $output) {
                    foreach ($match in [regex]::Matches($line, 'pid=(\d+)')) {
                        $pids += [int]$match.Groups[1].Value
                    }
                }
            }

            if ($pids.Count -eq 0) {
                $lsof = Get-Command lsof -ErrorAction SilentlyContinue
                if ($lsof) {
                    $args = if ($Protocol -eq "TCP") {
                        @("-tiTCP:$Port", "-sTCP:LISTEN")
                    } else {
                        @("-tiUDP:$Port")
                    }
                    $output = & $lsof.Source @args 2>$null
                    foreach ($line in $output) {
                        if ($line -match '^\d+$') {
                            $pids += [int]$line
                        }
                    }
                }
            }
        }

        return @($pids | Where-Object { $_ -and $_ -ne $PID } | Select-Object -Unique)
    }

    function Stop-RegressionProcessesListeningOnPorts {
        param(
            [Parameter(Mandatory)][int[]]$Ports,
            [int]$WaitSeconds = 10
        )

        $killedPids = @{}
        foreach ($port in $Ports) {
            foreach ($protocol in @("TCP", "UDP")) {
                foreach ($ownerPid in @(Get-RegressionPortOwningProcessIds -Port $port -Protocol $protocol)) {
                    if ($killedPids.ContainsKey($ownerPid)) {
                        continue
                    }
                    Write-Status "Killing existing $protocol process on port $port (PID $ownerPid)..."
                    try {
                        Stop-Process -Id $ownerPid -Force -ErrorAction SilentlyContinue
                        $killedPids[$ownerPid] = $true
                    } catch {}
                }
            }
        }

        if ($killedPids.Count -eq 0) {
            return
        }

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt $WaitSeconds) {
            $busy = $false
            foreach ($port in $Ports) {
                if (Test-RegressionTcpPortListening -Port $port) {
                    $busy = $true
                    break
                }
                $udpOwners = @(Get-RegressionPortOwningProcessIds -Port $port -Protocol UDP)
                if ($udpOwners.Count -gt 0) {
                    $busy = $true
                    break
                }
            }
            if (-not $busy) {
                return
            }
            Start-Sleep -Seconds 1
        }

        Write-Status "WARNING: One or more server ports are still in use after ${WaitSeconds}s" "Yellow"
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

    function Invoke-RegressionHostBuild {
        param(
            [Parameter(Mandatory)][string]$RepoRoot,
            [Parameter(Mandatory)][string]$Target,
            [string]$Label = $Target
        )

        $originalLocation = Get-Location
        if (Test-RegressionWindowsHost) {
            $buildScript = Join-RegressionPath $RepoRoot "run-windows-build.ps1"
            if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
                throw "Host build script not found: $buildScript"
            }

            try {
                & $buildScript -Target $Target
                if ($LASTEXITCODE -eq 0) {
                    return
                }
            } catch {
                $message = [string]$_
                if ($message -notmatch 'Supported values:\s*([A-Za-z0-9_,\s-]+)') {
                    throw
                }

                $supportedArchList = @($matches[1].Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
                if ($supportedArchList.Count -eq 0) {
                    throw
                }

                $fallbackArch = $supportedArchList[0]
                Write-Host "Build guardrail: retrying host build with -VcVarsArch $fallbackArch"
                & $buildScript -Target $Target -VcVarsArch $fallbackArch
                if ($LASTEXITCODE -ne 0) {
                    throw "Host build failed for $Label with exit code $LASTEXITCODE (fallback arch: $fallbackArch)"
                }
                return
            } finally {
                Set-Location -LiteralPath $originalLocation.Path
            }

            throw "Host build failed for $Label with exit code $LASTEXITCODE"
        }

        $buildScript = Join-RegressionPath $RepoRoot "run-linux-build.sh"
        if (-not (Test-Path -LiteralPath $buildScript -PathType Leaf)) {
            throw "Host build script not found: $buildScript"
        }
        $bash = Get-Command bash -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $bash) {
            throw "bash was not found on PATH"
        }

        & $bash.Source $buildScript --target $Target
        if ($LASTEXITCODE -ne 0) {
            throw "Host build failed for $Label with exit code $LASTEXITCODE"
        }
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
