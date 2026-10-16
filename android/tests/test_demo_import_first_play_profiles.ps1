#!/usr/bin/env pwsh
# Offline negative controls for the public first-play owner; never initialize an SDK or device
$ErrorActionPreference = 'Stop'
Set-StrictMode -Off
$helperPath = Join-Path $PSScriptRoot 'demo_import_first_play_helpers.ps1'
$source = Get-Content -LiteralPath $helperPath -Raw
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helperPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "First-play helper parser errors: $parseErrors" }
foreach ($name in @('Assert-FirstPlayEmulator', 'Test-FirstPlayReadiness', 'Assert-FirstPlayFiles', 'Send-FirstPlaySource', 'Tap-FirstPlayButton', 'Import-FirstPlaySources', 'Get-FirstPlayPickerChoicePoint', 'Test-FirstPlayDocumentsUiRoot', 'Test-FirstPlayDocumentsUiCancel', 'Get-FirstPlayBytesHash', 'Get-FirstPlayHogMember', 'New-FirstPlayMission', 'Test-FirstPlayMissionPublication', 'Import-FirstPlayMission')) {
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

# Small synthetic bytes only: the owner must reject malformed archives before any publication
function New-ProfileHog {
    param([string[]]$Names = @('d2leva-1.sl2'))
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('DHF'))
        foreach ($name in $Names) {
            $field = [byte[]]::new(13)
            [Text.Encoding]::ASCII.GetBytes($name).CopyTo($field, 0)
            $writer.Write($field)
            $writer.Write([uint32]3)
            $writer.Write([byte[]]@(1, 2, 3))
        }
        $writer.Flush()
        return ,$stream.ToArray()
    } finally { $writer.Dispose(); $stream.Dispose() }
}
$profileHog = New-ProfileHog
if (((Get-FirstPlayHogMember -Bytes $profileHog -Name 'd2leva-1.sl2') -join ',') -cne '1,2,3') {
    throw 'Bounded HOG reader changed original member bytes'
}
if ((Get-FirstPlayBytesHash ([Text.Encoding]::ASCII.GetBytes('abc'))) -cne 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad') {
    throw 'Public fixture hash helper changed SHA-256'
}
foreach ($invalidHog in @(
        ([byte[]]@(1, 2, 3)),
        ([byte[]]$profileHog[0..10]),
        ([byte[]]$profileHog[0..($profileHog.Length - 2)]),
        (New-ProfileHog -Names @('other.rl2')),
        (New-ProfileHog -Names @('d2leva-1.sl2', 'D2LEVA-1.SL2')),
        ([byte[]]::new(2292567))
    )) {
    Assert-Rejected { Get-FirstPlayHogMember -Bytes $invalidHog -Name 'd2leva-1.sl2' } 'Malformed or ambiguous public level source accepted'
}
# Same-sized incorrect sources must fail their pin, rather than becoming playable claims
$profileSource = [IO.Path]::GetTempFileName()
try {
    [IO.File]::WriteAllBytes($profileSource, [byte[]]::new(2292566))
    Assert-Rejected { New-FirstPlayMission -SourceHog $profileSource -OutputDirectory 'unused-profile-output' } 'Unpinned source generated a mission'
} finally { Remove-Item -LiteralPath $profileSource -Force }

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
function New-MissionSetupState {
    $state = New-SetupState $true $true
    $state | Add-Member -NotePropertyName content_entries -NotePropertyValue @(
        @{ id = 'mod:fpplay.zip'; kind = 'mission_zip'; game = 'd2'; enabled = $true; managed = $true; name = 'Public First Play' }
    )
    return $state
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
    if ($AdbArgs -contains 'logcat') { return "$($script:tapAck)`ntap failure diagnostic marker" }
    if ($AdbArgs -contains 'sha256sum') { return "$($script:hash)  $($AdbArgs[-1])" }
    if ($AdbArgs -contains 'cp' -and $AdbArgs[-1] -notlike 'files/saf_provider_source/*') {
        throw 'Source staging attempted to seed the active set'
    }
}
$Serial = 'emulator-5554'
$script:tapResult = $true
$script:tapTexts = @()
$script:wrongTapAck = $false
function Send-TapButton {
    param($Serial, $Text, $TimeoutSec, $PollMs)
    if ($Serial -cne 'emulator-5554' -or $TimeoutSec -ne 120 -or $PollMs -ne 10000) { throw 'Tap lost its bounded explicit target or safe retry interval' }
    $script:tapTexts += $Text
    $script:tapAck = if ($script:wrongTapAck) { 'tap_button: tapped "Unrelated Button"' } else { 'tap_button: tapped "' + $Text.Trim("'") + '"' }
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
    -not ($script:writtenValues -match 'tap failure diagnostic marker')) {
    throw 'Unacknowledged tap did not fail with preserved button/log diagnostics'
}
$script:tapResult = $true
$script:wrongTapAck = $true
Assert-Rejected { Tap-FirstPlayButton -Text 'Import All' } 'An unrelated tap acknowledgment accepted the import confirmation'
$script:wrongTapAck = $false
# Execute the real shared tap reader, including its logcat argv, without loading SDK helpers
$transportPath = Join-Path $PSScriptRoot '../helpers/test_helpers.ps1'
$transportAst = [Management.Automation.Language.Parser]::ParseFile($transportPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Shared test helper parser errors: $parseErrors" }
$tapTransportDefinition = $transportAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Send-TapButton'
    }, $true)
