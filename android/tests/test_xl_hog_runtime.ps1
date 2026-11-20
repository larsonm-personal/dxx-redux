#!/usr/bin/env pwsh
# Extended HOG ownership, disk/rewind and stock resource isolation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [string]$OriginalSurvey,
    [switch]$CheckOriginalRestores,
    [string]$LightingRuntime,
    [switch]$WallAnimation,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$expectedHog = '1b8b75b829a865191b3dd6686d3eababfd9de692464a1a5dcc5b1410e13bc6c0'
$expectedDescriptor = 'a4a1be94729745d47fb34b5b83beeff85ed4379f3126c30556a2351ea611f589'
$mission = 'xhogs'
$extension = '.mn2'
$scenario = 'test_xl_hog_runtime.jsonc'
if ($WallAnimation -and !$LightingRuntime) { throw 'Standoff requires the original Vignette distribution and its runtime data' }
if ($LightingRuntime) {
    if ($OriginalSurvey -or $CheckOriginalRestores) { throw 'Select one original test scenario' }
    $addons = @{
        'd2x.hog' = '97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6'
        'd2x-xl.hog' = 'e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6'
    }
    foreach ($name in $addons.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $LightingRuntime $name)).Hash -ne $addons[$name]) { throw "Requires the original $name" }
    }
    $expectedHog = '69d0d102c5551f164cefee6b7d79f576e5f0c314eb943e49902fe9dcb591f0e2'
    $expectedDescriptor = 'fd89030cc1a7cf40cdaf5730d14e0c19370b3bdd294af057c7f1fcd169f80d57'
    $mission = 'vignette-xl'
    $scenario = 'test_xl_original_lighting.jsonc'
} elseif ($OriginalSurvey) {
    # The original distributions include a classic, an XL and a D1 descriptor
    $originals = @{
        'chaothry' = @('XL: Chaos Theory', 6, 'd5d0462d8849fe0518444d98822c36b423cefc97cf773be79632cdf734e8d155', '2792be916036170f7daa3dfdd1921b2cbf9911b87ce4097a8d304f3f58d240e2')
        'klassics' = @('Klassics', 4, 'f194685bd8630097df10ea4100b4cda9a5942e14731d1c60344858f1c78faa54', '9ba2a8637150d8ee4198a2268ff86ae46566a87ab91f715da421e8b892a93e4d')
        'belials' = @('belial system', 17, '83a2c63c8933bd8baadf0de3abd5e93f1eb09c0ba4ad15e136f04cf0726e2ceb', '87911b738857419908027570db1ad304278fdc02646fe92369f7ddb64f007f16')
        'belialsxl' = @('XL: Belial System (XL)', 17, 'b8057f14b8811ebd8fef8440b946242ce1a610c86d5cf7a77571178c333c4e83', 'c00ca97337fb1a7dfc2ee88306563966f643f163f2120e2e0451baf3dd242341')
        'pluton' = @('D1: Plutonion Outbreak', 10, 'c92d7d14b79f9ce2a9ad8e9fb60eab4edb4dd10340486f836a4194883ff1c595', '0a7e3de17d60dc4e93a4a131a7d9b96c852f25204bfc5778d6db9f69927c454e')
    }
    $mission = [IO.Path]::GetFileNameWithoutExtension($Descriptor)
    if (!$originals.ContainsKey($mission)) { throw 'Unknown original HOG mission' }
    $title, $count, $expectedHog, $expectedDescriptor = $originals[$mission]
    if ($mission -eq 'pluton') { $extension = '.msn' }
    $rows = @((Get-Content -Raw -LiteralPath $OriginalSurvey | ConvertFrom-Json).rows | Where-Object mission -eq $mission)
    if ($rows.Count -ne $count -or @($rows.level | Sort-Object -Unique).Count -ne $count) { throw 'Expected every original ordinal exactly once' }
    if (@($rows | Where-Object outcome -ne 'loaded_ticked').Count) { throw 'The original survey must confirm entry for every requested device ordinal' }
}
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne $expectedDescriptor) {
    throw 'HOG or descriptor differs from the expected distribution'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-extended-hog-android'
if ($OutputDirectory) { $output = [IO.Path]::GetFullPath($OutputDirectory) }
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
if ($OriginalSurvey) {
    function Visit-Original($Row) {
        @{ action = 'set_debug'; field = 'xl_hog_probe'; value = "visit:$($Row.level):$($Row.level_file)"; post_delay_ms = 0 }
        @{ action = 'skip_briefing'; timeout_ms = 30000 }
        @{ action = 'wait_for'; expect = @{ in_game = $true; current_level_num = $Row.level; time_paused = $false; 'xl_pilot.runtime_active' = [bool]$Row.xl_active }; timeout_ms = 30000 }
    }
    function Invoke-OriginalCheckpoint([string]$Operation) {
        @{ action = 'set_debug'; field = 'xl_hog_probe'; value = $Operation; post_delay_ms = 0 }
    }
    $steps = @(
        @{ _info = @{ _standalone = $false; _owner = 'test_xl_hog_runtime'; games = @('d2') } }
        @{ action = 'enter_game'; game = 'd2' }
        @{ action = 'skip_intro'; timeout_ms = 30000 }
        @{ action = 'select'; text = 'player'; timeout_ms = 20000 }
        @{ action = 'select'; text = 'New game'; timeout_ms = 20000 }
        @{ action = 'select_mission'; text = $title; timeout_ms = 20000 }
        @{ action = 'select'; text = 'Ok'; timeout_ms = 20000 }
        @{ action = 'select'; text = 'Hotshot'; timeout_ms = 20000 }
        @{ action = 'skip_briefing'; timeout_ms = 30000 }
        @{ action = 'wait_for'; expect = @{ in_game = $true; current_level_num = 1; time_paused = $false }; timeout_ms = 30000 }
        foreach ($row in $rows) {
            Visit-Original $row
        }
        if ($CheckOriginalRestores -and $mission -in @('chaothry', 'belialsxl')) {
            $first = $rows | Where-Object level -eq 1
            $second = $rows | Where-Object level -eq 2
            Visit-Original $first
            Invoke-OriginalCheckpoint 'remember_disk'
            if ($mission -eq 'belialsxl') {
                $secret = $rows | Where-Object level -eq -2
                Invoke-OriginalCheckpoint 'remember_memory'
                Visit-Original $secret
                Invoke-OriginalCheckpoint 'restore_memory'
                Visit-Original $secret
                Invoke-OriginalCheckpoint 'remember_memory'
                Visit-Original $first
                Invoke-OriginalCheckpoint 'restore_memory'
            }
            Visit-Original $second
            Invoke-OriginalCheckpoint 'restore_disk'
        }
    )
    $scenario = Join-Path $output 'original.jsonc'
    $steps | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $scenario -Encoding utf8
}
if ($WallAnimation) {
    function Sample-Standoff {
        foreach ($sample in 1..24) {
            @{ action = 'set_debug'; field = 'xl_hog_probe'; value = 'standoff_frame'; post_delay_ms = 71 + ($sample % 7) * 17 }
        }
        @{ action = 'set_debug'; field = 'xl_hog_probe'; value = 'standoff_complete' }
    }
    $steps = @(
        @{ _info = @{ _standalone = $false; _owner = 'test_xl_hog_runtime'; games = @('d2') } }
        @{ action = 'enter_game'; game = 'd2' }
        @{ action = 'skip_intro'; timeout_ms = 30000 }
        @{ action = 'select'; text = 'player'; timeout_ms = 20000 }
        @{ action = 'select'; text = 'New game'; timeout_ms = 20000 }
        @{ action = 'select_mission'; text = 'Descent Vignettes-XL'; timeout_ms = 20000 }
        @{ action = 'select'; text = 'Ok'; timeout_ms = 20000 }
        @{ action = 'select'; text = 'Hotshot'; timeout_ms = 20000 }
        @{ action = 'skip_briefing'; timeout_ms = 30000 }
        @{ action = 'wait_for'; expect = @{ in_game = $true; time_paused = $false; current_level_num = 1 }; timeout_ms = 30000 }
        Sample-Standoff
        @{ action = 'set_debug'; field = 'xl_hog_probe'; value = 'remember_disk' }
        @{ action = 'set_debug'; field = 'xl_hog_probe'; value = 'visit:2:initiati.rl2'; post_delay_ms = 0 }
        @{ action = 'skip_briefing'; timeout_ms = 30000 }
        @{ action = 'wait_for'; expect = @{ current_level_num = 2; 'xl_pilot.source_level_version' = 17 }; timeout_ms = 30000 }
        @{ action = 'set_debug'; field = 'xl_hog_probe'; value = 'restore_disk' }
        @{ action = 'wait_for'; expect = @{ in_game = $true; time_paused = $false; current_level_num = 1 }; timeout_ms = 30000 }
        Sample-Standoff
        @{ action = 'introspect' }
    )
    $scenario = Join-Path $output 'wall-animation.jsonc'
    $steps | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $scenario -Encoding utf8
}
function Invoke-HogAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Hog adb failed: $result" }
    return $result
}
function Invoke-HogPrivate {
    param([string]$Command)
    Invoke-HogAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-extended-hog-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    "files/d2x-redux/missions/$mission.hog", "files/d2x-redux/missions/$mission$extension")
