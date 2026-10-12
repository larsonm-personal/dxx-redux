#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"

. "$PSScriptRoot/../helpers/shareware_test_data.ps1"
Set-StrictMode -Off

$names = @(
    'idle_screen_saver', 'independent_trigger_axes', 'pilot_long_hold_delete_unified',
    'gles3_shim_vbo_arrays', 'abort_game_to_main_menu_d2',
    'android_saveload_dispatch_unified', 'autoselect_crash_unified', 'autosave_resume_missing_pilot_unified',
    'controller_compare_unified', 'death', 'engine_prefs_unified', 'intro_skip_inputs_unified',
    'newmenu_render_paths_unified', 'ogl_gauge_batch_unified', 'ogl_runtime_texture_options_unified',
    'resolution_unified', 'title_music_skip_pref_unified',
    'graphics_black_output', 'graphics_capabilities', 'graphics_confirmation_input', 'graphics_context_loss',
    'graphics_egl_failure', 'graphics_lifecycle', 'graphics_mode_restore', 'graphics_msaa_allocation_failure',
    'graphics_native_interruption', 'graphics_settings_confirmation'
)
$publicDeps = @(Get-SharewareAutomationDeps)
if ($publicDeps.Count -ne 6 -or @($publicDeps.file | Sort-Object -Unique).Count -ne 6) { throw 'Expected six pinned public files' }
foreach ($game in @('d1', 'd2')) {
    $config = Get-SharewareTestGameConfig -RepoRoot $script:REPO_ROOT -Game $game
    foreach ($file in $config.RequiredHashes) {
        if (@($publicDeps | Where-Object { $_.file -ceq $file.File -and $_.sha256 -ceq $file.Sha256 }).Count -ne 1) {
            throw "Public automation dep differs from installer oracle: $($file.File)"
        }
    }
}
$listed = & "$PSScriptRoot/../helpers/run_shareware_tests.ps1" -ListTests | ConvertFrom-Json
if (@($listed).Count -ne $names.Count -or @($listed | Where-Object { ($_.name -replace '^test_', '') -notin $names }).Count) {
    throw 'Public runner did not select exactly the reviewed scenarios'
}
$owned = @($listed | Where-Object owner)
if ($owned.Count -ne 1 -or $owned[0].owner -cne 'test_ogl_runtime_texture_options_unified') {
    throw 'Public runner bypassed the texture integration owner'
}
$ownerText = Get-Content "$PSScriptRoot/test_ogl_runtime_texture_options_unified.ps1" -Raw
if ($ownerText -notmatch '-DataEdition \$DataEdition' -or $ownerText -notmatch '\[string\]\$DataEdition') {
    throw 'Texture integration owner lost explicit retail edition forwarding'
}
$caseCount = 0
foreach ($name in $names) {
    $path = Join-Path $script:ANDROID_ROOT "game_scripts/test_$name.jsonc"
    $info = Get-TestScriptInfo -ScriptPath $path
    if ((Get-TestScriptDataEdition -ScriptPath $path) -cne 'shareware') { throw "$name did not default to public data" }
    $deps = @(Get-ScriptDeps -ScriptPath $path)
    if (($deps | ConvertTo-Json -Compress) -cne ($publicDeps | ConvertTo-Json -Compress)) { throw "$name did not use pinned deps" }
    $retailDeps = @(Get-ScriptDeps -ScriptPath $path -DataEdition retail)
    if ($retailDeps.Count -ne @($info._deps).Count -or ($retailDeps | ConvertTo-Json -Compress) -cne ($info._deps | ConvertTo-Json -Compress)) {
        throw "$name lost original retail dependencies"
    }
    foreach ($game in $info.games) {
        $params = if ($name -eq 'ogl_gauge_batch_unified') { @{ ASSETS = 'native' } } else { @{} }
        $retailPath = Resolve-TestScript -ScriptPath $path -GameId $game -Params $params -DataEdition retail
        $retailText = Get-Content -LiteralPath $retailPath -Raw
        $retail = $retailText | ConvertFrom-Json
        $publicPath = Resolve-TestScript -ScriptPath $path -GameId $game -Params $params
        $publicText = Get-Content -LiteralPath $publicPath -Raw
        $public = $publicText | ConvertFrom-Json
        if ($public.Count -ne $retail.Count) { throw "$name/$game removed steps" }
        if ($publicText -match '\$\{') { throw "$name/$game has unresolved variables" }
        $config = Get-SharewareTestGameConfig -RepoRoot $script:REPO_ROOT -Game $game
        # Only fixture identifiers may change: preserve every action, assertion and timeout
        for ($i = 0; $i -lt $public.Count; $i++) {
            $actual = $public[$i] | ConvertTo-Json -Depth 100 -Compress
            $expected = $retail[$i] | ConvertTo-Json -Depth 100 -Compress
            if ($info.vars -and $info.vars.$game -and $info.vars.$game.MISSION) {
                $expected = $expected.Replace('"' + $info.vars.$game.MISSION + '"', '"' + $config.MissionTitle + '"')
            }
            if ($name -eq 'death') {
                if ($retail[$i].action -eq 'key' -and $retail[$i].key -eq $info.vars.$game.LEVEL_KEY_1) {
                    # Key 1 and key 2 coincide in retail D1; inspect the source-independent sequence below instead
                    $expected = $actual
                }
                if ($retail[$i].action -eq 'key' -and $retail[$i].key -eq $info.vars.$game.LEVEL_KEY_2) { $expected = $actual }
                $expected = $expected.Replace('"current_level_num":"' + $info.vars.$game.LEVEL + '"', '"current_level_num":"1"')
            }
            if ($name -eq 'autosave_resume_missing_pilot_unified' -and $game -eq 'd2') {
                $expected = $expected.Replace('/player/d2/', '/player/d2demo/')
            }
            if ($actual -cne $expected) { throw "$name/$game unexpectedly changed step $i" }
        }
        if ($name -eq 'death') {
            $keys = @($public | Where-Object action -eq 'key' | ForEach-Object key)
            if (($keys -join ',') -notmatch 'backspace,0,1') { throw 'Public death test did not enter level 01' }
        }
        $caseCount++
    }
}
$gauge = Join-Path $script:ANDROID_ROOT 'game_scripts/test_ogl_gauge_batch_unified.jsonc'
$imported = Get-Content (Resolve-TestScript -ScriptPath $gauge -GameId d2 -Params @{ ASSETS = 'd1_in_d2' }) -Raw | ConvertFrom-Json
if (@($imported | Where-Object { $_.action -eq 'select_mission' -and $_.text -ceq 'Descent Demo' }).Count -ne 1) { throw 'Imported D1 cockpit selected the D2 demo' }

