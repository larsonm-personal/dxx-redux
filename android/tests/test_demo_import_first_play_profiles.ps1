#!/usr/bin/env pwsh
# Offline negative controls for the public first-play owner; never initialize an SDK or device
$ErrorActionPreference = 'Stop'
Set-StrictMode -Off
$helperPath = Join-Path $PSScriptRoot 'demo_import_first_play_helpers.ps1'
$source = Get-Content -LiteralPath $helperPath -Raw
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helperPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "First-play helper parser errors: $parseErrors" }
foreach ($name in @('Assert-FirstPlayEmulator', 'Test-FirstPlayReadiness', 'Assert-FirstPlayFiles', 'Send-FirstPlaySource', 'Tap-FirstPlayButton', 'Get-FirstPlayPickerChoicePoint')) {
    $function = $ast.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $function) { throw "Missing first-play control: $name" }
    . ([scriptblock]::Create($function.Extent.Text))
}
function Assert-Rejected {
    param([scriptblock]$Action, [string]$Message)
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw $Message }
}

$script:online = 'device'
$script:qemu = '1'
$script:deviceCalls = @()
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    $script:deviceCalls += $AdbArgs -join ' '
    if (($AdbArgs -join ' ') -ceq 'get-state') { return $script:online }
    if (($AdbArgs -join ' ') -ceq 'shell getprop ro.kernel.qemu') { return $script:qemu }
    throw 'Target admission attempted an unexpected device operation'
}
Assert-FirstPlayEmulator -Serial emulator-5554
if ($script:deviceCalls.Count -ne 2) { throw 'Emulator admission did not verify transport and QEMU' }
foreach ($serial in @('', 'phone', 'emulator', 'emulator-5554-other', '192.168.1.9:5555')) {
    $script:deviceCalls = @()
    Assert-Rejected { Assert-FirstPlayEmulator -Serial $serial } 'Physical or ambiguous target was accepted'
    if ($script:deviceCalls.Count) { throw 'Invalid serial reached device commands' }
}
foreach ($state in @('offline', '', 'unauthorized')) {
    $script:online = $state
    Assert-Rejected { Assert-FirstPlayEmulator -Serial emulator-5554 } 'Offline emulator was accepted'
}
$script:online = 'device'
foreach ($qemu in @('0', '', 'true')) {
    $script:qemu = $qemu
    Assert-Rejected { Assert-FirstPlayEmulator -Serial emulator-5554 } 'Unverified emulator was accepted'
}

function New-SetupState {
    param([bool]$D1 = $false, [bool]$D2 = $false)
    return [pscustomobject]@{
        screen = 'setup'; active_set = 'default'; d1 = @{ ready = $D1 }; d2 = @{ ready = $D2 }
        can_launch = ($D1 -or $D2); set_files_recursive = @()
    }
}
foreach ($pair in @(@($false, $false), @($true, $false), @($true, $true))) {
    if (-not (Test-FirstPlayReadiness (New-SetupState $pair[0] $pair[1]) $pair[0] $pair[1])) { throw 'Valid readiness rejected' }
}
foreach ($missing in @($null, @{}, @{ screen = 'setup'; active_set = 'default' }, @{ can_launch = $false })) {
    if (Test-FirstPlayReadiness $missing $false $false) { throw 'Missing readiness passed as empty content' }
}
foreach ($mutate in @(
        { param($s) $s.screen = 'game' }, { param($s) $s.active_set = 'retail' },
        { param($s) $s.can_launch = $true }, { param($s) $s.d1.ready = $true },
        { param($s) $s.d2.ready = $true }, { param($s) $s.d1.ready = 'false' },
        { param($s) $s.d2.Remove('ready') }
    )) {
    $state = New-SetupState
    & $mutate $state
    if (Test-FirstPlayReadiness $state $false $false) { throw 'Incorrect or incomplete readiness accepted' }
}

