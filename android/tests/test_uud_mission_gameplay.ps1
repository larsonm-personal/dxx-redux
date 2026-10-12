#!/usr/bin/env pwsh
# Author mission acceptance through normal launcher import and real rendered level references
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [switch]$Install,
    [string]$ApkPath,
    [string]$TexturePackDirectory,
    [ValidateSet('author', 'extras')][string]$Variant = 'author',
    [ValidateSet('none', 'uud-first', 'hires-first')][string]$HighResOrder = 'none',
    [ValidateRange(1, 7)][int[]]$Levels = @(1, 2, 3, 4, 5, 6, 7)
)
$ErrorActionPreference = 'Stop'
if ($HighResOrder -ne 'none' -and $Variant -ne 'extras') { throw 'High-res combination checks require -Variant extras' }
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
. (Join-Path $PSScriptRoot '../helpers/uud_gameplay_fixture.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
if (!$TexturePackDirectory) { $TexturePackDirectory = Join-Path $repo 'game_data/mods/d2x-xl' }
$source = Join-Path $repo 'game_data/mods/xfing/dxx_tp'
$output = Join-Path $repo ("android/temp/uud-gameplay/$Variant-$HighResOrder/run_" + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
New-Item -ItemType Directory -Force $output | Out-Null
if ($Install) {
    if (!$ApkPath -or !(Test-Path -LiteralPath $ApkPath)) { throw 'Provide a current APK with -ApkPath' }
    $result = Adb -AdbArgs @('install', '-r', '-t', $ApkPath) -Seconds 180
    if ($result -notmatch 'Success') { throw "Install failed: $result" }
}
function Copy-AppInput([string]$Path, [string]$Destination) {
    $staging = '/data/local/tmp/uud-gameplay-' + [guid]::NewGuid().ToString('N')
    Adb -AdbArgs @('push', $Path, $staging) -Seconds 120 | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $staging, $Destination) -Seconds 120 | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', $staging) | Out-Null
}
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/uud-gameplay', 'files/imported/sets/uud-demo') | Out-Null
$mission = Join-Path $output 'pogtest.zip'
if ($Variant -eq 'extras') {
    & python (Join-Path $repo 'android/helpers/build_uud_extra_mission.py') (Join-Path $source 'uud2tp') $mission
    if ($LASTEXITCODE -ne 0) { throw 'Extra-texture fixture construction failed' }
    $fixture = Get-Content -LiteralPath ([IO.Path]::ChangeExtension($mission, '.json')) -Raw | ConvertFrom-Json
} else {
    $zip = [IO.Compression.ZipFile]::Open($mission, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in @('pogtest.hog', 'pogtest.mn2', 'descent.256')) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $source "uud2tp/$name"), $name) | Out-Null
        }
    } finally { $zip.Dispose() }
}
Copy-AppInput $mission 'files/uud-gameplay/pogtest.zip'
Copy-AppInput (Join-Path $source 'tmp/plain_texture_dxa/uud2tp-textures.dxa') 'files/uud-gameplay/uud2tp-textures.dxa'
$hires = 'd2-hires-256-textures-ktx2.dxa'
if ($HighResOrder -ne 'none') {
    Copy-AppInput (Join-Path $TexturePackDirectory $hires) "files/uud-gameplay/$hires"
}
# The optional seventh palette is D2 data; keep it separate from D1's same-named PIG
Copy-AppInput (Join-Path $source 'baseline-d2-descent/original-2015/DESCENT.PIG') 'files/imported/sets/uud-demo/descent.pig'
$palettes = @('water', 'groupa', 'fire', 'ice', 'alien1', 'alien2', 'descent')
$levelFiles = @('l2watr.rl2', 'l1grpa.rl2', 'l3fire.rl2', 'l4ice.rl2', 'l5aln1.rl2', 'l6aln2.rl2', 'l7dsc1.rl2')
$missionName = if ($Variant -eq 'extras') { 'UUD extras' } else { 'pogtest' }
foreach ($level in $Levels) {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    $params = @{
        LEVEL = [string]$level; MODE = 'd2'; SET = 'uud-demo'; MISSION = $missionName
        FIRST = $(if ($HighResOrder -eq 'hires-first') { $hires } else { 'uud2tp-textures.dxa' })
        SECOND = $(if ($HighResOrder -eq 'hires-first') { 'uud2tp-textures.dxa' } else { $hires })
        SECOND_COMMAND = $(if ($HighResOrder -eq 'none') { 'delete_mod' } else { 'import_mod' })
    }
    & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_uud_mission_gameplay.jsonc -Game d2 -Serial $env:ANDROID_SERIAL -LeaveRunning -TimeoutSeconds 300 -Params $params
    $testResult = $LASTEXITCODE
    $json = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json')
    $json | Set-Content -LiteralPath (Join-Path $output "level-$level.json") -Encoding utf8
    Adb -AdbArgs @('logcat', '-d') -Seconds 30 | Set-Content -LiteralPath (Join-Path $output "level-$level.log") -Encoding utf8
    if ($testResult -ne 0) { throw "Author mission failed: level $level; $output" }
    $state = $json | ConvertFrom-Json
    if ($state.asset_trace.current_pigfile -ine ($palettes[$level - 1] + '.pig')) { throw "Wrong author palette at level $level" }
    $references = @($state.level_scene.mod_texture_references.PSObject.Properties | ForEach-Object Value)
    if (!$references.Count) { throw "Author mission has no applied mod texture references: level $level" }
    $steps = @()
    if ($HighResOrder -ne 'none') {
        # The fixture fills the spawn room with UUD extras; visit an unchanged wall as well
        $wall = $state.level_scene.texture_references.'127'
        if (!$wall -or $wall.layer -ne 0) { throw 'Missing ordinary wall for high-res combination check' }
        $prefix = "level_scene.segment.sides[$($wall.side)].base_asset"
        $steps += @{ action = 'face_view'; segment = $wall.segment; side = $wall.side; distance = 7; post_delay_ms = 200 }
        $steps += @{ action = 'wait_for'; timeout_ms = 5000; expect = @{
                "$prefix.rendered" = $true
                "$prefix.render_width" = 256
                "$prefix.render_source" = @{ contains = $hires }
            }
        }
    }
    $targets = if ($Variant -eq 'extras') { $fixture.($levelFiles[$level - 1]).surfaces } else { $references }
    foreach ($row in $targets) {
        $layer = if ($row.layer -eq 1 -or $row.layer -eq 'overlay') { 'overlay' } else { 'base' }
        $prefix = "level_scene.segment.sides[$($row.side)].${layer}_asset"
        $steps += @{ action = 'face_view'; segment = $row.segment; side = $row.side; distance = 7; post_delay_ms = 200 }
        $steps += @{ action = 'wait_for'; timeout_ms = 5000; expect = @{
                'position.segment' = $row.segment
                "$prefix.texture" = $row.texture
                "$prefix.indexed_mod_owned" = $true
                "$prefix.rendered" = $true
                "$prefix.render_width" = 64
                "$prefix.render_height" = 64
                "$prefix.render_source" = @{ contains = 'uud2tp-textures.dxa' }
            }
        }
    }
    $monitor = $state.level_scene.texture_references.'374'
    if (!$monitor -or $monitor.layer -ne 1) { throw 'Author spider monitor missing' }
    $sidePath = "level_scene.segment.sides[$($monitor.side)]"
    $steps += @{ action = 'face_view'; segment = $monitor.segment; side = $monitor.side; distance = 7; post_delay_ms = 200 }
    $steps += @{ action = 'send_button'; button = 100; held = 1 }
    $steps += @{ action = 'wait_for'; timeout_ms = 8000; expect = @{ "$sidePath.overlay" = 910 } }
    $steps += @{ action = 'send_button'; button = 100; pressed = 0 }
    $steps += @{ action = 'assert'; expect = @{
            "$sidePath.overlay_asset.bitmap" = 2593
            "$sidePath.overlay_asset.indexed_mod_owned" = $true
            "$sidePath.overlay_asset.rendered" = $true
            "$sidePath.overlay_asset.render_source" = @{ contains = 'uud2tp-textures.dxa' }
        }
    }
    $steps += @{ action = 'introspect'; post_delay_ms = 300 }
    Invoke-UudGameplayProbe -Steps $steps -OutputDirectory $output -Name "level-$level-render-monitor" | Out-Null
    if ($Variant -eq 'extras') {
        Test-UudExtraGameplay -Fixture $fixture.($levelFiles[$level - 1]) -OutputDirectory $output -Level $level
    }
    Write-Host "Verified $Variant level ${level}: $($targets.Count) rendered surfaces and destroyed spider monitor"
}
Write-Host "UUD mission gameplay checks passed ($Variant): $output"
