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
    [switch]$SmokeFixture,
    [switch]$OriginalHub,
    [switch]$MissionFlow,
    [switch]$CampaignEnd,
    [switch]$RevisitFixture,
    [switch]$FlyoutFixture,
    [string]$D1Pig = '',
    [string]$SmokeImage = '',
    [ValidateRange(0, 100000)][int]$SceneSegment = 409,
    [ValidateRange(0, 5)][int]$SceneSide = 2,
    [ValidateRange(0.1, 1000)][double]$SceneDistance = 8
)
$ErrorActionPreference = 'Stop'
if ($FlyoutFixture -and (!$OriginalHub -or $SceneOnly -or $MissionFlow -or $CampaignEnd -or $RevisitFixture -or !$D1Pig)) { throw 'FlyoutFixture requires OriginalHub, retail D1Pig and its own mode' }
if ($FlyoutFixture -and (Get-FileHash -LiteralPath $D1Pig -Algorithm SHA256).Hash -ne '093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe') { throw 'Flyout fixture requires the retail D1 exit-model PIG' }
if ($RevisitFixture -and ($OriginalHub -or $SceneOnly -or $SecretFixture -or $SkyboxFixture -or $SmokeFixture -or $PumoMods)) { throw 'RevisitFixture requires its own derived mode' }
if ($MissionFlow -and (!$OriginalHub -or $SceneOnly)) { throw 'MissionFlow requires OriginalHub without SceneOnly' }
if ($CampaignEnd -and (!$OriginalHub -or $SceneOnly -or $MissionFlow)) { throw 'CampaignEnd requires OriginalHub and its own mode' }
if ($OriginalHub -and ($SmokeFixture -or $SkyboxFixture -or $SecretFixture -or !$PumoMods -or !$SmokeImage)) { throw 'OriginalHub requires original mod assets, smoke image and its own mode' }
if ($SmokeFixture -and ($SkyboxFixture -or $SecretFixture -or $SceneOnly -or !$PumoMods -or !$SmokeImage)) { throw 'SmokeFixture requires original mod assets, smoke image and its own fixture mode' }
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
if ($RevisitFixture) {
    $mission = 'pfrevis'
    $hogHash = 'b0e11a4dbcc8d94261bfb2b6e77a036e8b4d69384bdb5aaee4b8a9ed6d235111'
    $descriptorHash = 'b6636bd55f6db72df985d4490842651d1b89d52378b23dfeb6fb599eb08c6c78'
}
if ($SkyboxFixture) {
    $mission = 'pskybox'
    $hogHash = '69f56e52a45fb16f061290c115709086c416b88cecd1037e26d81c95ad8e89b7'
    $descriptorHash = 'fe8d90d8efd9efe2d270fdf6f90b1854171c15b8701ff6c9435df7bc94f45c7c'
}
if ($SmokeFixture) {
    $mission = 'psmoke'
    $hogHash = '9584308a740140b81950812e00779b5c5e92c35dac4b431852bf7e4ba811c326'
    $descriptorHash = 'd7a34f614d094e5f288f3b648b8da30aab6847fb87af47a1a11c50e2b5d77eea'
}
if (($SmokeFixture -or $OriginalHub) -and (Get-FileHash -LiteralPath $SmokeImage -Algorithm SHA256).Hash -ne '6fe8b321ff2cf6e3bc6edd1c1cf8d692b2ba7796325bdb64254a2556e4d1e6ac') { throw 'Requires original XL smoke atlas' }
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
if ($SmokeFixture) { $output = Join-Path $PSScriptRoot '../../temp/xl-smoke-authored-android' }
if ($OriginalHub) { $output = Join-Path $PSScriptRoot '../../temp/xl-original-hub-android' }
if ($OriginalHub -and $SceneOnly) { $output = Join-Path $PSScriptRoot '../../temp/xl-original-hub-scene' }
if ($MissionFlow) { $output = Join-Path $PSScriptRoot '../../temp/xl-original-flow-android' }
if ($CampaignEnd) { $output = Join-Path $PSScriptRoot '../../temp/xl-original-end-android' }
if ($RevisitFixture) { $output = Join-Path $PSScriptRoot '../../temp/xl-revisit-android' }
if ($FlyoutFixture) { $output = Join-Path $PSScriptRoot '../../temp/xl-flyout-cache-android' }
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
if ($FlyoutFixture) { $paths += 'files/d2x-redux/descent.pig' }
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
    $stagedHog = (Resolve-Path -LiteralPath $Hog).Path
    if ($FlyoutFixture) {
        # Add optional scenery to an isolated archive; original geometry stays intact
        $stream = [IO.MemoryStream]::new()
        $writer = [IO.BinaryWriter]::new($stream)
        try {
            $be32 = { param([int]$Value) $bytes = [BitConverter]::GetBytes($Value); [Array]::Reverse($bytes); $writer.Write($bytes) }
            $writer.Write([Text.Encoding]::ASCII.GetBytes('FORM'))
            & $be32 4912
            $writer.Write([Text.Encoding]::ASCII.GetBytes('PBM BMHD'))
            & $be32 20
            $writer.Write([byte[]]@(0, 64, 0, 64, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 1, 1, 0, 64, 0, 64))
            $writer.Write([Text.Encoding]::ASCII.GetBytes('CMAP'))
            & $be32 768
            for ($i = 0; $i -lt 256; $i++) { $writer.Write([byte[]]@($i, $i, $i)) }
            $writer.Write([Text.Encoding]::ASCII.GetBytes('BODY'))
            & $be32 4096
            $writer.Write([byte[]](, ( [byte]32 ) * 4096))
            $bitmap = $stream.ToArray()
        } finally { $writer.Dispose(); $stream.Dispose() }
        $stagedHog = Join-Path $output 'flyout.hog'
        Copy-Item -LiteralPath $Hog -Destination $stagedHog -Force
        $archive = [IO.BinaryWriter]::new([IO.File]::Open($stagedHog, [IO.FileMode]::Append))
        try {
            $append = {
                param([string]$Name, [byte[]]$Bytes)
                $nameBytes = [byte[]]::new(13)
                [Text.Encoding]::ASCII.GetBytes($Name).CopyTo($nameBytes, 0)
                $archive.Write($nameBytes)
                $archive.Write([int]$Bytes.Length)
                $archive.Write($Bytes)
            }
            & $append 'xlsky.bbm' $bitmap
            & $append 'mvlev1.end' ([Text.Encoding]::ASCII.GetBytes("xlsky.bbm`nxlsky.bbm`n5,10`n1`nxlsky.bbm`n205,0`n200`n90,40`n"))
        } finally { $archive.Dispose() }
        Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $D1Pig).Path, '/data/local/tmp/xl-pog-d1.pig') | Out-Null
        Invoke-PogPrivate 'cp /data/local/tmp/xl-pog-d1.pig files/d2x-redux/descent.pig' | Out-Null
    }
    Invoke-PogAdb -Arguments @('push', $stagedHog, '/data/local/tmp/xl-pog.hog') | Out-Null
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
    if ($SmokeFixture -or $OriginalHub) {
        Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $SmokeImage).Path, '/data/local/tmp/xl-pog-smoke.tga') | Out-Null
        Invoke-PogPrivate "cp /data/local/tmp/xl-pog-smoke.tga files/d2x-redux/mods/$mission/textures/d2x-xl/smoke.tga" | Out-Null
    }
    Invoke-PogAdb -Arguments @('logcat', '-c') | Out-Null
    $scriptName = if ($PumoMods) { 'test_xl_pumo_particles.jsonc' } else { 'test_xl_pog_runtime.jsonc' }
    if ($SecretFixture) { $scriptName = 'test_xl_particle_secret.jsonc' }
    if ($SkyboxFixture) { $scriptName = 'test_xl_skybox_authored.jsonc' }
    if ($SmokeFixture) { $scriptName = 'test_xl_smoke_authored.jsonc' }
    if ($OriginalHub) {
        $steps = Get-Content "$PSScriptRoot/../game_scripts/test_xl_smoke_authored.jsonc" -Raw | ConvertFrom-Json -AsHashtable
        $starts = @($steps | Where-Object { $_.action -eq 'set_debug' -and $_.field -eq 'xl_pog_probe' -and $_.value -eq 'skybox_start_smoke' })
        if ($starts.Count -ne 1) { throw 'Smoke lifecycle no longer has exactly one startup operation' }
        $starts[0].value = 'skybox_start_hub'
        $scriptName = [IO.Path]::GetFullPath((Join-Path $output 'original-hub.jsonc'))
        ConvertTo-Json -InputObject $steps -Depth 16 | Set-Content -LiteralPath $scriptName -Encoding utf8
    }
    if ($MissionFlow) { $scriptName = 'test_xl_pumo_flow.jsonc' }
    if ($CampaignEnd) { $scriptName = 'test_xl_pumo_end.jsonc' }
    if ($RevisitFixture) { $scriptName = 'test_xl_mission_revisit.jsonc' }
    if ($FlyoutFixture) { $scriptName = 'test_xl_flyout_cache.jsonc' }
    if ($SceneOnly) {
        # Reuse the original owner's startup and first scene assertion
        $sourcePath = if ([IO.Path]::IsPathRooted($scriptName)) { $scriptName } else { "$PSScriptRoot/../game_scripts/$scriptName" }
        $sourceSteps = Get-Content $sourcePath -Raw | ConvertFrom-Json -AsHashtable
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
        $sceneSteps.Add(@{ action = 'set_debug'; field = 'android_game_request'; value = 'resume'; post_delay_ms = 1500 })
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
    if ($FlyoutFixture) { Write-Host "PASS: Derived intact rendered flyout and disk-cache revisit ($($result.total_steps) steps)" }
    elseif ($RevisitFixture) { Write-Host "PASS: Derived Android disk revisits, mission timeline, player/world and GPU resource restoration ($($result.total_steps) steps)" }
    elseif ($CampaignEnd) { Write-Host "PASS: Original Pumo destination29 ending and menu return ($($result.total_steps) steps); controlled dispatch" }
    elseif ($MissionFlow) { Write-Host "PASS: Original Pumo controlled authored exits and hub checkpoint recovery ($($result.total_steps) steps); no earned traversal" }
    elseif ($OriginalHub -and $SceneOnly) { Write-Host "PASS: Unchanged original Pumo hub fixed-view live smoke scene ($($result.total_steps) steps)" }
    elseif ($OriginalHub) { Write-Host "PASS: Unchanged original Pumo hub, smoke/skybox/GPU/resources/save/context/stock lifecycle ($($result.total_steps) steps)" }
    elseif ($SmokeFixture) { Write-Host "PASS: Original hub smoke in derived outdoors-isolated fixture, GPU/resources/save/context/stock lifecycle ($($result.total_steps) steps)" }
    elseif ($SkyboxFixture) { Write-Host "PASS: Derived authored skybox resources, disk restore, context recreation and stock isolation ($($result.total_steps) steps)" }
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
            if ($FlyoutFixture) { Invoke-PogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-pog-d1.pig') | Out-Null }
            Invoke-PogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-pog.hog', '/data/local/tmp/xl-pog.mn2', '/data/local/tmp/xl-pog.png', '/data/local/tmp/xl-pog-fire.tga', '/data/local/tmp/xl-pog-smoke.tga', '/data/local/tmp/xl-pog-mods.tar') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
