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

Write-Host "=== Code Quality Checks ==="
Write-Host ""

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
