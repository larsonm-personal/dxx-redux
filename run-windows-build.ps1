#!/usr/bin/env pwsh
param(
    [ValidateSet("both", "d1", "d2")]
    [string]$Target = "both",
    [string]$Preset = "x86-release",
    [string]$BuildType = "RelWithDebInfo",
    [switch]$Clean,
    [string]$Compiler = "auto",
    [string]$VisualStudioPath,
    [string]$VcVarsAllPath,
    [string]$VcVarsArch,
    [string]$VcVarsVersion,
    [string]$WindowsSdkVersion,
    [switch]$ListCompilers
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSCommandPath

. (Join-Path $repoRoot "android\helpers\test_host_platform.ps1")

function Add-PathDirectory($dir) {
    if (-not $dir -or -not (Test-Path $dir)) {
        return $false
    }

    $currentEntries = @($env:PATH -split ';' | Where-Object { $_ })
    if ($currentEntries -contains $dir) {
        return $true
    }

    $env:PATH = "$dir;$env:PATH"
    return $true
}

function Require-Tool($name, $hint) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "$name not found in PATH. $hint"
    }
}

function Find-ExecutablePath($name, $candidateDirs) {
    $existing = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1

    if ($existing) {
        return $existing.Source
    }

    foreach ($dir in $candidateDirs) {
        $candidate = $null

        if (-not $dir) {
            continue
        }

        $candidate = Join-Path $dir $name
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Find-VsWherePath {
    $existing = Get-Command vswhere.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    $candidates = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    if ($existing) {
        return $existing.Source
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Get-FallbackVsInstances {
    $roots = @(
        "$env:ProgramFiles\Microsoft Visual Studio",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio"
    )
    $instances = @()

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) {
            continue
        }

        foreach ($yearDir in Get-ChildItem $root -Directory -ErrorAction SilentlyContinue) {
            foreach ($editionDir in Get-ChildItem $yearDir.FullName -Directory -ErrorAction SilentlyContinue) {
                $vcvars = Join-Path $editionDir.FullName "VC\Auxiliary\Build\vcvarsall.bat"
                if (-not (Test-Path $vcvars)) {
                    continue
                }

                $instances += [PSCustomObject]@{
                    DisplayName = "Visual Studio $($yearDir.Name) $($editionDir.Name)"
                    InstallationVersion = $yearDir.Name
                    InstallationPath = $editionDir.FullName
                    VcVarsAllPath = $vcvars
                    InstanceId = "$($yearDir.Name)-$($editionDir.Name)"
                    Toolsets = @(
                        Get-ChildItem (Join-Path $editionDir.FullName "VC\Tools\MSVC") -Directory -ErrorAction SilentlyContinue |
                        Sort-Object Name -Descending |
                        Select-Object -ExpandProperty Name
                    )
                }
            }
        }
    }

    return $instances
}

function Get-VsInstances {
    $vsWhere = Find-VsWherePath
    $instances = @()

    if ($vsWhere) {
        $json = & $vsWhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json
        if ($LASTEXITCODE -eq 0 -and $json) {
            $parsed = @($json | ConvertFrom-Json)
            foreach ($item in $parsed) {
                $vcvars = Join-Path $item.installationPath "VC\Auxiliary\Build\vcvarsall.bat"
                if (-not (Test-Path $vcvars)) {
                    continue
                }

                $instances += [PSCustomObject]@{
                    DisplayName = if ($item.displayName) { $item.displayName } else { Split-Path $item.installationPath -Leaf }
                    InstallationVersion = if ($item.installationVersion) { $item.installationVersion } else { "0.0" }
                    InstallationPath = $item.installationPath
                    VcVarsAllPath = $vcvars
                    InstanceId = if ($item.instanceId) { $item.instanceId } else { Split-Path $item.installationPath -Leaf }
                    Toolsets = @(
                        Get-ChildItem (Join-Path $item.installationPath "VC\Tools\MSVC") -Directory -ErrorAction SilentlyContinue |
                        Sort-Object Name -Descending |
                        Select-Object -ExpandProperty Name
                    )
                }
            }
        }
    }

    if (-not $instances) {
        $instances = Get-FallbackVsInstances
    }

    return @(
        $instances |
        Sort-Object @{ Expression = { $_.InstallationVersion } ; Descending = $true }, @{ Expression = { $_.InstallationPath } ; Descending = $true }
    )
}

function Get-WindowsSdkVersions {
    $sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib"

    if (-not (Test-Path $sdkRoot)) {
        return @()
    }

    return @(
        Get-ChildItem $sdkRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -ExpandProperty Name
    )
}

function Get-CompilerIdBase($instance) {
    $pathParts = $instance.InstallationPath -split '\\'
    $edition = if ($pathParts.Length -ge 1) { $pathParts[-1] } else { "visualstudio" }
    $year = if ($pathParts.Length -ge 2) { $pathParts[-2] } else { "vs" }

    return ("vs{0}-{1}" -f $year, $edition.ToLowerInvariant())
}

function Get-DefaultVcVarsArch($preset) {
    if ($preset -match '^(x86|x64|arm|arm64)(?:-|$)') {
        return $matches[1]
    }

    return "x86"
}

function New-TemporaryBatchFile($lines) {
    $path = [System.IO.Path]::ChangeExtension([System.IO.Path]::GetTempFileName(), ".cmd")
    Set-Content -Path $path -Value $lines -Encoding Ascii
    return $path
}

function Invoke-VcVarsCommand($vcvarsPath, $argList, [switch]$EmitEnvironment) {
    $argsText = if ($argList) { ($argList -join ' ') } else { "" }
    $lines = @(
        "@echo off",
        "call `"$vcvarsPath`" $argsText >nul",
        "if errorlevel 1 exit /b %errorlevel%",
        "if not defined VSCMD_VER exit /b 1"
    )
    $batchFile = $null
    $output = @()

    if ($EmitEnvironment) {
        $lines += "set"
    }

    $batchFile = New-TemporaryBatchFile $lines
    try {
        $output = & cmd.exe /d /c "`"$batchFile`"" 2>&1
        if ($LASTEXITCODE -ne 0) {
            $detail = ($output | Where-Object { $_ } | Select-Object -First 10) -join [Environment]::NewLine
            throw "vcvarsall failed for '$argsText'. $detail"
        }
    } finally {
        if ($batchFile -and (Test-Path $batchFile)) {
            Remove-Item $batchFile -Force -ErrorAction SilentlyContinue
        }
    }

    return @($output)
}

$script:SupportedVcVarsArchCache = @{}

function Test-VcVarsArch($vcvarsPath, $arch) {
    try {
        Invoke-VcVarsCommand $vcvarsPath @($arch) | Out-Null
        return $true
    } catch {
        return $false
    }
}

function Get-SupportedVcVarsArches($vcvarsPath) {
    $candidateArches = @("x86", "x64", "arm", "arm64", "x86_x64", "x86_arm", "x86_arm64", "x64_x86", "x64_arm", "x64_arm64")

    if ($script:SupportedVcVarsArchCache.ContainsKey($vcvarsPath)) {
        return $script:SupportedVcVarsArchCache[$vcvarsPath]
    }

    $supported = @()
    foreach ($arch in $candidateArches) {
        if (Test-VcVarsArch $vcvarsPath $arch) {
            $supported += $arch
        }
    }

    $script:SupportedVcVarsArchCache[$vcvarsPath] = $supported
    return $supported
}

function Resolve-SelectedInstance($instances, $compilerName, $visualStudioPath) {
    if ($visualStudioPath) {
        $normalized = [System.IO.Path]::GetFullPath($visualStudioPath)
        $match = $instances | Where-Object {
            [System.IO.Path]::GetFullPath($_.InstallationPath) -eq $normalized
        } | Select-Object -First 1

        if (-not $match) {
            throw "Visual Studio installation not found: $visualStudioPath"
        }

        return $match
    }

    if (-not $compilerName -or $compilerName -eq "auto" -or $compilerName -eq "msvc") {
        return $instances | Select-Object -First 1
    }

    $compilerId = ($compilerName -split ':', 2)[0]
    $match = $instances | Where-Object {
        $baseId = Get-CompilerIdBase $_
        $baseId -eq $compilerId -or $_.InstanceId -eq $compilerId -or $_.DisplayName -eq $compilerId -or $_.InstallationPath -eq $compilerId
    } | Select-Object -First 1

    if (-not $match) {
        $choices = @($instances | ForEach-Object { Get-CompilerIdBase $_ }) -join ", "
        throw "Unknown compiler '$compilerName'. Available compiler ids: $choices"
    }

    return $match
}

function Resolve-VcVarsArch($compilerName, $explicitArch, $preset) {
    $compilerArch = $null

    if ($compilerName -and $compilerName.Contains(':')) {
        $compilerArch = ($compilerName -split ':', 2)[1]
    }

    if ($explicitArch -and $compilerArch -and $explicitArch -ne $compilerArch) {
        throw "Conflicting architecture selection: -Compiler specifies '$compilerArch' but -VcVarsArch specifies '$explicitArch'"
    }

    if ($explicitArch) {
        return $explicitArch
    }

    if ($compilerArch) {
        return $compilerArch
    }

    return Get-DefaultVcVarsArch $preset
}

function Import-VcVarsEnvironment($vcvarsPath, $arch, $toolsetVersion, $sdkVersion) {
    $argList = @($arch)
    $output = @()

    if ($sdkVersion) {
        $argList += "-winsdk=$sdkVersion"
    }
    if ($toolsetVersion) {
        $argList += "-vcvars_ver=$toolsetVersion"
    }

    $output = Invoke-VcVarsCommand $vcvarsPath $argList -EmitEnvironment
    foreach ($line in $output) {
        if ($line -notmatch '^([^=]+)=(.*)$') {
            continue
        }

        $name = $matches[1]
        $value = $matches[2]
        Set-Item -Path "Env:$name" -Value $value
    }

    return $argList
}

function Show-CompilerChoices($instances) {
    $sdkVersions = Get-WindowsSdkVersions

    if (-not $instances) {
        Write-Host "No Visual Studio instances with MSVC tools were found"
        return
    }

    Write-Host "Discovered MSVC compiler choices:"
    foreach ($instance in $instances) {
        $compilerId = Get-CompilerIdBase $instance
        $arches = @(Get-SupportedVcVarsArches $instance.VcVarsAllPath)

        Write-Host ""
        Write-Host "Compiler: $compilerId"
        Write-Host "  display: $($instance.DisplayName)"
        Write-Host "  install: $($instance.InstallationPath)"
        Write-Host "  vcvarsall: $($instance.VcVarsAllPath)"
        Write-Host "  arches: $($arches -join ', ')"
        if ($instance.Toolsets) {
            Write-Host "  toolsets: $($instance.Toolsets -join ', ')"
        }
    }

    Write-Host ""
    Write-Host "Windows SDKs: $($sdkVersions -join ', ')"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\run-windows-build.ps1 -Compiler auto"
    Write-Host "  .\run-windows-build.ps1 -Compiler $(Get-CompilerIdBase ($instances | Select-Object -First 1)):x86"
    Write-Host "  .\run-windows-build.ps1 -Compiler $(Get-CompilerIdBase ($instances | Select-Object -First 1)) -VcVarsVersion 14.40 -WindowsSdkVersion 10.0.26100.0"
}

$depBase = Get-RegressionDependencyBase -RepoRoot $repoRoot
$vsInstances = Get-VsInstances

if ($ListCompilers) {
    Show-CompilerChoices $vsInstances
    return
}

if (-not $VcVarsAllPath) {
    if (-not $vsInstances) {
        throw "No Visual Studio instances with VC tools were found. Install Visual Studio Build Tools or Visual Studio Community with Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    }

    $selectedInstance = Resolve-SelectedInstance $vsInstances $Compiler $VisualStudioPath
    $VcVarsAllPath = $selectedInstance.VcVarsAllPath
} else {
    $selectedInstance = [PSCustomObject]@{
        DisplayName = "manual-vcvarsall"
        InstallationVersion = "manual"
        InstallationPath = Split-Path (Split-Path (Split-Path $VcVarsAllPath -Parent) -Parent) -Parent
        VcVarsAllPath = $VcVarsAllPath
        InstanceId = "manual-vcvarsall"
        Toolsets = @()
    }
}

if (-not (Test-Path $VcVarsAllPath)) {
    throw "vcvarsall.bat not found at $VcVarsAllPath"
}

$resolvedArch = Resolve-VcVarsArch $Compiler $VcVarsArch $Preset
$vcpkgTriplet = Get-RegressionVcpkgTripletForVcVarsArch -Arch $resolvedArch
$supportedArches = @(Get-SupportedVcVarsArches $VcVarsAllPath)
if ($supportedArches -and $resolvedArch -notin $supportedArches) {
    throw "vcvarsall at $VcVarsAllPath does not support '$resolvedArch'. Supported values: $($supportedArches -join ', ')"
}

$vcvarsArgs = Import-VcVarsEnvironment $VcVarsAllPath $resolvedArch $VcVarsVersion $WindowsSdkVersion

$cmakeCandidateDirs = @()
$ninjaCandidateDirs = @()

$cmakeCandidateDirs += Get-RegressionAndroidSdkCMakeBinDirs -DepBase $depBase
$ninjaCandidateDirs += Get-RegressionAndroidSdkCMakeBinDirs -DepBase $depBase

if ($selectedInstance -and $selectedInstance.InstallationPath) {
    $cmakeCandidateDirs += Join-Path $selectedInstance.InstallationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    $ninjaCandidateDirs += Join-Path $selectedInstance.InstallationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
}

$cmakeCandidateDirs += @(
    "$env:ProgramFiles\CMake\bin",
    "${env:ProgramFiles(x86)}\CMake\bin"
)
$ninjaCandidateDirs += @(
    "$env:ProgramFiles\Ninja",
    "$env:ProgramFiles\CMake\bin",
    "${env:ProgramFiles(x86)}\CMake\bin"
)

$cmakePath = Find-ExecutablePath "cmake.exe" $cmakeCandidateDirs
$ninjaPath = Find-ExecutablePath "ninja.exe" $ninjaCandidateDirs

if (-not $cmakePath) {
    throw "cmake.exe not found. Install CMake or Android SDK CMake under the dependency_base.txt root"
}
if (-not $ninjaPath) {
    throw "ninja.exe not found. Install Ninja or Android SDK CMake under the dependency_base.txt root"
}

Add-PathDirectory (Split-Path $cmakePath -Parent) | Out-Null
Add-PathDirectory (Split-Path $ninjaPath -Parent) | Out-Null

Require-Tool cmake.exe "Install CMake or place Android SDK CMake under dependency_base.txt"
Require-Tool ninja.exe "Install Ninja or place Android SDK CMake under dependency_base.txt"
Require-Tool cl.exe "Install Visual Studio C++ tools or point the script at vcvarsall.bat"

Write-Host "Using compiler: $(Get-CompilerIdBase $selectedInstance)"
Write-Host "Visual Studio: $($selectedInstance.InstallationPath)"
Write-Host "vcvarsall: $VcVarsAllPath"
Write-Host "vcvars args: $($vcvarsArgs -join ' ')"
Write-Host "vcpkg triplet: $vcpkgTriplet"
Write-Host "cmake: $cmakePath"
Write-Host "ninja: $ninjaPath"

Get-Process cl -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$targets = switch ($Target) {
    "d1" { @("d1") }
    "d2" { @("d2") }
    default { @("d1", "d2") }
}

foreach ($game in $targets) {
    $buildDirName = if ($game -eq "d1") { "buildd1" } else { "buildd2" }
    $buildDir = Join-Path $repoRoot $buildDirName
    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "Configuring $game with preset $Preset ($BuildType)"
    $sourceDir = Join-Path $repoRoot $game
    $freshConfigure = Test-RegressionCMakeCacheNeedsFreshConfigure -BuildDir $buildDir -ExpectedTriplet $vcpkgTriplet
    Invoke-RegressionCMakeConfigure $cmakePath $Preset $BuildType $sourceDir $buildDir $vcpkgTriplet -Fresh:$freshConfigure
    $configureExit = $script:LastRegressionCMakeConfigureExitCode
    if ($configureExit -ne 0 -and -not $freshConfigure -and (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
        Write-Host "CMake configure failed for $game; retrying once with a fresh CMake cache"
        Invoke-RegressionCMakeConfigure $cmakePath $Preset $BuildType $sourceDir $buildDir $vcpkgTriplet -Fresh
        $configureExit = $script:LastRegressionCMakeConfigureExitCode
    }
    if ($configureExit -ne 0) {
        throw "CMake configure failed for $game"
    }

    Write-Host "Building $game"
    & $cmakePath --build $buildDir --parallel -- -k 10
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed for $game"
    }

    $sourceRevision = & git -C $repoRoot rev-parse HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $sourceRevision) {
        $stampDirectory = Join-Path $repoRoot "temp\input_demo_build_stamps"
        New-Item -ItemType Directory -Path $stampDirectory -Force | Out-Null
        [System.IO.File]::WriteAllText(
            (Join-Path $stampDirectory "$game.stamp"),
            ([string]$sourceRevision).Trim())
    }
}

Write-Host "Windows build complete for $($targets -join ', ')"
