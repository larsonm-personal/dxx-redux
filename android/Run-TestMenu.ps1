#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Interactive menu for running DXX-Redux regression tests

.DESCRIPTION
    Discovers .json5 game-automation scripts (run via run_test.ps1) from
    game_scripts/ and test_*.ps1 PowerShell integration tests (in tests/ dir),
    and presents a unified menu.

.EXAMPLE
    .\Run-TestMenu.ps1
#>

param(
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent -Path $PSCommandPath
$HelpersDir = Join-Path -Path $ScriptDir -ChildPath "helpers"
$GameScriptsDir = Join-Path -Path $ScriptDir -ChildPath "game_scripts"
$TestsDir = Join-Path -Path $ScriptDir -ChildPath "tests"
$runTestScript = Join-Path -Path $HelpersDir -ChildPath "run_test.ps1"

. (Join-Path $HelpersDir "test_helpers.ps1")

function Read-NumberedChoice {
    param(
        [string]$Prompt,
        [int]$OptionCount,
        [int]$DefaultChoice = 1
    )

    while ($true) {
        $choice = Read-Host $Prompt
        if ([string]::IsNullOrWhiteSpace($choice)) {
            return $DefaultChoice
        }

        $selected = 0
        if ([int]::TryParse($choice, [ref]$selected) -and $selected -ge 1 -and $selected -le $OptionCount) {
            return $selected
        }

        Write-Host "Enter a number between 1 and $OptionCount" -ForegroundColor Yellow
    }
}

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
$json5Tests = @(Get-ChildItem -Path $GameScriptsDir -Filter "*.json5" -File -ErrorAction SilentlyContinue |
        Where-Object { Get-ScriptStandalone -ScriptPath $_.FullName } |
        Sort-Object Name)
foreach ($t in $json5Tests) {
    $games = Get-ScriptGameInfo -ScriptPath $t.FullName
    $tag = if ($games) { "[" + ($games -join ",") + "]" } else { "" }
    $allTests += @{ Name = $t.BaseName; Type = "json5"; Path = $t.FullName; Games = $games; Tag = $tag }
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
    Write-Host "  $($i + 1)) $($entry.Name)  $($entry.Tag)" -ForegroundColor White
}
Write-Host ""

$parsed = Read-NumberedChoice -Prompt "Select test (1-$($allTests.Count))" -OptionCount $allTests.Count
$selected = $allTests[$parsed - 1]
Write-Host ""
Write-Host "[*] Selected: $($selected.Name)  $($selected.Tag)" -ForegroundColor Green

# -- Build fresh APK ---------------------------------------------------------

if (-not $NoBuild) {
    Write-Host ""
    Write-Status "Building fresh APK..."
    if ((Test-RegressionWindowsHost) -and (Test-Path "C:\local\jdk-21")) {
        $env:JAVA_HOME = "C:\local\jdk-21"
    }
    $gradleWrapper = Resolve-RegressionGradleWrapper -AndroidDir $ScriptDir
    & $gradleWrapper assembleDebug --no-daemon 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) {
        Write-Status "Build failed" "Red"
        exit 1
    }
    Write-Status "Build succeeded"
}

# -- Run the selected test ---------------------------------------------------

if ($selected.Type -eq "json5") {
    # json5: delegate to run_test.ps1 with optional game choice and params
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
        $gc = Read-NumberedChoice -Prompt "Select game (1-$($games.Count + 1))" -OptionCount ($games.Count + 1)
        if ($gc -le $games.Count) {
            $gameArg = @{ Game = $games[$gc - 1] }
        }
    }

    # Prompt for script params (e.g. texture resolution)
    $paramArg = @{}
    $scriptParams = Get-ScriptParams -ScriptPath $selected.Path
    if ($scriptParams) {
        $selectedParams = @{}
        foreach ($prop in $scriptParams.PSObject.Properties) {
            $pName = $prop.Name
            $pDef = $prop.Value
            $optKeys = @($pDef.options.PSObject.Properties.Name)
            $label = if ($pDef.label) { $pDef.label } else { $pName }
            Write-Host ""
            Write-Host "${label}:" -ForegroundColor White
            for ($pi = 0; $pi -lt $optKeys.Count; $pi++) {
                Write-Host "  $($pi + 1)) $($optKeys[$pi])"
            }
            Write-Host ""
            $pci = Read-NumberedChoice -Prompt "Select (1-$($optKeys.Count))" -OptionCount $optKeys.Count
            $selectedParams[$pName] = $optKeys[$pci - 1]
        }
        $paramArg = @{ Params = $selectedParams }
    }

    Write-Host ""
    $scriptFile = [System.IO.Path]::GetFileName($selected.Path)
    $installArg = if (-not $NoBuild) { @{ Install = $true } } else { @{} }
    & $runTestScript $scriptFile @gameArg @paramArg @installArg
    exit $LASTEXITCODE
} else {
    # ps1: run directly
    Write-Host ""
    & $selected.Path
    exit $LASTEXITCODE
}
