#!/usr/bin/env pwsh
# Original POG application, GPU readback and lifecycle; no campaign traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [Parameter(Mandatory)][string]$FireImage,
    [string]$PumoMods = '',
    [switch]$SceneOnly,
    [switch]$SecretFixture,
    [switch]$SkyboxFixture,
    [ValidateRange(0, 100000)][int]$SceneSegment = 409,
    [ValidateRange(0, 5)][int]$SceneSide = 2,
    [ValidateRange(0.1, 1000)][double]$SceneDistance = 8
)
$ErrorActionPreference = 'Stop'
if ($SkyboxFixture -and ($SecretFixture -or $SceneOnly -or !$PumoMods)) { throw 'SkyboxFixture requires original Pumo mod assets and its own fixture mode' }
if ($SecretFixture -and ($SceneOnly -or $PumoMods)) { throw 'SecretFixture is a separate derived control' }
if ($SceneOnly -and !$PumoMods) { throw 'SceneOnly requires the original Pumo package' }
$mission = if ($PumoMods) { 'pmines' } else { 'rtarena' }
$hogHash = if ($PumoMods) { '6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043' } else { 'adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e' }
$descriptorHash = if ($PumoMods) { '45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd' } else { '14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71' }
if ($SecretFixture) {
    $mission = 'pfsecret'
    $hogHash = 'b0e11a4dbcc8d94261bfb2b6e77a036e8b4d69384bdb5aaee4b8a9ed6d235111'
    $descriptorHash = 'af3d59ab6a9b13600ab05aae3102e7776edd2fe49c9f3dac2c825bdb878702a2'
}
if ($SkyboxFixture) {
    $mission = 'pskybox'
    $hogHash = '69f56e52a45fb16f061290c115709086c416b88cecd1037e26d81c95ad8e89b7'
    $descriptorHash = 'fe8d90d8efd9efe2d270fdf6f90b1854171c15b8701ff6c9435df7bc94f45c7c'
}
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $hogHash -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne $descriptorHash) {
    throw "Requires the unchanged original $mission HOG and descriptor"
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
if ((Get-FileHash -LiteralPath $FireImage -Algorithm SHA256).Hash -ne 'b7bd821c02ba917a4cb0ff503f2e83d4e2007c19f0450728525df2f3af7f738f') {
    throw 'Requires the unchanged original XL smokingfire atlas'
}
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot $(if ($SceneOnly) { '../../temp/xl-pumo-particle-scene' } elseif ($PumoMods) { '../../temp/xl-pumo-particles-android' } else { '../../temp/xl-pog-android' })
if ($SecretFixture) { $output = Join-Path $PSScriptRoot '../../temp/xl-particle-secret-android' }
if ($SkyboxFixture) { $output = Join-Path $PSScriptRoot '../../temp/xl-skybox-authored-android' }
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach ($name in @('result.json', 'state.json', 'steps.jsonl', 'logcat.txt', 'render.png')) {
    Remove-Item -LiteralPath (Join-Path $output $name) -Force -ErrorAction SilentlyContinue
}
function Invoke-PogAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "POG adb failed: $result" }
    return $result
}
function Invoke-PogPrivate {
    param([string]$Command)
    Invoke-PogAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-pog-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/graphics_safety.json', 'files/.graphics_safety.lock',
    "files/d2x-redux/missions/$mission.hog", "files/d2x-redux/missions/$mission.mn2",
    "files/d2x-redux/mods/$mission")
