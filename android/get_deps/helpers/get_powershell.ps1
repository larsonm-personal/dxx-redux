#!/usr/bin/env pwsh
# Install the pinned PowerShell 7 host runtime.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot))
. (Join-Path $PSScriptRoot "Get-DepPlatform.ps1")
$depBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing
$hostPlatform = Get-HostPlatform

$conf = @{}
Get-Content (Join-Path $PSScriptRoot "../tool_versions.conf") | ForEach-Object {
    if ($_ -match '^([A-Z0-9_]+)=(.+)$') {
        $conf[$Matches[1]] = $Matches[2]
    }
}

$version = $conf["POWERSHELL_VERSION"]
$url = $conf["POWERSHELL_URL"]

function Get-BashCommandPath {
    foreach ($name in @("bash", "sh")) {
        $command = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) {
            return $command.Source
        }
    }
    return $null
}

function Get-PwshVersion($path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    try {
        foreach ($line in @(& $path -NoProfile -NonInteractive -Command '$PSVersionTable.PSVersion.ToString()' 2>&1)) {
            $text = ([string]$line).Trim()
            if ($text) { return $text }
        }
    } catch {}
    return $null
}

function Assert-InstallDirIsSafe($baseDir, $targetDir) {
    $baseFull = [System.IO.Path]::GetFullPath($baseDir).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $targetFull = [System.IO.Path]::GetFullPath($targetDir).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $prefix = $baseFull + [System.IO.Path]::DirectorySeparatorChar
    if (-not $targetFull.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove PowerShell install outside dependency base: $targetFull"
    }
    if ((Split-Path $targetFull -Leaf) -notlike "powershell-*") {
        throw "Refusing to remove unexpected PowerShell install directory: $targetFull"
    }
}

function Ensure-WindowsPwshShim($pwshPath) {
    $binDir = Join-Path $depBase "bin"
    if (-not (Test-Path -LiteralPath $binDir)) {
        New-Item -ItemType Directory -Path $binDir -Force | Out-Null
    }

    $shimPath = Join-Path $binDir "pwsh.cmd"
    "@echo off`r`n`"$pwshPath`" %*`r`n" | Set-Content -LiteralPath $shimPath -NoNewline

    $pathParts = @($env:Path -split ';' | Where-Object { $_ })
    if (-not ($pathParts | Where-Object { $_ -ieq $binDir })) {
        $env:Path = "$binDir;$env:Path"
    }

    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $userPathParts = @($userPath -split ';' | Where-Object { $_ })
    if (-not ($userPathParts | Where-Object { $_ -ieq $binDir })) {
        $newUserPath = if ($userPath) { "$binDir;$userPath" } else { $binDir }
        [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
        Write-Host "Added $binDir to the user PATH for future terminals"
    }

    Write-Host "pwsh shim installed: $shimPath"
}

if ($hostPlatform -eq "Linux") {
    $bash = Get-BashCommandPath
    if (-not $bash) {
        throw "bash or sh was not found on PATH"
    }
    & $bash (Join-Path $PSScriptRoot "get_powershell.sh")
    return
}

if ($hostPlatform -ne "Windows") {
    throw "PowerShell install sync is only automated on Windows and Linux hosts"
}

$installDir = Join-Path $depBase "powershell-$version"
$pwshExe = Join-Path $installDir "pwsh.exe"
$installedVersion = Get-PwshVersion $pwshExe
if ($installedVersion -eq $version -and -not $Force) {
    Write-Host "PowerShell $version already installed at $installDir"
    Ensure-WindowsPwshShim $pwshExe
    return $pwshExe
}

if ((Test-Path -LiteralPath $installDir) -and ($Force -or $installedVersion -ne $version)) {
    Assert-InstallDirIsSafe $depBase $installDir
    Remove-Item -LiteralPath $installDir -Recurse -Force
}

Write-Host "Downloading PowerShell $version from $url"
New-Item -ItemType Directory -Path $installDir -Force | Out-Null
$tmpArchive = Join-Path $depBase "powershell-$version.zip"
try {
    Invoke-WebRequest -Uri $url -OutFile $tmpArchive -UseBasicParsing
    Expand-Archive -Path $tmpArchive -DestinationPath $installDir -Force
} finally {
    Remove-Item -LiteralPath $tmpArchive -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $pwshExe)) {
    throw "pwsh.exe not found after extraction at: $pwshExe"
}

$actualVersion = Get-PwshVersion $pwshExe
if ($actualVersion -ne $version) {
    throw "Installed PowerShell version is $actualVersion, expected $version"
}

Ensure-WindowsPwshShim $pwshExe
Write-Host "PowerShell $version installed: $pwshExe"
return $pwshExe
