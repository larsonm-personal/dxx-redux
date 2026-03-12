#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Interactive menu for running DXX-Redux regression tests

.DESCRIPTION
    Presents a menu of test_*.json5 tests, then delegates to run_test.ps1
    which handles emulator health, app launch, and test monitoring.

.EXAMPLE
    .\Run-TestMenu.ps1
#>

param()

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent -Path $PSCommandPath
$GameScriptsDir = Join-Path -Path $ScriptDir -ChildPath "game_scripts"
$runTestScript = Join-Path -Path $ScriptDir -ChildPath "run_test.ps1"

if (-not (Test-Path $runTestScript)) {
    Write-Host "[!] run_test.ps1 not found at $runTestScript" -ForegroundColor Red
    exit 1
}

# ── Discover tests ──────────────────────────────────────────────────────────

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "DXX-Redux Regression Test Menu" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

$tests = @(Get-ChildItem -Path $GameScriptsDir -Filter "test_*.json5" -File | Sort-Object Name)

if ($tests.Count -eq 0) {
    Write-Host "[!] No test_*.json5 files found in $GameScriptsDir" -ForegroundColor Red
    exit 1
}

Write-Host "Available tests:" -ForegroundColor White
Write-Host ""
for ($i = 0; $i -lt $tests.Count; $i++) {
    Write-Host "  $($i + 1))) $($tests[$i].BaseName)"
}
Write-Host ""

$choice = Read-Host "Select test (1-$($tests.Count))"

if (-not [int]::TryParse($choice, [ref]0) -or $choice -lt 1 -or $choice -gt $tests.Count) {
    Write-Host "[!] Invalid selection" -ForegroundColor Red
    exit 1
}

$selectedTest = $tests[$choice - 1]
Write-Host ""
Write-Host "[*] Selected: $($selectedTest.BaseName)" -ForegroundColor Green
Write-Host ""

# ── Delegate to run_test.ps1 ────────────────────────────────────────────────

& $runTestScript $selectedTest.Name
exit $LASTEXITCODE
