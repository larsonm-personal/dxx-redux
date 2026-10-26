#!/usr/bin/env pwsh
# Required private originals; the nightly controller owns retail provisioning
param([string]$Serial = $env:ANDROID_SERIAL)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$repo = Split-Path (Split-Path $PSScriptRoot)
$stage = Join-Path $repo 'game_data/retail-ci/xl'
& python3 "$PSScriptRoot/xl_private_assets.py" --stage $stage
if ($LASTEXITCODE -ne 0) { throw 'Required pinned XL fixtures are missing or changed' }
$work = Join-Path $repo 'temp/xl-original-jvm'
& python3 "$PSScriptRoot/run_xl_originals.py" --repo $repo --work $work --jvm-only
if ($LASTEXITCODE -ne 0) { throw 'Required original XL JVM test failed' }
& "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_original_pilot.jsonc -Game d2 -Serial $Serial -LeaveRunning -TimeoutSeconds 120
if ($LASTEXITCODE -ne 0) { throw 'Required isolated D2 pilot setup failed' }
foreach ($case in @(
        @{ Runner = 'test_xl_water_runtime.ps1'; Mission = 'spotlight/rtarena' },
        @{ Runner = 'test_xl_observatory_runtime.ps1'; Mission = 'kruel/obsrv-xl' },
        @{ Runner = 'test_xl_konflict_runtime.ps1'; Mission = 'spotlight/konflict' }
    )) {
    $mission = Join-Path $stage "missions/$($case.Mission)"
    & (Join-Path $PSScriptRoot $case.Runner) -Serial $Serial -Hog "$mission.hog" -Descriptor "$mission.mn2"
    if ($LASTEXITCODE -ne 0) { throw 'Required original XL Android runtime test failed' }
}
& "$PSScriptRoot/test_scoped_static_textures.ps1" -Serial $Serial -OriginalTexture (Join-Path $stage 'adapter/ceil002.tga')
if ($LASTEXITCODE -ne 0) { throw 'Required original texture renderer witness failed' }
Write-Host 'PASS: original XL JVM, water, Observatory, Konflict and scoped texture renderer owners'
