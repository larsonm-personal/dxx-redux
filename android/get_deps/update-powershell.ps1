#!/usr/bin/env pwsh
# Update PowerShell for repo dependency tooling or the Windows host package.

param(
    [switch]$Pinned,
    [switch]$System,
    [switch]$StatusOnly,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot
$androidDir = Split-Path $scriptDir -Parent
$repoRoot = Split-Path $androidDir -Parent
. (Join-Path $scriptDir "helpers/Get-DepPlatform.ps1")

$depBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing
$hostPlatform = Get-HostPlatform
$updateSystem = -not $StatusOnly -and ($System -or (-not $Pinned -and $hostPlatform -eq "Windows"))
$updatePinned = -not $StatusOnly -and ($Pinned -or (-not $System -and $hostPlatform -ne "Windows"))

function Get-PwshVersion($path) {
    if (-not $path -or -not (Test-Path -LiteralPath $path)) {
        return $null
    }

    try {
        foreach ($line in @(& $path -NoProfile -NonInteractive -Command '$PSVersionTable.PSVersion.ToString()' 2>&1)) {
            $text = ([string]$line).Trim()
            if ($text) { return $text }
        }
    } catch {}

    return $null
}

function Get-RepoPinnedPowerShellConfig {
    $confPath = Join-Path $scriptDir "tool_versions.conf"
    $result = @{}
    foreach ($line in Get-Content -LiteralPath $confPath) {
        if ($line -match '^(POWERSHELL_VERSION|POWERSHELL_URL)=(.+)$') {
            $result[$Matches[1]] = $Matches[2].Trim("'", '"')
        }
    }
    if (-not $result["POWERSHELL_VERSION"]) {
        throw "POWERSHELL_VERSION not found in $confPath"
    }
    if (-not $result["POWERSHELL_URL"]) {
        throw "POWERSHELL_URL not found in $confPath"
    }
    return $result
}

function Invoke-PinnedPowerShellUpdate {
    $helper = Join-Path $scriptDir "helpers/get_powershell.ps1"
    $args = @()
    if ($Force) {
        $args += "-Force"
    }

    & $helper @args
}

function Invoke-SystemPowerShellUpdate {
    param(
        [string]$Version,
        [string]$ZipUrl
    )

    if ($hostPlatform -ne "Windows") {
        throw "-System is only automated on Windows hosts"
    }

    if ($ZipUrl -notmatch '\.zip$') {
        throw "Expected a PowerShell ZIP URL that can be mapped to an MSI URL: $ZipUrl"
    }

    $msiUrl = $ZipUrl -replace '\.zip$', '.msi'
    $downloadDir = Join-Path $repoRoot "temp/powershell-update"
    New-Item -ItemType Directory -Path $downloadDir -Force | Out-Null
    $msiPath = Join-Path $downloadDir ("PowerShell-$Version-win-x64.msi")

    Write-Host "Downloading PowerShell $Version MSI from $msiUrl"
    Invoke-WebRequest -Uri $msiUrl -OutFile $msiPath -UseBasicParsing

    $msiArgs = @(
        "/package `"$msiPath`"",
        "/passive",
        "ADD_EXPLORER_CONTEXT_MENU_OPENPOWERSHELL=1",
        "ADD_FILE_CONTEXT_MENU_RUNPOWERSHELL=1",
        "ENABLE_PSREMOTING=0",
        "REGISTER_MANIFEST=1",
        "USE_MU=1",
        "ENABLE_MU=1",
        "ADD_PATH=1"
    )

    Write-Host "Launching the PowerShell $Version MSI installer with UAC elevation"
    $process = Start-Process -FilePath "msiexec.exe" -ArgumentList $msiArgs -Verb RunAs -Wait -PassThru
    if ($process.ExitCode -notin @(0, 3010)) {
        throw "PowerShell MSI installer failed with exit code $($process.ExitCode)"
    }

    if ($process.ExitCode -eq 3010) {
        Write-Host "PowerShell $Version installed; Windows reports a restart is required"
    } else {
        Write-Host "PowerShell $Version MSI install completed"
    }
}

$currentProcess = Get-Process -Id $PID
$currentPwshPath = $currentProcess.Path
$currentVersion = Get-PwshVersion $currentPwshPath
$pinnedConfig = Get-RepoPinnedPowerShellConfig
$pinnedVersion = $pinnedConfig["POWERSHELL_VERSION"]
$pinnedUrl = $pinnedConfig["POWERSHELL_URL"]
$pinnedDir = Join-Path $depBase "powershell-$pinnedVersion"
$pinnedPwsh = Join-Path $pinnedDir (Get-PlatformExecutableName -ToolName "pwsh")
$pinnedInstalledVersion = Get-PwshVersion $pinnedPwsh

Write-Host "Active PowerShell: $currentVersion"
Write-Host "Active path: $currentPwshPath"
Write-Host "Repo-pinned PowerShell: $pinnedVersion"
if ($pinnedInstalledVersion) {
    Write-Host "Pinned install: $pinnedInstalledVersion at $pinnedPwsh"
} else {
    Write-Host "Pinned install: not installed"
}

if ($updatePinned) {
    Invoke-PinnedPowerShellUpdate | Out-Host
}

if ($updateSystem) {
    Invoke-SystemPowerShellUpdate -Version $pinnedVersion -ZipUrl $pinnedUrl
}

$shimPath = Join-Path (Join-Path $depBase "bin") "pwsh.cmd"
if ((Get-HostPlatform) -eq "Windows" -and (Test-Path -LiteralPath $shimPath)) {
    Write-Host "Repo pwsh shim: $shimPath"
}

Write-Host "Restart this terminal to pick up any PATH or system PowerShell changes"
