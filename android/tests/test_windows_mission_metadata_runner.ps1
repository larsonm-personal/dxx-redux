#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Assert-Contains([string]$Text, [string]$Needle, [string]$Message) {
    if (-not $Text.Contains($Needle, [StringComparison]::Ordinal)) { throw $Message }
}

function Invoke-NoisyBuildFixture {
    $powershell = (Get-Process -Id $PID).Path
    & $powershell -NoProfile -NonInteractive -Command "Write-Output 'build stdout'; [Console]::Error.WriteLine('build stderr')" 2>&1 |
        ForEach-Object { Write-Host ([string]$_) }
    if ($LASTEXITCODE -ne 0) { throw "Noisy build fixture failed with exit code $LASTEXITCODE" }
    return 'metadata-cli.bat'
}

$publicRunner = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata.ps1'))
$hostRunner = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata_host.ps1'))
$nativeAnalyzer = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\app\src\main\cpp\jni_level_metadata.cpp'))
$projection = [IO.File]::ReadAllText((Join-Path $repoRoot 'android\mission-metadata-core\src\main\kotlin\com\dxxredux\app\MissionMetadataProjection.kt'))
$d1Cmake = [IO.File]::ReadAllText((Join-Path $repoRoot 'd1\main\CMakeLists.txt'))
$d2Cmake = [IO.File]::ReadAllText((Join-Path $repoRoot 'd2\main\CMakeLists.txt'))

Assert-Contains $publicRunner "[string]`$Engine = 'Windows'" 'Windows must be the default metadata engine'
Assert-Contains $publicRunner "`$Engine -eq 'Windows'" 'The public runner must dispatch to the Windows path'
Assert-Contains $hostRunner 'dxx-redux-$Game-metadata-worker' 'The host runner must use the shared native worker'
Assert-Contains $hostRunner '--directory-precedence' 'The host runner must load variant precedence from shared Kotlin'
Assert-Contains $hostRunner "op = 'project'" 'The host runner must use the shared Kotlin checked-in projection'
Assert-Contains $hostRunner "op = 'descriptor'" 'The host runner must use the shared Kotlin descriptor parser'
Assert-Contains $nativeAnalyzer 'row["route_required_key_mask"]' 'The native analyzer must serialize required key masks'
Assert-Contains $nativeAnalyzer 'row["route_completing_key_mask_set"]' 'The native analyzer must serialize completing key-mask sets'
Assert-Contains $projection 'level.requiredInt("route_required_key_mask")' 'The projection must reject a missing required key mask'
Assert-Contains $projection 'level.requiredInt("route_completing_key_mask_set")' 'The projection must reject a missing completing key-mask set'
Assert-Contains $d1Cmake 'jni_level_metadata.cpp' 'D1 worker must compile the Android analyzer source'
Assert-Contains $d2Cmake 'jni_level_metadata.cpp' 'D2 worker must compile the Android analyzer source'

$containedBuildOutputCount = [regex]::Matches(
    $hostRunner,
    '2>&1\s*\|\s*ForEach-Object \{ Write-Host \(\[string\]\$_\) \}'
).Count
if ($containedBuildOutputCount -lt 5) {
    throw "All five Gradle and native build invocations must contain console output; found $containedBuildOutputCount"
}

$fixtureResult = @(Invoke-NoisyBuildFixture)
if ($fixtureResult.Count -ne 1 -or $fixtureResult[0] -ne 'metadata-cli.bat') {
    throw "Noisy build output leaked into the function result: $($fixtureResult -join ', ')"
}

Write-Host 'Windows mission metadata runner wiring passed' -ForegroundColor Green
