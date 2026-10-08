#!/usr/bin/env pwsh
# Actual native robot-station mode transitions for both engines
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_env.ps1')
if (-not $Serial) { throw 'Select the Android test serial explicitly' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot ('android/temp/matcen-stations/run_' + [guid]::NewGuid().ToString('N')) }
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $repoRoot $OutputDirectory }
$native = @{} + $PSBoundParameters
$native.OutputDirectory = $OutputDirectory
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture matcen_stations -ExpectedCases 36 @native
if ($LASTEXITCODE -ne 0) { throw 'Native matcen station fixture failed' }
$depBase = (Get-Content (Join-Path $repoRoot 'dependency_base.txt') -First 1).Trim()
$adb = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir 'platform-tools' -ToolName 'adb'
foreach ($game in @('d1', 'd2')) {
    & $adb -s $Serial logcat -c
    if ($LASTEXITCODE -ne 0) { throw 'Clear Android logcat failed' }
    & (Get-Command pwsh -ErrorAction Stop).Source -NoProfile -File (Join-Path $repoRoot 'android/helpers/run_test.ps1') -Serial $Serial -ScriptName test_matcen_save_restore.jsonc -Game $game *> (Join-Path $OutputDirectory "$game-save.log")
    if ($LASTEXITCODE -ne 0) { throw "$game matcen save/restore failed; see $OutputDirectory" }
    $resultText = & $adb -s $Serial shell run-as com.dxxredux.app cat files/automation_result.json
    if ($LASTEXITCODE -ne 0) { throw "$game durable automation result missing" }
    $result = $resultText | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw "$game durable save/restore result is not complete PASS" }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory "$game-save-result.json"), ($result | ConvertTo-Json) + "`n", [Text.UTF8Encoding]::new($false))
    Write-Host "$game matcen save/restore passed: $($result.steps_completed) steps"
}