foreach ($name in @('test_async_checkpoint_unified', 'test_launch_to_automap')) {
    $path = Join-Path $script:ANDROID_ROOT "game_scripts/$name.jsonc"
    if ((Get-TestScriptDataEdition -ScriptPath $path) -cne 'retail') { throw "$name lost retail coverage" }
    $rejected = $false
    try { Get-ScriptDeps -ScriptPath $path -DataEdition shareware | Out-Null } catch { $rejected = $_ -match 'not been audited' }
    if (-not $rejected) { throw "$name silently accepted incompatible public data" }
}
Write-Host "PASS: $($names.Count) public automation profiles, $caseCount engine variants, retained retail assertions and dependencies"

# Exercise the device isolation contract with an in-memory ADB boundary
$script:deviceFiles = @('descent2.hog', 'DESCENT.PIG', 'loose.rl2')
$script:removedTargets = @()
$script:stopped = $false
function Stop-AppAndWait { $script:stopped = $true }
function Adb {
    param([string[]]$AdbArgs)
    if (-not $script:stopped) { throw 'Tried to clear live engine data' }
    if (($AdbArgs -join ' ') -match 'mkdir -p files/imported/sets/default$') { return }
    if (($AdbArgs -join ' ') -notmatch 'rm -rf files/imported/sets/default$') { throw 'Unexpected device mutation' }
    $script:removedTargets += $AdbArgs[-1]
    $script:deviceFiles = @()
}
function Adb-Timeout {
    param([string[]]$AdbArgs, [int]$Seconds, [switch]$IncludeStandardError)
    if ($AdbArgs -contains 'ls') {
        if ($script:deviceFiles.Count) { return $script:deviceFiles -join "`n" }
        return 'ls: files/imported/sets/default: No such file or directory'
    }
    if ($AdbArgs -contains 'sha256sum') { return "$script:deviceHash  $($AdbArgs[-1])" }
    throw 'Unexpected device read'
}
foreach ($edition in @('shareware', 'retail', 'shareware')) {
    Clear-AutomationDataSet
    if ($script:deviceFiles.Count -ne 0) { throw "$edition retained previous-edition files" }
    $script:deviceFiles = if ($edition -eq 'shareware') { @('descent.hog', 'd2demo.hog') } else { @('descent.hog', 'descent2.hog') }
}
if ($script:removedTargets.Count -ne 3) { throw 'Not every edition transition isolated its data' }
$script:deviceHash = $publicDeps[0].sha256
Assert-AutomationDeviceData -Deps @($publicDeps[0])
$script:deviceHash = '0' * 64
$rejected = $false
try { Assert-AutomationDeviceData -Deps @($publicDeps[0]) } catch { $rejected = $_ -match 'SHA256 verification' }
if (-not $rejected) { throw 'Corrupt device bytes were accepted' }
$script:DEFAULT_SET_DIR = 'files/imported/sets/user-content'
$rejected = $false
try { Clear-AutomationDataSet } catch { $rejected = $_ -match 'refusing to clear' }
if (-not $rejected -or $script:removedTargets.Count -ne 3) { throw 'Isolation was allowed outside the disposable default set' }
Write-Host 'PASS: retail/public transitions, exact staging hashes and bounded cleanup'

