#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$androidDir = Split-Path -Parent $PSScriptRoot
$gameScriptsDir = Join-Path $androidDir "game_scripts"
. (Join-Path (Join-Path $androidDir "helpers") "test_helpers.ps1")

$jsonFiles = @(Get-ChildItem -LiteralPath $gameScriptsDir -Filter "test_*.json5" -File | Sort-Object Name)
$ps1Files = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter "test_*.ps1" -File | Sort-Object Name)
$allPowerShellFiles = @(
    Get-ChildItem -LiteralPath $androidDir -Filter "*.ps1" -File
    Get-ChildItem -LiteralPath $PSScriptRoot -Filter "*.ps1" -File -Recurse
    Get-ChildItem -LiteralPath (Join-Path $androidDir "helpers") -Filter "*.ps1" -File -Recurse
)
$powerShellTextByPath = @{}
foreach ($file in $allPowerShellFiles) {
    $powerShellTextByPath[$file.FullName] = Get-Content -LiteralPath $file.FullName -Raw
}
$standaloneJson = @{}
$ps1ByName = @{}
$infoByPath = @{}
$ownerClosureByName = @{}
$failures = [System.Collections.Generic.List[string]]::new()

function Get-PowerShellDependencyClosureText {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$RootFile)

    $pending = [System.Collections.Generic.Queue[System.IO.FileInfo]]::new()
    $visited = [System.Collections.Generic.HashSet[string]]::new()
    $text = [System.Collections.Generic.List[string]]::new()
    $pending.Enqueue($RootFile)

    while ($pending.Count -gt 0) {
        $file = $pending.Dequeue()
        if (-not $visited.Add($file.FullName)) { continue }
        $fileText = $powerShellTextByPath[$file.FullName]
        $text.Add($fileText)
        foreach ($candidate in $allPowerShellFiles) {
            if (-not $visited.Contains($candidate.FullName) -and
                $fileText -match [regex]::Escape($candidate.Name)) {
                $pending.Enqueue($candidate)
            }
        }
    }

    return $text -join "`n"
}

foreach ($file in $ps1Files) {
    $ps1ByName[$file.BaseName] = $file
}
foreach ($file in $jsonFiles) {
    $info = Get-TestScriptInfo -ScriptPath $file.FullName
    if ($null -eq $info) {
        $failures.Add("$($file.Name): missing or unparseable first _info object")
        continue
    }
    $infoByPath[$file.FullName] = $info
    if (-not ($info._standalone -eq $false)) {
        $standaloneJson[$file.BaseName] = $file
    }

    if ($file.BaseName -notmatch '_manual(?:_|$)') {
        $scriptText = Get-Content -LiteralPath $file.FullName -Raw
        if ($scriptText -match '(?s)\{[^{}]*"action"\s*:\s*"tap_button"[^{}]*"text"\s*:\s*"(?:Launch )?Descent\b[^"]*"[^{}]*\}') {
            $failures.Add("$($file.Name): automated game launch must use enter_game instead of launcher button taps")
        }
    }
}

foreach ($name in $standaloneJson.Keys) {
    if ($ps1ByName.ContainsKey($name)) {
        $failures.Add("duplicate top-level test name '$name'")
    }
}

$supportCount = 0
foreach ($file in $jsonFiles) {
    if (-not $infoByPath.ContainsKey($file.FullName)) { continue }
    $info = $infoByPath[$file.FullName]
    $standalone = -not ($info._standalone -eq $false)
    $owner = if ($info._owner) { [string]$info._owner } else { $null }
    if ($standalone) {
        if ($owner) {
            $failures.Add("$($file.Name): standalone scripts cannot declare _owner")
        }
        continue
    }

    $supportCount++
    if (-not $owner) {
        $failures.Add("$($file.Name): _standalone=false requires _owner")
        continue
    }

    $ownerFile = if ($ps1ByName.ContainsKey($owner)) { $ps1ByName[$owner] } else { $null }
    if (-not $ownerFile) {
        $failures.Add("$($file.Name): owner '$owner' is not a top-level PowerShell test")
        continue
    }

    if (-not $ownerClosureByName.ContainsKey($owner)) {
        $ownerClosureByName[$owner] = Get-PowerShellDependencyClosureText -RootFile $ownerFile
    }
    $ownerText = $ownerClosureByName[$owner]
    if ($ownerText -notmatch [regex]::Escape($file.Name)) {
        $failures.Add("$($file.Name): owner '$owner' does not reference the support script")
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Host "FAIL: $failure"
    }
    exit 1
}

Write-Host "Automation catalog valid: $($standaloneJson.Count) standalone JSON tests, $supportCount support scripts, $($ps1ByName.Count) PowerShell entries"
exit 0
