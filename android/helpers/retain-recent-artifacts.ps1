#!/usr/bin/env pwsh
# Before producing output, retain three prior generations in its timestamped families
# Artifacts may name planned outputs which do not exist yet

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$Artifacts,
    [ValidateRange(1, 1000)][int]$Keep = 3,
    [string]$RepositoryRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path (Split-Path $PSScriptRoot) }
$separators = [char[]]@([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd($separators)
$pathComparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) { [StringComparison]::Ordinal } else { [StringComparison]::OrdinalIgnoreCase }
$repositoryPrefix = $RepositoryRoot + [IO.Path]::DirectorySeparatorChar
$artifactPaths = @($Artifacts | ForEach-Object { [IO.Path]::GetFullPath($_) } | Sort-Object -Unique)
$artifactPaths = @($artifactPaths | Where-Object { $_.StartsWith($repositoryPrefix, $pathComparison) })
if ($artifactPaths.Count -eq 0) {
    Write-Verbose "No repository artifacts are eligible for retention"
    return
}
$roots = @($artifactPaths | ForEach-Object { Split-Path -Parent $_ } | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Sort-Object -Unique)
if ($roots.Count -eq 0) { return }
$arguments = @{
    apply                    = $true
    KeepDirectoryGenerations = $Keep
    KeepFileGenerations      = $Keep
    MinimumAgeHours          = 0
    Roots                    = $roots
    FamilySeeds              = $artifactPaths
    ExcludePaths             = $artifactPaths
    IgnoreUnrecognizedFamilySeeds = $true
    Confirm                  = $false
    RepositoryRoot           = $RepositoryRoot
}

& (Join-Path $PSScriptRoot "clean-old-artifacts.ps1") @arguments