# Legacy retail provisioning must not accept public D1 filenames after a demo run
$script:DEFAULT_SET_DIR = 'files/imported/sets/default'
$script:deviceFiles = @('descent.hog', 'descent.pig', 'd2demo.hog')
$originalRoot = $script:REPO_ROOT
try {
    $script:REPO_ROOT = Join-Path ([IO.Path]::GetTempPath()) ('absent-retail-' + [guid]::NewGuid().ToString('N'))
    if (Ensure-GameDataOnDevice -Game d1) { throw 'Legacy provisioning accepted unavailable retail data' }
    if ($script:deviceFiles.Count -ne 0 -or $script:removedTargets.Count -ne 4) {
        throw 'Legacy retail provisioning reused public D1 data'
    }
} finally {
    $script:REPO_ROOT = $originalRoot
}
Write-Host 'PASS: legacy retail provisioning rejects a mixed public/retail set'

# Exercise actual run_test dispatch without starting a device or subprocess
$runnerAst = [Management.Automation.Language.Parser]::ParseFile((Join-Path $script:ANDROID_ROOT 'helpers/run_test.ps1'), [ref]$null, [ref]$null)
$dispatch = $runnerAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Resolve-GameDataDepsWithEmulatorRecovery'
    }, $true)
. ([scriptblock]::Create($dispatch.Extent.Text))
$script:dispatchCalls = @()
function Initialize-SharewareAutomationData { $script:dispatchCalls += 'public-init' }
function Clear-AutomationDataSet { $script:dispatchCalls += 'clear' }
function Assert-AutomationDeviceData { param([array]$Deps) $script:dispatchCalls += 'verify' }
function Resolve-GameDataDeps { param([array]$Deps) $script:dispatchCalls += 'resolve'; return $true }
foreach ($edition in @('shareware', 'retail')) {
    $script:dispatchCalls = @()
    $script:resolvedDataEdition = $edition
    $script:isolatedDataProfile = $true
    if (-not (Resolve-GameDataDepsWithEmulatorRecovery -Deps $publicDeps)) { throw 'Profile dispatch failed' }
    $expected = if ($edition -eq 'shareware') { 'public-init,resolve,verify' } else { 'clear,resolve,verify' }
    if (($script:dispatchCalls -join ',') -cne $expected) { throw "$edition dispatch bypassed isolation or verification" }
}
$script:isolatedDataProfile = $false
$script:dispatchCalls = @()
if (-not (Resolve-GameDataDepsWithEmulatorRecovery -Deps $publicDeps) -or ($script:dispatchCalls -join ',') -cne 'resolve') {
    throw 'Unaudited legacy dispatch unexpectedly changed'
}
Write-Host 'PASS: actual runner dispatch isolates and verifies both audited editions'

