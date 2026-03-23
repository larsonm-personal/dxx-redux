#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Interactive menu for running DXX-Redux regression tests

.DESCRIPTION
    Discovers both test_*.json5 game-automation scripts (run via run_test.ps1)
    and test_*.ps1 PowerShell integration tests (in tests/ dir), and presents
    a unified menu.

.EXAMPLE
    .\Run-TestMenu.ps1
#>

param()

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent -Path $PSCommandPath
$GameScriptsDir = Join-Path -Path $ScriptDir -ChildPath "game_scripts"
$TestsDir = Join-Path -Path $ScriptDir -ChildPath "tests"
$runTestScript = Join-Path -Path $ScriptDir -ChildPath "run_test.ps1"

. "$ScriptDir\test_helpers.ps1"

if (-not (Test-Path $runTestScript)) {
    Write-Host "[!] run_test.ps1 not found at $runTestScript" -ForegroundColor Red
    exit 1
}

# -- Discover tests ----------------------------------------------------------

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "DXX-Redux Regression Test Menu" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

# Build unified list: each entry is @{Name; Type; Path; Games}
$allTests = @()

# json5 game-automation scripts
$json5Tests = @(Get-ChildItem -Path $GameScriptsDir -Filter "test_*.json5" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $json5Tests) {
    $games = Get-ScriptGameInfo -ScriptPath $t.FullName
    $standalone = Get-ScriptStandalone -ScriptPath $t.FullName
    $tag = if ($games) { "[" + ($games -join ",") + "]" } else { "" }
    if (-not $standalone) { $tag = "[support] $tag" }
    $allTests += @{ Name = $t.BaseName; Type = "json5"; Path = $t.FullName; Games = $games; Tag = $tag; Standalone = $standalone }
}

# ps1 integration tests
$ps1Tests = @(Get-ChildItem -Path $TestsDir -Filter "test_*.ps1" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $ps1Tests) {
    $allTests += @{ Name = $t.BaseName; Type = "ps1"; Path = $t.FullName; Games = $null; Tag = "[ps1]" }
}

if ($allTests.Count -eq 0) {
    Write-Host "[!] No tests found" -ForegroundColor Red
    exit 1
}

# -- Display menu ------------------------------------------------------------

Write-Host "Available tests:" -ForegroundColor White
Write-Host ""

# Show json5 tests first, then ps1 tests, with a separator
$json5Count = $json5Tests.Count
for ($i = 0; $i -lt $allTests.Count; $i++) {
    if ($i -eq $json5Count -and $json5Count -gt 0) {
        Write-Host ""
        Write-Host "  --- PowerShell integration tests ---" -ForegroundColor DarkGray
    }
    $entry = $allTests[$i]
    $color = if ($entry.Standalone -eq $false) { "DarkGray" } else { "White" }
    Write-Host "  $($i + 1)) $($entry.Name)  $($entry.Tag)" -ForegroundColor $color
}
Write-Host ""

$choice = Read-Host "Select test (1-$($allTests.Count))"
$parsed = 0
if (-not [int]::TryParse($choice, [ref]$parsed) -or $parsed -lt 1 -or $parsed -gt $allTests.Count) {
    Write-Host "[!] Invalid selection" -ForegroundColor Red
    exit 1
}

$selected = $allTests[$parsed - 1]
Write-Host ""
Write-Host "[*] Selected: $($selected.Name)  $($selected.Tag)" -ForegroundColor Green

# -- Run the selected test ---------------------------------------------------

if ($selected.Type -eq "json5") {
    # json5: delegate to run_test.ps1 with optional game choice
    $gameArg = @{}
    $games = $selected.Games
    if ($games -and $games.Count -gt 1) {
        Write-Host ""
        Write-Host "This test supports multiple games:" -ForegroundColor White
        for ($i = 0; $i -lt $games.Count; $i++) {
            Write-Host "  $($i + 1)) $($games[$i].ToUpper())"
        }
        Write-Host "  $($games.Count + 1)) Both (run sequentially)"
        Write-Host ""
        $gameChoice = Read-Host "Select game (1-$($games.Count + 1))"
        $gc = 0
        if ([int]::TryParse($gameChoice, [ref]$gc) -and $gc -ge 1 -and $gc -le $games.Count) {
            $gameArg = @{ Game = $games[$gc - 1] }
        }
    }
    Write-Host ""
    $scriptFile = [System.IO.Path]::GetFileName($selected.Path)
    & $runTestScript $scriptFile @gameArg
    exit $LASTEXITCODE
} else {
    # ps1: run directly
    Write-Host ""
    & $selected.Path
    exit $LASTEXITCODE
}
