#!/usr/bin/env pwsh
# run-psscriptanalyzer.ps1 -- Run PSScriptAnalyzer on PowerShell scripts.
# Usage:
#   .\run-psscriptanalyzer.ps1          # auto-fix + format (default)
#   .\run-psscriptanalyzer.ps1 --check  # report issues, exit 1 if any
#   .\run-psscriptanalyzer.ps1 -Paths path\to\file path\to\dir

param(
    [switch]$Check,
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path $PSScriptRoot
$repoRoot = Split-Path $androidRoot
$confFile = Join-Path $androidRoot "get_deps\tool_versions.conf"

function Get-ToolVersionSetting {
    param([Parameter(Mandatory)][string]$Name)

    if (-not (Test-Path -LiteralPath $confFile -PathType Leaf)) {
        return $null
    }

    foreach ($line in Get-Content -LiteralPath $confFile) {
        if ($line -match "^$([regex]::Escape($Name))=(.+)$") {
            return $Matches[1].Trim()
        }
    }

    return $null
}

function Get-ScopedFiles {
    param(
        [string]$RootPath,
        [string[]]$InputPaths,
        [string[]]$ValidExtensions
    )

    $results = @()
    if ($InputPaths -and $InputPaths.Count -gt 0) {
        foreach ($inputPath in $InputPaths) {
            if ([string]::IsNullOrWhiteSpace($inputPath)) {
                continue
            }

            $candidate = $inputPath
            if (-not [System.IO.Path]::IsPathRooted($candidate)) {
                $candidate = Join-Path $repoRoot $candidate
            }

            $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
            if (-not $item) {
                continue
            }

            if ($item.PSIsContainer) {
                $results += Get-ChildItem -LiteralPath $item.FullName -Recurse -File
            } else {
                $results += $item
            }
        }
    } else {
        $results = Get-ChildItem -Path $RootPath -Recurse -File
    }

    return @($results | Where-Object {
            $_.FullName.StartsWith($RootPath, [System.StringComparison]::OrdinalIgnoreCase) -and
            ($ValidExtensions -contains $_.Extension.ToLowerInvariant()) -and
            $_.FullName -notmatch '[\\/](build|\.cxx|temp)[\\/]'
        } | Sort-Object FullName -Unique)
}

# --- Ensure PSScriptAnalyzer is available ---
$analyzerVersion = Get-ToolVersionSetting -Name "PSSCRIPTANALYZER_VERSION"
if (-not $analyzerVersion) {
    Write-Error "PSSCRIPTANALYZER_VERSION not found in $confFile"
    exit 1
}
$analyzerModule = Get-Module PSScriptAnalyzer -ListAvailable |
    Where-Object { $_.Version -eq [version]$analyzerVersion } |
    Sort-Object Version -Descending |
    Select-Object -First 1
if (-not $analyzerModule) {
    Write-Host "PSScriptAnalyzer $analyzerVersion not found; installing for current user..."
    try {
        $installParameters = @{
            Name = 'PSScriptAnalyzer'
            Scope = 'CurrentUser'
            RequiredVersion = $analyzerVersion
            Repository = 'PSGallery'
            Force = $true
            AllowClobber = $true
            ErrorAction = 'Stop'
        }
        if ((Get-Command Install-Module).Parameters.ContainsKey('AcceptLicense')) {
            $installParameters['AcceptLicense'] = $true
        }
        Install-Module @installParameters
    } catch {
        Write-Error "PSScriptAnalyzer install failed. Run: Install-Module PSScriptAnalyzer -Scope CurrentUser -RequiredVersion $analyzerVersion -Force"
        exit 1
    }
    $analyzerModule = Get-Module PSScriptAnalyzer -ListAvailable |
        Where-Object { $_.Version -eq [version]$analyzerVersion } |
        Sort-Object Version -Descending |
        Select-Object -First 1
}
if (-not $analyzerModule) {
    Write-Error "PSScriptAnalyzer $analyzerVersion not available after install"
    exit 1
}
Import-Module PSScriptAnalyzer -RequiredVersion $analyzerVersion

$settingsFile = Join-Path $androidRoot "PSScriptAnalyzerSettings.psd1"
if (-not (Test-Path $settingsFile)) {
    Write-Error "Settings file not found: $settingsFile"
    exit 1
}

# --- Gather .ps1 files ---
# Exclude build outputs, gradle wrapper, and NDK cmake cache
$files = Get-ScopedFiles -RootPath $repoRoot -InputPaths $Paths -ValidExtensions @('.ps1')

if ($files.Count -eq 0) {
    Write-Host "No PowerShell files found"
    exit 0
}

Write-Host "Found $($files.Count) PowerShell files"

# --- Run ---
if ($Check) {
    $allIssues = @()

    # Lint pass
    foreach ($f in $files) {
        $results = Invoke-ScriptAnalyzer -Path $f.FullName -Settings $settingsFile
        $allIssues += $results
    }

    # Format check pass
    $formatDirty = @()
    foreach ($f in $files) {
        $content = Get-Content $f.FullName -Raw
        if (-not $content) { continue }
        $formatted = Invoke-Formatter -ScriptDefinition $content -Settings $settingsFile
        if ($content -ne $formatted) {
            $formatDirty += $f.FullName
        }
    }

    $exitCode = 0
    if ($allIssues.Count -gt 0) {
        Write-Host ""
        Write-Host "Lint issues ($($allIssues.Count)):"
        foreach ($issue in $allIssues) {
            $rel = $issue.ScriptPath.Substring($repoRoot.Length + 1)
            Write-Host "  ${rel}:$($issue.Line): [$($issue.RuleName)] $($issue.Message)"
        }
        $exitCode = 1
    }
    if ($formatDirty.Count -gt 0) {
        Write-Host ""
        Write-Host "Files that need formatting ($($formatDirty.Count)):"
        foreach ($d in $formatDirty) {
            $rel = $d.Substring($repoRoot.Length + 1)
            Write-Host "  $rel"
        }
        $exitCode = 1
    }
    if ($exitCode -eq 0) {
        Write-Host "All PowerShell files pass checks"
    }
    exit $exitCode
} else {
    foreach ($f in $files) {
        # Auto-fix lint issues
        $null = Invoke-ScriptAnalyzer -Path $f.FullName -Settings $settingsFile -Fix

        # Auto-format
        $content = Get-Content $f.FullName -Raw
        if (-not $content) { continue }
        $formatted = (Invoke-Formatter -ScriptDefinition $content -Settings $settingsFile) -replace "`r`n", "`n"
        if ($content -ne $formatted) {
            Set-Content -Path $f.FullName -Value $formatted -NoNewline
            $rel = $f.FullName.Substring((Split-Path $PSScriptRoot).Length + 1)
            Write-Host "  Formatted: $rel"
        }
    }
    Write-Host "PSScriptAnalyzer fix pass complete"
}
