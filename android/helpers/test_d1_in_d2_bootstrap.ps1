#!/usr/bin/env pwsh
# Exercise real D2 executable startup with only original D1 assets
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$Executable = "buildd2/main/d2x-redux.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$dataPath = (Resolve-Path -LiteralPath $D1DataDirectory).Path
$outputPath = Join-Path $repository "temp/d1-in-d2-bootstrap"
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$stagedData = Join-Path $outputPath "data"
New-Item -ItemType Directory -Path $stagedData -Force | Out-Null

foreach ($asset in @("descent.hog", "descent.pig")) {
    Copy-Item -LiteralPath (Join-Path $dataPath $asset) -Destination (Join-Path $stagedData $asset) -Force
}
# Reject leftover or accidentally staged game data that could mask dependencies
$unexpectedData = @(Get-ChildItem -LiteralPath $outputPath -Recurse -File | Where-Object {
        $_.Extension -in @(".hog", ".ham", ".pig", ".s11", ".s22", ".256", ".fnt", ".txb", ".tex", ".ini") -and
        $_.FullName -notin @((Join-Path $stagedData "descent.hog"), (Join-Path $stagedData "descent.pig"))
    })
if ($unexpectedData.Count) {
    throw "Bootstrap directory contains unexpected data: $($unexpectedData.Name -join ', ')"
}

$stagedExecutable = Join-Path $outputPath "d2x-redux.exe"
Copy-Item -LiteralPath $executablePath -Destination $stagedExecutable -Force
Get-ChildItem -LiteralPath (Split-Path $executablePath) -Filter "*.dll" |
    Copy-Item -Destination $outputPath -Force

$process = Start-Process -FilePath $stagedExecutable -WorkingDirectory $outputPath `
    -ArgumentList '-norun -nosound -nomusic -window -debug' -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) {
    Stop-Process -Id $process.Id
    throw "D1-only bootstrap timed out; see $outputPath/gamelog.txt"
}
$log = Get-Content -LiteralPath (Join-Path $outputPath "gamelog.txt") -Raw
if ($process.ExitCode -ne 0 -or
    -not $log.Contains("Initializing font system...") -or
    -not $log.Contains("Original D1 base assets initialized without D2 game data")) {
    throw "D1-only bootstrap failed (exit $($process.ExitCode)); see $outputPath/gamelog.txt"
}

$result = [ordered]@{
    passed = $true
    exitCode = $process.ExitCode
    assets = @("descent.hog", "descent.pig")
    stages = @("base_selection", "original_text", "graphics", "palette", "fonts", "original_game_data")
    gameplayValidated = $false
    soundPlaybackValidated = $false
}
$result | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $outputPath "bootstrap-result.json") -Encoding utf8
Write-Output "D1-only executable startup passed: $outputPath"
