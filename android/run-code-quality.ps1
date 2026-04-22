# run-code-quality.ps1 -- Run all code quality checks.
# Tools: clang-format (C/C++), ktlint (Kotlin), PSScriptAnalyzer (PowerShell),
#        shellcheck (bash lint), shfmt (bash format).
# Usage:
#   .\run-code-quality.ps1          # check only (exit 1 if issues)
#   .\run-code-quality.ps1 --fix    # auto-format all supported languages

param(
    [switch]$Fix
)

$ErrorActionPreference = "Continue"
$scriptDir = $PSScriptRoot
$failed = @()
$lockDir = Join-Path $scriptDir "temp"
$lockFile = Join-Path $lockDir "run-code-quality.lock.json"

function Test-ActiveProcess {
    param(
        [int]$ProcessId
    )

    if ($ProcessId -le 0) {
        return $false
    }

    $proc = Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -ErrorAction SilentlyContinue
    return $null -ne $proc
}

function Remove-CodeQualityLock {
    if (-not (Test-Path -LiteralPath $lockFile)) {
        return
    }

    $lockText = Get-Content -LiteralPath $lockFile -Raw -ErrorAction SilentlyContinue
    if (-not $lockText) {
        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
        return
    }

    $lockInfo = $null
    try {
        $lockInfo = $lockText | ConvertFrom-Json
    } catch {
        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
        return
    }

    if ($lockInfo.pid -eq $PID) {
        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Test-Path -LiteralPath $lockDir)) {
    New-Item -ItemType Directory -Path $lockDir | Out-Null
}

if (Test-Path -LiteralPath $lockFile) {
    $lockText = Get-Content -LiteralPath $lockFile -Raw -ErrorAction SilentlyContinue
    $lockInfo = $null
    if ($lockText) {
        try {
            $lockInfo = $lockText | ConvertFrom-Json
        } catch {
            $lockInfo = $null
        }
    }

    if ($lockInfo -and (Test-ActiveProcess -ProcessId ([int]$lockInfo.pid))) {
        Write-Host "Another run-code-quality.ps1 is still active"
        Write-Host "Lock file: $lockFile"
        Write-Host "Active PID: $($lockInfo.pid)"
        Write-Host "Started: $($lockInfo.started)"
        Write-Host "Wait for it to finish or run android\\stop-stale-formatters.ps1 -Kill"
        exit 1
    }

    Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
}

@{
    pid = $PID
    started = (Get-Date).ToString("s")
    fix = [bool]$Fix
    host = $Host.Name
} | ConvertTo-Json | Set-Content -LiteralPath $lockFile -Encoding utf8

Write-Host "=== Code Quality Checks ==="
Write-Host ""

try {
    # --- clang-format ---
    Write-Host "--- C/C++ (clang-format) ---"
    if ($Fix) {
        & "$scriptDir\run-clang-format.ps1"
    } else {
        & "$scriptDir\run-clang-format.ps1" -Check
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "clang-format"
    }
    Write-Host ""

    # --- ktlint ---
    Write-Host "--- Kotlin (ktlint) ---"
    if ($Fix) {
        & "$scriptDir\run-ktlint.ps1"
    } else {
        & "$scriptDir\run-ktlint.ps1" -Check
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "ktlint"
    }
    Write-Host ""

    # --- PSScriptAnalyzer ---
    Write-Host "--- PowerShell (PSScriptAnalyzer) ---"
    if ($Fix) {
        & "$scriptDir\run-psscriptanalyzer.ps1"
    } else {
        & "$scriptDir\run-psscriptanalyzer.ps1" -Check
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "psscriptanalyzer"
    }
    Write-Host ""

    # --- shellcheck ---
    Write-Host "--- Bash lint (shellcheck) ---"
    # shellcheck has no auto-fix; always runs in report mode
    & "$scriptDir\run-shellcheck.ps1"
    if ($LASTEXITCODE -ne 0) {
        $failed += "shellcheck"
    }
    Write-Host ""

    # --- shfmt ---
    Write-Host "--- Bash format (shfmt) ---"
    if ($Fix) {
        & "$scriptDir\run-shfmt.ps1"
    } else {
        & "$scriptDir\run-shfmt.ps1" -Check
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "shfmt"
    }
    Write-Host ""

    # --- Summary ---
    Write-Host "=== Summary ==="
    if ($failed.Count -eq 0) {
        Write-Host "All checks passed"
    } else {
        Write-Host "Failed checks: $($failed -join ', ')"
        if (-not $Fix) {
            Write-Host "Run with --fix to auto-format"
        }
        exit 1
    }
} finally {
    Remove-CodeQualityLock
}