# Software navigation changes only an implicit button-search deadline in resolved debug scripts
$previousSoftwareMode = $env:DXX_ANDROID_SOFTWARE_EMULATOR
$previousSerial = $env:ANDROID_SERIAL
$navigationTemp = Join-Path ([IO.Path]::GetTempPath()) ('software-navigation-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $navigationTemp | Out-Null
try {
    $fixture = Join-Path $navigationTemp 'navigation.jsonc'
    $fixtureText = @'
[
    {"action":"tap_button","text":"Game Preferences","post_delay_ms":500},
    {"action":"tap_button","text":"Explicit","timeout_ms":1234},
    {"action":"tap_button","text":"Explicit zero","timeout_ms":0},
    {"action":"tap_button","text":"Explicit null","timeout_ms":null},
    {"action":"wait_for","field":"in_game","value":true,"timeout_ms":1111},
    {"action":"wait_ms","ms":222},
    {"action":"send_button","button":0,"post_delay_ms":333},
    {"action":"assert","expect":{"frame_count":42,"player_dead":false}}
]
'@
    [IO.File]::WriteAllText($fixture, $fixtureText)
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = '1'
    $env:ANDROID_SERIAL = 'emulator-5554'
    $baseline = Get-Content (Resolve-TestScript -ScriptPath $fixture -GameId d1) -Raw
    $originalEstimate = Get-ScriptTimeoutSeconds -ScriptPath $fixture -BufferSeconds 0
    $software = Get-Content (Resolve-TestScript -ScriptPath $fixture -GameId d1 -SoftwareNavigation) -Raw | ConvertFrom-Json
    if ($software[0].timeout_ms -ne 60000 -or $software[1].timeout_ms -ne 1234 -or $software[2].timeout_ms -ne 0) {
        throw 'Implicit navigation budget or explicit action deadline changed incorrectly'
    }
    if (-not $software[3].PSObject.Properties['timeout_ms'] -or $null -ne $software[3].timeout_ms) { throw 'Explicit null timeout changed' }
    $software[0].PSObject.Properties.Remove('timeout_ms')
    if (($software | ConvertTo-Json -Depth 20 -Compress) -cne ($baseline | ConvertFrom-Json | ConvertTo-Json -Depth 20 -Compress)) {
        throw 'Software navigation changed a non-navigation action, assertion or delay'
    }
    $softwareEstimate = Get-ScriptTimeoutSeconds -ScriptPath $fixture -BufferSeconds 0 -SoftwareNavigation
    $resolvedSoftware = Resolve-TestScript -ScriptPath $fixture -GameId d1 -SoftwareNavigation
    if ($softwareEstimate -ne (Get-ScriptTimeoutSeconds -ScriptPath $resolvedSoftware -BufferSeconds 0 -SoftwareNavigation)) {
        throw 'Resolved navigation budgets were double-counted in the outer estimate'
    }
    if ($softwareEstimate -le $originalEstimate) { throw 'Parent timeout estimate omitted implicit navigation work' }
    foreach ($case in @(
            @{ Mode = '1'; Serial = 'physical-device' },
            @{ Mode = ''; Serial = 'emulator-5554' },
            @{ Mode = '1'; Serial = '' }
        )) {
        $env:DXX_ANDROID_SOFTWARE_EMULATOR = $case.Mode
        $env:ANDROID_SERIAL = $case.Serial
        $unchanged = Get-Content (Resolve-TestScript -ScriptPath $fixture -GameId d1 -SoftwareNavigation) -Raw
        if ($unchanged -cne $baseline -or (Get-ScriptTimeoutSeconds -ScriptPath $fixture -BufferSeconds 0 -SoftwareNavigation) -ne $originalEstimate) {
            throw 'Unselected, accelerated or physical target acquired software navigation budgets'
        }
    }
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = '1'
    $env:ANDROID_SERIAL = 'emulator-5554'
    $preferences = Join-Path $script:ANDROID_ROOT 'game_scripts/test_engine_prefs_unified.jsonc'
    $originalHash = (Get-FileHash $preferences).Hash
    foreach ($edition in @('shareware', 'retail')) {
        foreach ($game in @('d1', 'd2')) {
            $profileBaseline = Get-Content (Resolve-TestScript -ScriptPath $preferences -GameId $game -DataEdition $edition) -Raw | ConvertFrom-Json
            $profileSoftware = Get-Content (Resolve-TestScript -ScriptPath $preferences -GameId $game -DataEdition $edition -SoftwareNavigation) -Raw | ConvertFrom-Json
            for ($i = 0; $i -lt $profileBaseline.Count; $i++) {
                if ($profileBaseline[$i].action -eq 'tap_button' -and -not $profileBaseline[$i].PSObject.Properties['timeout_ms']) {
                    if ($profileSoftware[$i].timeout_ms -ne 60000) { throw 'Profile navigation default missing' }
                    $profileSoftware[$i].PSObject.Properties.Remove('timeout_ms')
                }
            }
            if (($profileSoftware | ConvertTo-Json -Depth 100 -Compress) -cne ($profileBaseline | ConvertTo-Json -Depth 100 -Compress)) {
                throw "$edition/$game software profile changed behavior or gameplay timing"
            }
        }
    }
    if ((Get-FileHash $preferences).Hash -cne $originalHash) { throw 'Software profile rewrote an authored fixture' }
    $suiteAst = [Management.Automation.Language.Parser]::ParseFile((Join-Path $script:ANDROID_ROOT 'run_all_tests.ps1'), [ref]$null, [ref]$null)
    $childBudget = $suiteAst.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-RunTestJsoncChildTimeoutSeconds'
        }, $true)
    . ([scriptblock]::Create($childBudget.Extent.Text))
    $TestTimeoutSeconds = 300
    $testTimeouts = @{}
    $manyTaps = Join-Path $navigationTemp 'many-taps.jsonc'
    $steps = @([pscustomobject]@{ action = 'enter_launcher' })
    for ($i = 0; $i -lt 12; $i++) { $steps += [pscustomobject]@{ action = 'tap_button'; text = 'Example' } }
    [IO.File]::WriteAllText($manyTaps, (ConvertTo-Json -InputObject $steps))
    if ((Get-RunTestJsoncChildTimeoutSeconds -ScriptPath $manyTaps -TestName 'many-taps') -ne 735) {
        throw 'Actual master runner omitted implicit navigation budgets from its child deadline'
    }
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = ''
    if ((Get-RunTestJsoncChildTimeoutSeconds -ScriptPath $manyTaps -TestName 'many-taps') -ne 600) {
        throw 'Actual master runner changed the accelerated launcher deadline'
    }
    Write-Host 'PASS: software-only implicit UI navigation, explicit deadlines, profiles and outer estimates'
} finally {
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = $previousSoftwareMode
    $env:ANDROID_SERIAL = $previousSerial
    Remove-Item -LiteralPath $navigationTemp -Recurse -Force
}