if ($SecretFixture) { $paths += @('files/d2x-redux/secret.sgb', 'files/d2x-redux/secret.sgc') }
$ready = $false
try {
    Invoke-PogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-PogPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-PogPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-PogPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-PogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-pog.hog') | Out-Null
    Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-pog.mn2') | Out-Null
    Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $FireImage).Path, '/data/local/tmp/xl-pog-fire.tga') | Out-Null
    Invoke-PogPrivate "mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-pog.hog files/d2x-redux/missions/$mission.hog && cp /data/local/tmp/xl-pog.mn2 files/d2x-redux/missions/$mission.mn2" | Out-Null
    if ($PumoMods) {
        $mods = (Resolve-Path -LiteralPath $PumoMods).Path
        if ((Split-Path -Leaf $mods) -ne 'pmines') { throw 'PumoMods must name the original mods/pmines directory' }
        & tar -cf "$output/pumo-mods.tar" -C $mods .
        if ($LASTEXITCODE -ne 0) { throw 'Cannot stage original Pumo mod resources' }
        Invoke-PogAdb -Arguments @('push', "$output/pumo-mods.tar", '/data/local/tmp/xl-pog-mods.tar') | Out-Null
        Invoke-PogPrivate "mkdir -p files/d2x-redux/mods/$mission && tar -xf /data/local/tmp/xl-pog-mods.tar -C files/d2x-redux/mods/$mission" | Out-Null
    }
    Invoke-PogPrivate "mkdir -p files/d2x-redux/mods/$mission/textures/d2x-xl && cp /data/local/tmp/xl-pog-fire.tga files/d2x-redux/mods/$mission/textures/d2x-xl/smokingfire.tga" | Out-Null
    Invoke-PogAdb -Arguments @('logcat', '-c') | Out-Null
    $scriptName = if ($PumoMods) { 'test_xl_pumo_particles.jsonc' } else { 'test_xl_pog_runtime.jsonc' }
    if ($SecretFixture) { $scriptName = 'test_xl_particle_secret.jsonc' }
    if ($SkyboxFixture) { $scriptName = 'test_xl_skybox_authored.jsonc' }
    if ($SceneOnly) {
        # Reuse the original owner's startup and first scene assertion
        $sourceSteps = Get-Content "$PSScriptRoot/../game_scripts/$scriptName" -Raw | ConvertFrom-Json -AsHashtable
        $sceneSteps = [Collections.Generic.List[object]]::new()
        $found = $false
        foreach ($step in $sourceSteps) {
            if ($step.action -eq 'face_view') {
                $step.segment = $SceneSegment
                $step.side = $SceneSide
                $step.distance = $SceneDistance
            }
            $sceneSteps.Add($step)
            if ($step.action -eq 'assert' -and $step.expect.ContainsKey('xl_pog.scene.changed')) { $found = $true; break }
        }
        if (!$found) { throw 'Original owner no longer contains its initial scene assertion' }
        $sceneSteps.Add(@{ action = 'introspect' })
        $scriptName = [IO.Path]::GetFullPath((Join-Path $output 'scene.jsonc'))
        ConvertTo-Json -InputObject @($sceneSteps.ToArray()) -Depth 16 | Set-Content -LiteralPath $scriptName -Encoding utf8
    }
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scriptName -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-PogAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-PogPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-PogPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-PogPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "POG runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete POG runtime result' }
    Invoke-PogAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-pog.png') | Out-Null
    Invoke-PogAdb -Arguments @('pull', '/data/local/tmp/xl-pog.png', "$output/render.png") | Out-Null
    if ($SkyboxFixture) { Write-Host "PASS: Derived authored skybox resources, disk restore, context recreation and stock isolation ($($result.total_steps) steps)" }
    elseif ($SecretFixture) { Write-Host "PASS: Derived particle automatic secret save/return and headed GPU resources ($($result.total_steps) steps)" }
    elseif ($SceneOnly) { Write-Host "PASS: Original Pumo controlled scene $SceneSegment/$SceneSide ($($result.total_steps) steps)" }
    else { Write-Host "PASS: Original $mission GPU resources, disk/rewind, context loss and stock transition ($($result.total_steps) steps)" }
} finally {
    try {
        Invoke-PogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-PogPrivate "rm -rf $path" | Out-Null }
            Invoke-PogPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-PogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "POG test state restoration differs; backup retained at $backup" }
            Invoke-PogPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-PogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-pog.hog', '/data/local/tmp/xl-pog.mn2', '/data/local/tmp/xl-pog.png', '/data/local/tmp/xl-pog-fire.tga', '/data/local/tmp/xl-pog-mods.tar') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
