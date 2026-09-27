#!/usr/bin/env pwsh
function Get-HostPlatform {
    if ($env:OS -eq "Windows_NT" -or (Get-Variable IsWindows -ValueOnly -ErrorAction SilentlyContinue)) { return "Windows" }
    if (Get-Variable IsLinux -ValueOnly -ErrorAction SilentlyContinue) { return "Linux" }
    if (Get-Variable IsMacOS -ValueOnly -ErrorAction SilentlyContinue) { return "MacOS" }
    return "Unknown"
}

function Get-HomeDirectory {
    if ($env:HOME) {
        return $env:HOME
    }

    $userHome = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)
    if ($userHome) {
        return $userHome
    }

    return "~"
}

# Dependency shell scripts require Bash, and Windows installs require MSYS/Git
# Bash rather than the WSL launcher (which would select Linux downloads)
function Get-BashCommandPath {
    $windowsHost = (Get-HostPlatform) -eq 'Windows'
    $candidates = @(Get-Command bash -CommandType Application -All -ErrorAction SilentlyContinue | ForEach-Object { $_.Source })
    if ($windowsHost) {
        $git = Get-Command git -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($git) {
            $gitRoot = Split-Path (Split-Path $git.Source -Parent) -Parent
            $candidates += Join-Path $gitRoot 'bin/bash.exe'
        }
        foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:LOCALAPPDATA)) {
            if ($base) { $candidates += Join-Path $base 'Git/bin/bash.exe' }
        }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate)) { continue }
        # Do not launch WSL just to discover it is the wrong host
        if ($windowsHost -and $env:WINDIR -and $candidate.StartsWith($env:WINDIR + '\', [StringComparison]::OrdinalIgnoreCase)) { continue }
        $hostName = @(& $candidate -c 'test -n "$BASH_VERSION" && uname -s' 2>$null)
        if ($LASTEXITCODE -ne 0) { continue }
        if (-not $windowsHost -or ($hostName -match '^(MINGW|MSYS|CYGWIN|.*_NT)')) { return $candidate }
    }
    throw 'Native Bash was not found; install Git for Windows on Windows or Bash on Linux/macOS'
}

function Get-DefaultDependencyBase {
    switch (Get-HostPlatform) {
        "Windows" { return "C:\local" }
        default { return (Join-Path (Get-HomeDirectory) "local") }
    }
}

function Get-DependencyBaseFilePath {
    param([string]$RepoRoot)

    return (Join-Path $RepoRoot "dependency_base.txt")
}

function Get-DependencyBase {
    param(
        [string]$RepoRoot,
        [switch]$CreateIfMissing
    )

    $dependencyBaseFile = Get-DependencyBaseFilePath -RepoRoot $RepoRoot
    if (Test-Path -LiteralPath $dependencyBaseFile) {
        $value = (Get-Content -LiteralPath $dependencyBaseFile -First 1).Trim()
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            New-Item -ItemType Directory -Force -Path $value | Out-Null
            return $value
        }
    }

    if (-not $CreateIfMissing) {
        return $null
    }

    $defaultBase = Get-DefaultDependencyBase
    Set-Content -LiteralPath $dependencyBaseFile -Value $defaultBase
    New-Item -ItemType Directory -Force -Path $defaultBase | Out-Null
    return $defaultBase
}

function Get-CheckUpdatesInvocation {
    if ((Get-HostPlatform) -eq "Windows") {
        return ".\\check-updates.ps1"
    }

    return "./check-updates.ps1"
}

function Get-PlatformExecutableName {
    param([string]$ToolName)

    if ((Get-HostPlatform) -eq "Windows") {
        return "$ToolName.exe"
    }

    return $ToolName
}

function Get-PlatformBatchName {
    param([string]$ToolName)

    if ((Get-HostPlatform) -eq "Windows") {
        return "$ToolName.bat"
    }

    return $ToolName
}

function Find-FirstExistingPath {
    param([string[]]$CandidatePaths)

    foreach ($candidate in $CandidatePaths) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Get-ToolPathFromPath {
    param([string[]]$CommandNames)

    foreach ($name in $CommandNames) {
        if ([string]::IsNullOrWhiteSpace($name)) {
            continue
        }

        $command = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) {
            return $command.Source
        }
    }

    return $null
}

function Get-PlatformToolPath {
    param(
        [string]$BaseDir,
        [string]$ToolName,
        [string[]]$AlternativeNames = @(),
        [switch]$UseBatch
    )

    $fileName = if ($UseBatch) {
        Get-PlatformBatchName -ToolName $ToolName
    } else {
        Get-PlatformExecutableName -ToolName $ToolName
    }

    $candidatePaths = @()
    if ($BaseDir) {
        $candidatePaths += (Join-Path $BaseDir $fileName)
        foreach ($name in $AlternativeNames) {
            $candidatePaths += (Join-Path $BaseDir $name)
        }
    }

    $foundPath = Find-FirstExistingPath -CandidatePaths $candidatePaths
    if ($foundPath) {
        return $foundPath
    }

    $pathNames = @($ToolName)
    if ((Get-HostPlatform) -eq "Windows") {
        $pathNames += $fileName
    }
    $pathNames += $AlternativeNames
    return Get-ToolPathFromPath -CommandNames @($pathNames | Where-Object { $_ } | Select-Object -Unique)
}

function Get-SdkCmdlineToolsOsToken {
    switch (Get-HostPlatform) {
        "Windows" { return "win" }
        "Linux" { return "linux" }
        "MacOS" { return "mac" }
        default { return $null }
    }
}

function Get-AdoptiumOsToken {
    switch (Get-HostPlatform) {
        "Windows" { return "windows" }
        "Linux" { return "linux" }
        "MacOS" { return "mac" }
        default { return $null }
    }
}

function Get-NdkArchiveOsToken {
    switch (Get-HostPlatform) {
        "Windows" { return "windows" }
        "Linux" { return "linux" }
        "MacOS" { return "darwin" }
        default { return $null }
    }
}