$script:PACKAGE = 'test.package'
$script:DEFAULT_SET_DIR = 'files/imported/sets/default'
$firstPlayOutput = 'unused-profile-output'
$expected = @(@{ File = 'descent.hog'; Sha256 = ('a' * 64) })
$script:state = New-SetupState
$script:state.set_files_recursive = @('assets.json', 'descent.hog')
$script:hash = $expected[0].Sha256
$script:outputWrites = 0
$script:writtenValues = @()
function Wait-FirstPlaySetup { param($Label, $Predicate) return $script:state }
function Set-Content { param([Parameter(Position = 0)]$Path, [Parameter(ValueFromPipeline)]$Value, $Encoding) process { $script:outputWrites++; $script:writtenValues += [string]$Value } }
function Adb {
    param($AdbArgs, $Seconds)
    if ($AdbArgs -contains 'logcat') { return 'tap failure diagnostic marker' }
    if ($AdbArgs -contains 'sha256sum') { return "$($script:hash)  $($AdbArgs[-1])" }
    if ($AdbArgs -contains 'cp' -and $AdbArgs[-1] -notlike 'files/saf_provider_source/*') {
        throw 'Source staging attempted to seed the active set'
    }
}
$Serial = 'emulator-5554'
$script:tapResult = $true
$script:tapTexts = @()
function Send-TapButton {
    param($Serial, $Text, $TimeoutSec, $PollMs)
    if ($Serial -cne 'emulator-5554' -or $TimeoutSec -ne 30 -or $PollMs -ne 500) { throw 'Tap lost its bounded explicit target' }
    $script:tapTexts += $Text
    return $script:tapResult
}
foreach ($label in @('Select Game Files or Archive to Import', 'Import to Current Set', 'Import All')) {
    Tap-FirstPlayButton -Text $label
    if ($script:tapTexts[-1] -cne "'$label'") { throw 'Multi-word button lost its remote-shell quoting' }
}
$script:tapTexts = @()
Assert-Rejected { Tap-FirstPlayButton -Text "unsafe'button" } 'Arbitrary shell text reached the button tap helper'
if ($script:tapTexts.Count) { throw 'Unreviewed button text reached device transport' }
$script:tapResult = $false
$failure = ''
try { Tap-FirstPlayButton -Text 'Import All' } catch { $failure = $_.Exception.Message }
if ($failure -cne 'Could not tap first-play button: Import All' -or
    'requested_button=Import All' -cnotin $script:writtenValues -or
    'tap failure diagnostic marker' -cnotin $script:writtenValues) {
    throw 'Unacknowledged tap did not fail with preserved button/log diagnostics'
}
$chooserXml = '<hierarchy><node package="test.package" text="Import Files"/><node package="test.package" text="Pick One or More Files" enabled="true" bounds="[20,40][120,100]"/></hierarchy>'
$point = Get-FirstPlayPickerChoicePoint -XmlText $chooserXml
if ($null -eq $point -or $point.X -ne 70 -or $point.Y -ne 70) { throw 'Exact chooser node did not produce its observed center' }
foreach ($invalidXml in @(
        '', '<broken',
        $chooserXml.Replace('Import Files', 'Other Dialog'),
        $chooserXml.Replace('test.package', 'other.package'),
        $chooserXml.Replace('Pick One or More Files', 'Pick One or More Files Copy'),
        $chooserXml.Replace('enabled="true"', 'enabled="false"'),
        $chooserXml.Replace(' bounds="[20,40][120,100]"', ''),
        $chooserXml.Replace('[20,40][120,100]', '[20,40][20,100]'),
        $chooserXml.Replace('</hierarchy>', '<node package="test.package" text="Pick One or More Files" enabled="true" bounds="[20,40][120,100]"/></hierarchy>')
    )) {
    $invalidPoint = & { Set-StrictMode -Version Latest; Get-FirstPlayPickerChoicePoint -XmlText $invalidXml }
    if ($null -ne $invalidPoint) { throw 'Unobserved, disabled, ambiguous, or foreign chooser node was accepted' }
}
Assert-FirstPlayFiles -Expected $expected -Label valid
Send-FirstPlaySource -Path fixture -Name descent.hog -Sha256 $expected[0].Sha256
$script:hash = 'bad-hash'
Assert-Rejected { Assert-FirstPlayFiles -Expected $expected -Label invalid } 'Incorrect published hashes accepted'
Assert-Rejected { Send-FirstPlaySource -Path fixture -Name descent.hog -Sha256 $expected[0].Sha256 } 'Incorrect provider source hashes accepted'
$script:hash = $expected[0].Sha256
$script:state.set_files_recursive += 'descent2.hog'
Assert-Rejected { Assert-FirstPlayFiles -Expected $expected -Label contaminated } 'Unexpected retail file accepted'
$script:state.set_files_recursive = @('assets.json')
Assert-Rejected { Assert-FirstPlayFiles -Expected $expected -Label unpublished } 'Missing published file accepted'
$script:state = [pscustomobject]@{ active_set = 'default' }
Assert-Rejected { Assert-FirstPlayFiles -Expected @() -Label missing } 'Missing file inventory accepted'

