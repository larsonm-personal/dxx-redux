# run-psscriptanalyzer.ps1 -- Run PSScriptAnalyzer on PowerShell scripts.
# Usage:
#   .\run-psscriptanalyzer.ps1          # auto-fix + format (default)
#   .\run-psscriptanalyzer.ps1 --check  # report issues, exit 1 if any

param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"

# --- Ensure PSScriptAnalyzer is available ---
if (-not (Get-Module PSScriptAnalyzer -ListAvailable)) {
    Write-Error "PSScriptAnalyzer not installed. Run: Install-Module PSScriptAnalyzer -Scope CurrentUser -Force"
    exit 1
}
Import-Module PSScriptAnalyzer

$settingsFile = Join-Path $PSScriptRoot "PSScriptAnalyzerSettings.psd1"
if (-not (Test-Path $settingsFile)) {
    Write-Error "Settings file not found: $settingsFile"
    exit 1
}

# --- Gather .ps1 files ---
# Exclude build outputs and gradle wrapper
$files = Get-ChildItem -Path $PSScriptRoot -Recurse -Include "*.ps1" |
    Where-Object { $_.FullName -notmatch '[\\/]build[\\/]' }

if ($files.Count -eq 0) {
    Write-Host "No PowerShell files found."
    exit 0
}

Write-Host "Found $($files.Count) PowerShell files."

# --- Run ---
if ($Check) {
    $allIssues = @()
    $repoRoot = Split-Path $PSScriptRoot

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
        Write-Host "All PowerShell files pass checks."
    }
    exit $exitCode
} else {
    foreach ($f in $files) {
        # Auto-fix lint issues
        $null = Invoke-ScriptAnalyzer -Path $f.FullName -Settings $settingsFile -Fix

        # Auto-format
        $content = Get-Content $f.FullName -Raw
        if (-not $content) { continue }
        $formatted = Invoke-Formatter -ScriptDefinition $content -Settings $settingsFile
        if ($content -ne $formatted) {
            Set-Content -Path $f.FullName -Value $formatted -NoNewline
            $rel = $f.FullName.Substring((Split-Path $PSScriptRoot).Length + 1)
            Write-Host "  Formatted: $rel"
        }
    }
    Write-Host "PSScriptAnalyzer fix pass complete."
}