if (-not $tapTransportDefinition) { throw 'Missing shared tap reader' }
foreach ($ackScenario in @('noisy_success', 'wrong_ack', 'missing_ack')) {
    & {
        . ([scriptblock]::Create($tapTransportDefinition.Extent.Text))
        $logBuffer = [Collections.Generic.List[object]]::new()
        $tapTransportState = @{ Requests = 0; Clears = 0 }
        function Send-MpCommand {
            param($Serial, $Command, $Extras)
            if ($Serial -cne 'emulator-5554' -or $Command -cne 'tap_button' -or $Extras[-1] -cne "'Import All'") {
                throw 'Shared tap reader changed its explicit request'
            }
            $tapTransportState.Requests++
            if ($ackScenario -ne 'missing_ack') {
                $label = if ($ackScenario -eq 'wrong_ack') { 'Unrelated Button' } else { 'Import All' }
                $logBuffer.Add([pscustomobject]@{ Tag = 'DXX-MP'; Text = 'tap_button: tapped "' + $label + '"' })
            }
            foreach ($noiseIndex in 1..30) {
                $logBuffer.Add([pscustomobject]@{ Tag = 'Other'; Text = "Unrelated device log $noiseIndex" })
            }
            if (@($logBuffer | Select-Object -Last 20 | Where-Object Tag -CEQ 'DXX-MP').Count) {
                throw 'Noisy fixture did not displace the acknowledgment from the global tail'
            }
        }
        function Adb-Dev-Timeout {
            param($Serial, $AdbArgs, $Seconds)
            if ($Serial -cne 'emulator-5554') { throw 'Shared tap read lost its target' }
            if ($AdbArgs -contains '-c') {
                $tapTransportState.Clears++
                $logBuffer.Clear()
                return
            }
            if ($AdbArgs -ccontains '-t' -or $AdbArgs -ccontains '-T') {
                throw 'Global log tail can discard the successful tap before tag filtering'
            }
            if ($AdbArgs -cnotcontains '-d' -or $AdbArgs -cnotcontains '-s' -or $AdbArgs -cnotcontains 'DXX-MP:*') {
                throw 'Shared tap reader lost its bounded tag-filtered dump'
            }
            return @($logBuffer | Where-Object Tag -CEQ 'DXX-MP' | ForEach-Object Text) -join "`n"
        }
        function Adb {
            param($AdbArgs, $Seconds)
            return @($logBuffer | Where-Object Tag -CEQ 'DXX-MP' | ForEach-Object Text) -join "`n"
        }
        function Start-Sleep { param($Milliseconds) [Threading.Thread]::Sleep(10) }
        function Write-Status { param($Msg, $Color) }
        if ($ackScenario -eq 'missing_ack') {
            if (Send-TapButton -Serial emulator-5554 -Text "'Import All'" -TimeoutSec 1 -PollMs 1) {
                throw 'Missing acknowledgment passed the real shared reader'
            }
        } else {
            $ackFailure = ''
            try { Tap-FirstPlayButton -Text 'Import All' } catch { $ackFailure = $_.Exception.Message }
            $wantedAckFailure = if ($ackScenario -eq 'wrong_ack') { 'Could not tap first-play button: Import All' } else { '' }
            if ($ackFailure -cne $wantedAckFailure -or $tapTransportState.Requests -ne 1 -or $tapTransportState.Clears -ne 1) {
                throw "Acknowledgment scenario $ackScenario lost the first completed click: $ackFailure"
            }
        }
    }
}
foreach ($importScenario in @('archive', 'loose', 'missing_ack', 'missing_copy', 'wrong_hash', 'missing_file', 'wrong_inventory')) {
    & {
        $fixtureExpected = @(
            @{ File = 'descent.hog'; Sha256 = ('a' * 64) },
            @{ File = 'descent.pig'; Sha256 = ('b' * 64) }
        )
        $fixtureKind = if ($importScenario -eq 'archive') { 'archive' } else { 'loose' }
        $fixtureNames = if ($fixtureKind -eq 'archive') { @('desc14sw.exe') } else { @('descent.hog', 'descent.pig') }
        $importEvents = [Collections.Generic.List[string]]::new()
        $importPoll = @{ Count = 0 }
        function Tap-FirstPlayButton {
            param($Text)
            $wanted = if ($fixtureKind -eq 'archive') { 'Import to Current Set' } else { 'Import All' }
            if ($Text -cne $wanted) { throw 'Wrong production confirmation requested' }
            $importEvents.Add('tap')
            if ($importScenario -eq 'missing_ack') { throw 'Mock missing acknowledgment' }
        }
        function Wait-SetupCondition { throw 'Import confirmation must use the scroll-capable tap, not a passive viewport gate' }
        function Adb {
            param($AdbArgs, $Seconds)
            if ($AdbArgs -contains 'import_picked_uris') {
                $wantedUris = @($fixtureNames | ForEach-Object { "content://test.package.saf-test/seekable/$_" }) -join ','
                if ($AdbArgs -cnotcontains '--esa' -or $AdbArgs[-1] -cne $wantedUris) { throw 'Post-picker callback changed' }
                $importEvents.Add('callback')
            }
            if ($AdbArgs -contains 'logcat') {
                if ($importScenario -eq 'missing_copy') { return 'No fresh copy completion' }
                return "Imported descent.hog -> descent.hog (23 bytes)`nImported descent.pig -> descent.pig (51 bytes)"
            }
            if ($AdbArgs -contains 'sha256sum') {
                if ($importEvents -cnotcontains 'tap') { throw 'Publication checked before actual confirmation' }
                $entry = @($fixtureExpected | Where-Object { $AdbArgs[-1] -ceq "files/imported/sets/default/$($_.File)" })
                if ($entry.Count -ne 1) { throw 'Publication checked an unpinned path' }
                if ($importPoll.Count -eq 1 -or ($importScenario -eq 'missing_file' -and $entry[0].File -ceq 'descent.pig')) {
                    return 'No such file or directory'
                }
                if ($importScenario -eq 'wrong_hash' -and $entry[0].File -ceq 'descent.pig') { return "bad-hash  $($AdbArgs[-1])" }
                return "$($entry[0].Sha256)  $($AdbArgs[-1])"
            }
        }
        function Wait-ForCondition {
            param($Description, $TimeoutSec, $PollMs, $Condition)
            if ($TimeoutSec -ne 90 -or $PollMs -ne 500) { throw 'Publication wait lost its bound' }
            foreach ($attempt in 1..2) {
                $importPoll.Count++
                if (& $Condition) { return $true }
            }
            return $false
        }
        function Assert-FirstPlayFiles {
            param($Expected, $Label)
            if (($Expected.File -join ',') -cne 'descent.hog,descent.pig') { throw 'Full pinned inventory was not verified' }
            $importEvents.Add('inventory')
            if ($importScenario -eq 'wrong_inventory') { throw 'Mock unexpected published file' }
        }
        function Get-FirstPlaySetup { return (New-SetupState) }
        $importFailure = ''
        try { Import-FirstPlaySources -Names $fixtureNames -Kind $fixtureKind -Label 'profile' -Expected $fixtureExpected } catch {
            $importFailure = $_.Exception.Message
        }
        $wantedFailure = switch ($importScenario) {
            'missing_ack' { 'Mock missing acknowledgment' }
            'wrong_inventory' { 'Mock unexpected published file' }
            { $_ -in @('missing_copy', 'wrong_hash', 'missing_file') } { 'profile lacks fresh production publication with the expected hashes' }
            default { '' }
        }
        $wantedEvents = if ($importScenario -in @('archive', 'loose', 'wrong_inventory')) { 'callback,tap,inventory' } else { 'callback,tap' }
        if ($importFailure -cne $wantedFailure -or ($importEvents -join ',') -cne $wantedEvents) {
            throw "Import scenario $importScenario bypassed confirmation/publication or failed unexpectedly: $importFailure"
        }
        if ($importScenario -ne 'missing_ack' -and $importPoll.Count -ne 2) { throw 'Import publication did not wait for fresh copied bytes' }
    }
}
foreach ($missionScenario in @('valid', 'missing_log', 'wrong_hash', 'missing_owner', 'disabled', 'string_enabled', 'wrong_kind', 'wrong_game', 'extra_mod', 'wrong_name', 'unmanaged', 'string_managed')) {
    & {
        $missionEvents = [Collections.Generic.List[string]]::new()
        $missionPoll = @{ Count = 0 }
        $fixture = @{ Path = 'generated-fixture'; File = 'fpplay.zip'; Sha256 = ('c' * 64) }
        function Send-FirstPlaySource {
            param($Path, $Name, $Sha256)
            if ($Path -cne $fixture.Path -or $Name -cne 'fpplay.zip' -or $Sha256 -cne $fixture.Sha256) {
                throw 'Mission staging lost its fixed identity'
            }
            $missionEvents.Add('stage')
        }
        function Tap-FirstPlayButton { throw 'Mission ZIP import has no base-installer confirmation' }
        function Adb {
            param($AdbArgs, $Seconds)
            if ($AdbArgs -contains 'logcat') {
                if ($AdbArgs -contains '-c') { $missionEvents.Add('clear'); return }
                if ($missionPoll.Count -eq 1 -or $missionScenario -eq 'missing_log') { return 'No fresh import' }
                return 'Mission ZIP import ok: fpplay.zip (123 bytes)'
            }
            if ($AdbArgs -contains 'import_picked_uris') {
                if (($missionEvents -join ',') -cne 'stage,clear' -or $AdbArgs -cnotcontains '--esa' -or
                    $AdbArgs[-1] -cne 'content://test.package.saf-test/seekable/fpplay.zip') { throw 'Mission callback lost source staging, fresh log, or URI array' }
                $missionEvents.Add('callback')
                return
            }
            if ($AdbArgs -contains 'sha256sum') {
                if ($AdbArgs[-1] -cne 'files/imported/sets/default/.content/mods/fpplay.zip') { throw 'Mission publication checked an unrelated path' }
                if ($missionScenario -eq 'wrong_hash') { return 'bad-hash' }
                return "$($fixture.Sha256)  $($AdbArgs[-1])"
            }
            throw 'Unexpected mission import transport'
        }
        function Get-FirstPlaySetup {
            $state = New-MissionSetupState
            switch ($missionScenario) {
                'missing_owner' { $state.content_entries = @() }
                'disabled' { $state.content_entries[0].enabled = $false }
                'string_enabled' { $state.content_entries[0].enabled = 'true' }
                'wrong_kind' { $state.content_entries[0].kind = 'dxa' }
                'wrong_game' { $state.content_entries[0].game = 'd1' }
                'extra_mod' { $state.content_entries += @{ id = 'mod:other.zip' } }
                'wrong_name' { $state.content_entries[0].name = 'Other Mission' }
                'unmanaged' { $state.content_entries[0].managed = $false }
                'string_managed' { $state.content_entries[0].managed = 'true' }
            }
            return $state
        }
        function Wait-ForCondition {
            param($Description, $TimeoutSec, $PollMs, $Condition)
            if ($TimeoutSec -ne 90 -or $PollMs -ne 500) { throw 'Mission import wait lost its bound' }
            foreach ($attempt in 1..2) {
                $missionPoll.Count++
                if (& $Condition) { return $true }
            }
            return $false
        }
        $missionFailure = ''
        try { Import-FirstPlayMission -Fixture $fixture } catch { $missionFailure = $_.Exception.Message }
        $wantedFailure = if ($missionScenario -eq 'valid') { '' } else { 'Public mission lacks fresh production import, exact bytes, or enabled ownership' }
        if ($missionFailure -cne $wantedFailure -or $missionPoll.Count -ne 2 -or ($missionEvents -join ',') -cne 'stage,clear,callback') {
            throw "Mission publication scenario $missionScenario lost exact identity or fresh callback proof: $missionFailure"
        }
    }
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
$documentsXml = '<hierarchy><node package="com.google.android.documentsui"/></hierarchy>'
$documentsActivity = 'topResumedActivity=ActivityRecord{123 u0 com.google.android.documentsui/com.android.documentsui.picker.PickActivity t9}'
foreach ($pickerPackage in @('com.android.documentsui', 'com.google.android.documentsui')) {
    if (-not (Test-FirstPlayDocumentsUiRoot -XmlText $documentsXml.Replace('com.google.android.documentsui', $pickerPackage) `
                -ActivityText $documentsActivity.Replace('com.google.android.documentsui', $pickerPackage))) {
        throw 'Exact system picker root rejected'
    }
}
foreach ($invalidXml in @('', '<broken', '<hierarchy/>',
        $documentsXml.Replace('com.google.android.documentsui', 'com.google.android.documentsui.fake'),
        '<hierarchy><node text="com.google.android.documentsui"/></hierarchy>',
        '<hierarchy><node package="test.package"><node package="com.google.android.documentsui"/></node></hierarchy>')) {
    if (& { Set-StrictMode -Version Latest; Test-FirstPlayDocumentsUiRoot -XmlText $invalidXml -ActivityText $documentsActivity }) { throw 'Non-system picker root accepted' }
}
foreach ($invalidActivity in @('',
        $documentsActivity.Replace('topResumedActivity', 'Hist #0'),
        $documentsActivity.Replace('PickActivity', 'OtherActivity'),
        $documentsActivity.Replace('com.google.android.documentsui/', 'other.package/'),
        ($documentsActivity + "`n" + 'mResumedActivity=ActivityRecord{234 u0 test.package/.SetupActivity t9}'))) {
    if (Test-FirstPlayDocumentsUiRoot -XmlText $documentsXml -ActivityText $invalidActivity) { throw 'Picker XML passed without exact fresh foreground activity' }
}
# PowerShell names are case-insensitive; fixture loops must not overwrite the app PACKAGE
if ($script:PACKAGE -cne 'test.package') { throw 'Picker fixture controls changed the target app package' }
foreach ($scenario in @('chooser', 'already_open', 'missing_second_observation')) {
    & {
        $observations = [Collections.Generic.Queue[string]]::new()
        $observations.Enqueue($(if ($scenario -eq 'chooser') { $chooserXml } else { $documentsXml }))
        $appXml = '<hierarchy><node package="test.package"/></hierarchy>'
        $observations.Enqueue($(if ($scenario -eq 'missing_second_observation') { $appXml } else { $documentsXml }))
        $observations.Enqueue($appXml)
        $events = [Collections.Generic.List[string]]::new()
        $observed = @{ Xml = '' }
        function Send-MpCommand {
            param($Serial, $Command, $Extras)
            if ($Command -cne 'tap_button' -or $Extras[-1] -cne "'Select Game Files or Archive to Import'") { throw 'Initial chooser request changed' }
            $events.Add('request')
        }
        function Adb {
            param($AdbArgs, $Seconds)
            if ($AdbArgs -contains 'cat') { $observed.Xml = $observations.Dequeue(); return $observed.Xml }
            if ($AdbArgs -contains 'dumpsys') {
                if ($observed.Xml -ceq $documentsXml) { return $documentsActivity }
                return 'topResumedActivity=ActivityRecord{234 u0 test.package/.SetupActivity t9}'
            }
            if ($AdbArgs -contains 'input') { $events.Add($AdbArgs[2]) }
        }
        function Wait-ForCondition { param($Description, $TimeoutSec, $PollMs, $Condition) return [bool](& $Condition) }
        function Wait-FirstPlaySetup {
            param($Label, $Predicate)
            if (-not (& $Predicate (New-SetupState $true $true))) { throw 'Cancellation must retain both imported games ready' }
            $events.Add('readiness')
        }
        function Assert-FirstPlayFiles {
            param($Expected, $Label)
            if (($Expected -join ',') -cne 'd1,d2') { throw 'Cancellation did not recheck final fixture hashes' }
            $events.Add('files')
        }
        $failed = $false
        try { Test-FirstPlayDocumentsUiCancel -Expected @('d1', 'd2') | Out-Null } catch {
            if ($_.Exception.Message -cne 'Import chooser did not open the system DocumentsUI') { throw }
            $failed = $true
        }
        $expectedEvents = switch ($scenario) {
            'chooser' { 'request,tap,keyevent,readiness,files' }
            'already_open' { 'request,keyevent,readiness,files' }
            'missing_second_observation' { 'request' }
        }
        if (($events -join ',') -cne $expectedEvents -or $failed -ne ($scenario -eq 'missing_second_observation')) {
            throw "Picker scenario $scenario skipped fresh proof, repeated a click, or lost cancel/readiness checks"
        }
    }
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
foreach ($phase in @('empty', 'd1', 'd1_in_d2', 'd2', 'mission')) {
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
        $liveAssertions = @($steps | Where-Object { $_.action -eq 'assert' -and $_.expect.in_game -eq $true -and $_.expect.current_level_num -eq 1 })
        $expectedLiveCount = if ($phase -eq 'mission') { 3 } else { 1 }
        if (@($steps | Where-Object { $_.action -eq 'select' -and $_.text -ceq 'New game' }).Count -ne 1 -or
            $liveAssertions.Count -ne $expectedLiveCount) {
            throw "First-play $phase lost ordinary new-game navigation or live level-one evidence"
        }
        if ($phase -eq 'd2' -and @($steps | Where-Object {
                    $_.action -eq 'assert' -and $_.expect.'asset_trace.runtime_engine' -ceq 'd2' -and
                    $_.expect.'asset_trace.selected_base_game' -ceq 'd2' -and $_.expect.'asset_trace.mode' -ceq 'd2' -and
                    $_.expect.'asset_trace.current_level_palette' -ceq 'GROUPA.256' -and
                    $_.expect.'asset_trace.current_pigfile' -ceq 'groupa.pig'
                }).Count -ne 1) {
            throw 'Native D2 trace must assert the requested palette/pig names; pinned import hashes prove the demo asset'
        }
        if ($phase -eq 'mission') {
            if (@($steps | Where-Object { $_.action -eq 'select_mission' -or $_.action -like 'debug*' -or $_.action -in @('set_debug', 'set_field') }).Count) {
                throw 'Public mission must not use mission-picker shortcuts or debug state mutation'
            }
            $selections = @($steps | Where-Object action -EQ 'select' | ForEach-Object text) -join '|'
            if ($selections -cne 'New game|Public First Play|Ok|Trainee|Save Game...|Load Game|level 1|Yes' -or
                @($steps | Where-Object { $_.action -eq 'assert' -and $_.expect.'menu.items'.contains -ceq 'Public First Play' }).Count -ne 1) {
                throw 'Public mission lost its observed normal mission picker or save/load menus'
            }
            $ammo = @($steps | Where-Object { $_.action -eq 'assert' -and $null -ne $_.expect.'player.secondary_ammo[0]' } | ForEach-Object { $_.expect.'player.secondary_ammo[0]' }) -join ','
            if ($ammo -cne '7,6,5,6,6' -or @($steps | Where-Object { $_.action -eq 'send_button' -and $_.button -eq 101 }).Count -ne 2) {
                throw 'Public mission lost its nondefault saved-ammo witness'
            }
            foreach ($live in $liveAssertions) {
                if ($live.expect.mission_asset_owner -cne 'mod/fpplay.zip' -or $live.expect.'player.callsign' -cne 'player' -or
                    $live.expect.difficulty -ne 0 -or $live.expect.player_dead -ne $false -or $live.expect.time_paused -ne $false) {
                    throw 'Public mission lost owned live identity or preserved player state'
                }
            }
            if (@($steps | Where-Object { $_.action -eq 'tap_button' -and $_.text -ceq 'Load Last Save' -and $_.exact -eq $true -and $_.launches_game -eq $true }).Count -ne 1 -or
                @($steps | Where-Object { $_.action -eq 'assert' -and $_.expect.'resume_candidate.mission_name' -ceq 'fpplay' -and $_.expect.'resume_candidate.level_num' -eq 1 -and $_.expect.'resume_candidate.save_kind' -ceq 'auto_exit' }).Count -ne 1 -or
                @($steps | Where-Object { $_.action -eq 'delete_mod' -and $_.file -ceq 'fpplay.zip' }).Count -ne 1 -or
                @($steps | Where-Object action -EQ 'clear_mods').Count -ne 1) {
                throw 'Public mission lost real launcher resume, saved mission identity, or cleanup'
            }
        }
    }
}
Write-Host 'PASS: public first-play target, readiness, hash, mission packaging/publication, normal menu save/resume and correlated-evidence controls'
