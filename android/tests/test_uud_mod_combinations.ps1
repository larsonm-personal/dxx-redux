#!/usr/bin/env pwsh
# Actual launcher imports, engine restarts and mission re-entry with real recommended packs
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [switch]$Install,
    [string]$ApkPath,
    [string]$TexturePackDirectory,
    [ValidateSet('d1', 'd2', 'd1-in-d2')][string[]]$Modes = @('d1', 'd2', 'd1-in-d2')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$logSession = $null
try {
    $logSession = Start-AndroidTestDebugLogSession
    $repo = Split-Path (Split-Path $PSScriptRoot)
    if (!$TexturePackDirectory) { $TexturePackDirectory = Join-Path $repo 'game_data/mods/d2x-xl' }
    $output = Join-Path $repo ('android/temp/uud-combinations/run_' + [guid]::NewGuid().ToString('N'))
    & (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
    New-Item -ItemType Directory -Force $output | Out-Null
    if ($Install) {
        if (!$ApkPath -or !(Test-Path -LiteralPath $ApkPath)) { throw 'Provide a current APK with -ApkPath' }
        $result = Adb -AdbArgs @('install', '-r', '-t', $ApkPath) -Seconds 180
        if ($result -notmatch 'Success') { throw "Install failed: $result" }
    }
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/uud-import') | Out-Null
    $packs = @('game_data/mods/d2x-xl/d1xr-hires.dxa')
    foreach ($game in @('d1', 'd2')) {
        $packs += "game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uu${game}tp-textures.dxa"
        $packs += "game_data/mods/d2x-xl/$game-hires-256-textures-ktx2.dxa"
        $packs += "game_data/mods/d2x-xl/$game-hires-sounds.dxa"
    }
    foreach ($pack in $packs) {
        $path = if ($pack -like '*-textures-ktx2.dxa') { Join-Path $TexturePackDirectory (Split-Path $pack -Leaf) } else { Join-Path $repo $pack }
        if (!(Test-Path -LiteralPath $path)) { throw "Missing real pack: $path" }
        $name = Split-Path $path -Leaf
        Adb -AdbArgs @('push', $path, "/data/local/tmp/$name") -Seconds 120 | Out-Null
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$name", "files/uud-import/$name") -Seconds 120 | Out-Null
        Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$name") | Out-Null
    }
    foreach ($mode in $Modes) {
        $contentGame = if ($mode -eq 'd2') { 'd2' } else { 'd1' }
        $game = if ($mode -eq 'd1') { 'd1' } else { 'd2' }
        $uud = "uu${contentGame}tp-textures.dxa"
        $hires = "$contentGame-hires-256-textures-ktx2.dxa"
        foreach ($phase in @('baseline', 'uud', 'uud-first', 'hires-first', 'off')) {
            $first = if ($phase -eq 'hires-first') { $hires } else { $uud }
            $second = if ($phase -eq 'hires-first') { $uud } else { $hires }
            $combined = $phase -in @('uud-first', 'hires-first')
            $params = @{
                MODE = $mode; FIRST = $first; SECOND = $second; SOUND = "$contentGame-hires-sounds.dxa"
                FIRST_COMMAND = $(if ($phase -in @('baseline', 'off')) { 'delete_mod' } else { 'import_mod' })
                SECOND_COMMAND = $(if ($combined) { 'import_mod' } else { 'delete_mod' })
                EXTRAS_COMMAND = $(if ($combined) { 'import_mod' } else { 'delete_mod' })
            }
            Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
            Adb -AdbArgs @('logcat', '-c') | Out-Null
            & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_uud_mod_combinations.jsonc -Game $game -Serial $env:ANDROID_SERIAL -LeaveRunning -TimeoutSeconds 300 -Params $params
            $testResult = $LASTEXITCODE
            Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json') | Set-Content -LiteralPath (Join-Path $output "$mode-$phase.json") -Encoding utf8
            Adb -AdbArgs @('logcat', '-d') -Seconds 30 | Set-Content -LiteralPath (Join-Path $output "$mode-$phase.log") -Encoding utf8
            if ($testResult -ne 0) { throw "UUD scenario failed: $mode $phase; $output" }
        }
    }
    & python (Join-Path $PSScriptRoot 'verify_uud_mod_combinations.py') $output (Join-Path $repo 'game_data/mods/d2x-xl') --modes $Modes
    if ($LASTEXITCODE -ne 0) { throw "UUD combination verification failed: $output" }
    Write-Host "UUD launcher combinations passed: $output"
} finally {
    Stop-AndroidTestDebugLogSession -Session $logSession
}