if ($LightingRuntime) {
    $paths += @($addons.Keys | ForEach-Object { "files/imported/sets/default/$_" })
}
$ready = $false
try {
    if ($LightingRuntime) {
        $activeSet = (Invoke-HogPrivate 'cat files/file_sets.json' | Out-String | ConvertFrom-Json).active
        if ($activeSet -ne 'default') { throw 'Original lighting fixture requires the default retail data set' }
    }
    Invoke-HogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-HogPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-HogPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-HogPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-HogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    if ($LightingRuntime) {
        foreach ($name in $addons.Keys) {
            $privatePath = "files/imported/sets/default/$name"
            Invoke-HogAdb -Arguments @('push', (Resolve-Path -LiteralPath (Join-Path $LightingRuntime $name)).Path, "/data/local/tmp/xl-light-$name") | Out-Null
            Invoke-HogPrivate "cp /data/local/tmp/xl-light-$name $privatePath" | Out-Null
            $installed = [string](Invoke-HogPrivate "sha256sum $privatePath")
            if ($installed.Split(' ')[0] -ne $addons[$name]) { throw "Installed $name differs" }
        }
    }
    Invoke-HogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-hog.hog') | Out-Null
    Invoke-HogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-hog.mn2') | Out-Null
    Invoke-HogPrivate "mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-hog.hog files/d2x-redux/missions/$mission.hog && cp /data/local/tmp/xl-hog.mn2 files/d2x-redux/missions/$mission$extension" | Out-Null
    Invoke-HogAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scenario -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-HogAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-HogPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-HogPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-HogPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Hog runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Hog runtime result' }
    Write-Host "PASS: HOG runtime for $mission ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-HogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-HogPrivate "rm -rf $path" | Out-Null }
            Invoke-HogPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-HogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Hog test state restoration differs; backup retained at $backup" }
            Invoke-HogPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-HogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-hog.hog', '/data/local/tmp/xl-hog.mn2') | Out-Null
            if ($LightingRuntime) {
                foreach ($name in $addons.Keys) { Invoke-HogAdb -Arguments @('shell', 'rm', '-f', "/data/local/tmp/xl-light-$name") | Out-Null }
            }
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
