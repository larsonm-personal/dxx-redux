#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$androidDir = Split-Path -Parent $PSScriptRoot
$gameScriptsDir = Join-Path $androidDir "game_scripts"
. (Join-Path (Join-Path $androidDir "helpers") "test_helpers.ps1")

$jsonFiles = @(Get-ChildItem -LiteralPath $gameScriptsDir -Filter "test_*.jsonc" -File | Sort-Object Name)
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
$ps1SupportOwners = @{}
$infoByPath = @{}
$ownerClosureByName = @{}
$failures = [System.Collections.Generic.List[string]]::new()

$jsoncProbe = Join-Path ([IO.Path]::GetTempPath()) ("dxx-jsonc-catalog-" + [guid]::NewGuid().ToString('N') + '.jsonc')
try {
    @'
[
  { "_info": { "games": ["d1"], "marker": "marker,}", "vars": { "d1": { "URL": "https://example.invalid/a" } } } },
  { "action": "write_config", "value": "https://example.invalid/a", }, // retained URL
]
'@ | Set-Content -LiteralPath $jsoncProbe -Encoding UTF8
    $probeInfo = Get-TestScriptInfo -ScriptPath $jsoncProbe
    $probeResolved = Resolve-TestScript -ScriptPath $jsoncProbe -GameId 'd1'
    $probeParsed = Read-JsoncFile -Path $probeResolved
    if ($probeInfo.marker -cne 'marker,}' -or
        $probeParsed[0].value -cne 'https://example.invalid/a') {
        $failures.Add('shared JSONC catalog parsing rewrote quoted comment or trailing-comma text')
    }
} catch {
    $failures.Add("shared JSONC catalog parsing probe failed: $($_.Exception.Message)")
} finally {
    Remove-Item -LiteralPath $jsoncProbe -Force -ErrorAction SilentlyContinue
    if ($probeResolved -and $probeResolved -ne $jsoncProbe) {
        Remove-Item -LiteralPath $probeResolved -Force -ErrorAction SilentlyContinue
    }
}

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
    $supportOwner = Get-PowerShellTestSupportOwner -ScriptPath $file.FullName
    if ($supportOwner) {
        $ps1SupportOwners[$file.BaseName] = $supportOwner
    }
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

$supportCount = $ps1SupportOwners.Count
foreach ($supportName in $ps1SupportOwners.Keys) {
    $supportOwner = $ps1SupportOwners[$supportName]
    if ($supportName -eq $supportOwner) {
        $failures.Add("$supportName.ps1: support script cannot own itself")
        continue
    }
    if (-not $ps1ByName.ContainsKey($supportOwner)) {
        $failures.Add("$supportName.ps1: owner '$supportOwner' is not a PowerShell test")
        continue
    }
    if ($ps1SupportOwners.ContainsKey($supportOwner)) {
        $failures.Add("$supportName.ps1: owner '$supportOwner' is itself a support script")
        continue
    }
    $ownerFile = $ps1ByName[$supportOwner]
    if (-not $ownerClosureByName.ContainsKey($supportOwner)) {
        $ownerClosureByName[$supportOwner] = Get-PowerShellDependencyClosureText -RootFile $ownerFile
    }
    if ($ownerClosureByName[$supportOwner] -notmatch [regex]::Escape($ps1ByName[$supportName].Name)) {
        $failures.Add("$supportName.ps1: owner '$supportOwner' does not reference the support script")
    }
}
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

    $ownerFile = if ($ps1ByName.ContainsKey($owner) -and -not $ps1SupportOwners.ContainsKey($owner)) { $ps1ByName[$owner] } else { $null }
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

$standalonePowerShellCount = $ps1ByName.Count - $ps1SupportOwners.Count
Write-Host "Automation catalog valid: $($standaloneJson.Count) standalone JSON tests, $supportCount support scripts, $standalonePowerShellCount standalone PowerShell tests"
exit 0
