function Get-HostPlatform {
    if ($IsWindows) { return "Windows" }
    if ($IsLinux) { return "Linux" }
    if ($IsMacOS) { return "MacOS" }
    return "Unknown"
}

function Get-HomeDirectory {
    if ($env:HOME) {
        return $env:HOME
    }

    $home = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)
    if ($home) {
        return $home
    }

    return "~"
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