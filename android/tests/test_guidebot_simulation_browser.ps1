#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$browser = Join-Path $repoRoot 'android\helpers\watch_guidebot_simulation.ps1'
$pwsh = (Get-Process -Id $PID).Path
$browserSource = [IO.File]::ReadAllText($browser)

if ($browserSource -notmatch "function Invoke-GuidebotBrowserBuild" -or
    $browserSource -notmatch "Invoke-GuidebotBrowserBuild\s+New-Item" -or
    $browserSource -notmatch "'-LevelTimeoutSeconds', '900', '-NoBuild'" -or
    $browserSource -match 'desktopBuildReady') {
    throw 'Every visible route run must check the build before starting a no-build child process'
}

function Get-BrowserResults {
    param([string]$Query, [string[]]$MissionJson, [int[]]$Level)
    $arguments = @('-NoProfile', '-File', $browser, '-ListOnly')
    if ($Query) { $arguments += @('-Query', $Query) }
    if ($MissionJson) { $arguments += @('-MissionJson') + $MissionJson }
    if ($Level) { $arguments += @('-Level') + @($Level | ForEach-Object { [string]$_ }) }
    $json = (& $pwsh @arguments) | Out-String
    if ($LASTEXITCODE -ne 0) { throw "Browser list failed for query $Query" }
    if (-not $json.Trim()) { return @() }
    return @($json | ConvertFrom-Json)
}

$castaway = @(Get-BrowserResults -Query 'castaway 2')
if ($castaway.Count -ne 1 -or $castaway[0].MissionJson -ne 'castaway_redux.json' -or $castaway[0].Level -ne 2) {
    throw 'castaway 2 did not select the expected level'
}
$obsidian = @(Get-BrowserResults -Query 'obsidian 10')
if (-not @($obsidian | Where-Object { $_.Level -eq 10 }).Count) {
    throw 'obsidian 10 did not include level 10'
}
$filename = @(Get-BrowserResults -Query 'd2leva-3')
if (-not @($filename | Where-Object { $_.MissionJson -eq 'Counterstrike.json' -and $_.Level -eq 3 }).Count) {
    throw 'Level filename filtering did not select Counterstrike level 3'
}
$direct = @(Get-BrowserResults -MissionJson castaway_redux.json -Level 2)
if ($direct.Count -ne 1 -or $direct[0].SimulationStatus -notin @(
        'not_run', 'stale', 'ok', 'partial', 'failed', 'timeout', 'unsupported',
        'route_mismatch', 'nondeterministic'
    )) {
    throw 'Direct mission and level filtering did not return one indexed record'
}
$preview = (& $pwsh -NoProfile -File $browser -PreviewPicker -Query 'obsidian 1') | Out-String
if ($LASTEXITCODE -ne 0 -or $preview -notmatch 'GuideBot route browser' -or
    $preview -notmatch 'Search: obsidian 1') {
    throw 'Initial picker frame did not render its search prompt immediately'
}

Write-Host 'GuideBot simulation browser filtering and initial rendering passed'