$ownerPath = Join-Path $PSScriptRoot 'test_demo_import.ps1'
$owner = [Management.Automation.Language.Parser]::ParseFile($ownerPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Demo import owner parser errors: $parseErrors" }
$public = $owner.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'PublicFirstPlay' }
if (-not $public -or $public.DefaultValue) { throw 'Full corpus must remain default; public journey requires an explicit switch' }
$clearIndex = $source.IndexOf("@('shell', 'pm', 'clear'")
$guardIndex = $source.LastIndexOf('Assert-FirstPlayEmulator -Serial $Serial', $clearIndex)
$healthIndex = $source.IndexOf('Ensure-EmulatorHealthy | Out-Null')
if ($clearIndex -lt 0 -or $guardIndex -lt $healthIndex) { throw 'Destructive clear lost its post-health disposable-target guard' }
foreach ($forbidden in @('import_files', 'Ensure-GameDataOnDevice', 'Initialize-SharewareAutomationData', "'grant'", 'appops')) {
    if ($source.Contains($forbidden)) { throw "First-play owner contains bypass or permission change: $forbidden" }
}
if ($source -notmatch "'import_picked_uris', '--esa', 'uris'" -or
    $source -notmatch 'Get-SharewareTestGameConfig' -or
    $source -notmatch 'Assert-DxxFileSha256' -or
    $source -notmatch 'Test-AutomationResultComplete' -or
    $source -notmatch '\$firstPlayRunIds\.Add' -or
    $source -notmatch 'runnerLog\.Contains\(\$expectedPushName\)' -or
    $source -match '-DataEdition|-Params\s') { throw 'First-play provenance, fresh evidence, or no-seeding contract changed' }

. "$PSScriptRoot/../helpers/jsonc.ps1"
$catalogPath = Join-Path $PSScriptRoot 'test_validate_automation_catalog.ps1'
$catalogAst = [Management.Automation.Language.Parser]::ParseFile($catalogPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Catalog parser errors: $parseErrors" }
$exception = $catalogAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-FirstPlayLauncherButtonException'
    }, $true)
if (-not $exception) { throw 'Missing reviewed first-play catalog exception' }
. ([scriptblock]::Create($exception.Extent.Text))
$gameScriptsDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../game_scripts'))
$allowedInfo = @{ _standalone = $false; _owner = 'test_demo_import' }
foreach ($unrelated in @(
        (Join-Path $gameScriptsDir 'test_launch_to_automap.jsonc'),
        (Join-Path $gameScriptsDir 'test_public_first_play_other.jsonc'),
        (Join-Path $gameScriptsDir 'test_public_first_play_d1_copy.jsonc'),
        (Join-Path $PSScriptRoot 'test_public_first_play_d1.jsonc')
    )) {
    if (Test-FirstPlayLauncherButtonException -ScriptPath $unrelated -Info $allowedInfo) {
        throw 'Catalog accepted an unrelated or external launcher-button script'
    }
}
foreach ($invalidInfo in @(
        $null, @{}, @{ _standalone = $false }, @{ _owner = 'test_demo_import' },
        @{ _standalone = $false; _owner = 'test_other' },
        @{ _standalone = $true; _owner = 'test_demo_import' },
        @{ _standalone = 'false'; _owner = 'test_demo_import' }
    )) {
    if (Test-FirstPlayLauncherButtonException -ScriptPath (Join-Path $gameScriptsDir 'test_public_first_play_d1.jsonc') -Info $invalidInfo) {
        throw 'Catalog accepted missing, standalone, or foreign first-play ownership'
    }
}
foreach ($phase in @('empty', 'd1', 'd1_in_d2', 'd2')) {
    $path = Join-Path $PSScriptRoot "../game_scripts/test_public_first_play_$phase.jsonc"
    $steps = Read-JsoncFile -Path $path
    $info = $steps[0]._info
    if (-not (Test-FirstPlayLauncherButtonException -ScriptPath $path -Info $info) -or
        -not $source.Contains("'test_public_first_play_$phase.jsonc'")) {
        throw "First-play $phase lost its exact catalog admission or literal owner reference"
    }
    if ($info._standalone -ne $false -or $info._owner -cne 'test_demo_import' -or $info._deps -or $info._shareware -or $info.params) {
        throw "First-play $phase lost its parameter-free, unseeded owner contract"
    }
    if (@($steps | Where-Object action -in @('enter_game', 'start_level', 'reset_state')).Count) { throw "First-play $phase bypasses normal launch" }
    if ($phase -ne 'empty') {
        $button = if ($phase -eq 'd1') { 'Launch Descent 1' } else { 'Launch Descent 2' }
        if (@($steps | Where-Object { $_.action -eq 'tap_button' -and $_.text -ceq $button -and $_.launches_game -eq $true }).Count -ne 1) {
            throw "First-play $phase does not launch through the normal button"
        }
        if (@($steps | Where-Object { $_.action -eq 'select' -and $_.text -ceq 'New game' }).Count -ne 1 -or
            @($steps | Where-Object { $_.action -eq 'assert' -and $_.expect.in_game -eq $true -and $_.expect.current_level_num -eq 1 }).Count -ne 1) {
            throw "First-play $phase lost ordinary new-game navigation or live level-one evidence"
        }
    }
}
Write-Host 'PASS: public first-play target, readiness, hash, menu-path and correlated-evidence controls'
