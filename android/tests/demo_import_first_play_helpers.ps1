# TEST-SUPPORT: owner=test_demo_import
# Bounded public first-play journey, separate from the full installer corpus

function Assert-FirstPlayEmulator {
    param([string]$Serial)
    if ($Serial -notmatch '^emulator-\d+$') { throw 'Public first-play requires a disposable emulator; physical targets are not allowed' }
    $online = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('get-state') -Seconds 5
    $qemu = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'getprop', 'ro.kernel.qemu') -Seconds 5
    if (([string]$online).Trim() -cne 'device' -or ([string]$qemu).Trim() -cne '1') {
        throw "Refusing to clear unverified disposable emulator $Serial"
    }
}

function Get-FirstPlaySetup {
    # Delete the prior snapshot before the broadcast so no stale readiness can pass
    $removed = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sh', '-c',
        "'rm -f files/setup_introspect.json && echo FIRST_PLAY_FRESH'")
    if ($removed -cne 'FIRST_PLAY_FRESH') { throw 'Could not invalidate the setup snapshot' }
    return Get-SetupIntrospection
}

function Wait-FirstPlaySetup {
    param([scriptblock]$Predicate, [string]$Label, [int]$TimeoutSeconds = 60)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $state = Get-FirstPlaySetup
        if ($state -and (& $Predicate $state)) {
            $state | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $firstPlayOutput "$Label-setup.json") -Encoding utf8
            return $state
        }
        Start-Sleep -Milliseconds 400
    }
    throw "Fresh launcher state did not satisfy $Label"
}

function Test-FirstPlayReadiness {
    param($State, [bool]$D1Ready, [bool]$D2Ready)
    return ($State -and $State.screen -ceq 'setup' -and $State.active_set -ceq 'default' -and
        $State.d1.ready -is [bool] -and $State.d2.ready -is [bool] -and $State.can_launch -is [bool] -and
        $State.d1.ready -eq $D1Ready -and $State.d2.ready -eq $D2Ready -and
        $State.can_launch -eq ($D1Ready -or $D2Ready))
}

function Tap-FirstPlayButton {
    param([ValidateSet('Select Game Files or Archive to Import', 'Import to Current Set', 'Import All')][string]$Text)
    # Host argv quoting does not survive adb shell's second parse; these fixed labels contain no quotes
    $quotedText = "'$Text'"
    # Let each five-second MP search finish before retrying its scroll/click request
    $tapped = Send-TapButton -Serial $Serial -Text $quotedText -TimeoutSec 120 -PollMs 10000
    $tapLog = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I', '*:S')
    if (-not $tapped -or $tapLog -notmatch ('tap_button: tapped "' + [regex]::Escape($Text) + '"')) {
        @(
            "requested_button=$Text"
            "remote_shell_argument=$quotedText"
            (Adb -AdbArgs @('logcat', '-d', '-t', '200', '-s', 'DXX-MP:*', 'DXX-Setup:*', '*:S'))
        ) | Set-Content (Join-Path $firstPlayOutput 'tap-button-failure.log') -Encoding utf8
        throw "Could not tap first-play button: $Text"
    }
}

function Get-FirstPlayPickerChoicePoint {
    param([string]$XmlText)
    try { [xml]$document = $XmlText } catch { return $null }
    if ($null -eq $document -or $null -eq $document.DocumentElement) { return $null }
    if (-not $document.SelectSingleNode("//node[@package='$($script:PACKAGE)' and @text='Import Files']")) { return $null }
    $choices = @($document.SelectNodes("//node[@package='$($script:PACKAGE)' and @text='Pick One or More Files' and @enabled='true']"))
    if ($choices.Count -ne 1) { return $null }
    $bounds = [regex]::Match($choices[0].GetAttribute('bounds'), '^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$')
    if (-not $bounds.Success) { return $null }
    $left, $top, $right, $bottom = 1..4 | ForEach-Object { [int]$bounds.Groups[$_].Value }
    if ($right -le $left -or $bottom -le $top) { return $null }
    return [pscustomobject]@{ X = [int](($left + $right) / 2); Y = [int](($top + $bottom) / 2) }
}

function Test-FirstPlayDocumentsUiRoot {
    param([string]$XmlText, [string]$ActivityText)
    try { [xml]$document = $XmlText } catch { return $false }
    if ($null -eq $document -or $null -eq $document.DocumentElement) { return $false }
    $roots = @($document.SelectNodes('/hierarchy/node'))
    if ($roots.Count -ne 1) { return $false }
    $package = $roots[0].GetAttribute('package')
    if ($package -cnotin @('com.android.documentsui', 'com.google.android.documentsui')) { return $false }
    $resumed = @([regex]::Matches($ActivityText,
            '(?m)^\s*(?:topResumedActivity|mResumedActivity|ResumedActivity)\s*[:=]\s*ActivityRecord\{[^\r\n}]*\s(?<component>[A-Za-z0-9_.]+/[A-Za-z0-9_.]+)(?:\s|\})') |
            ForEach-Object { $_.Groups['component'].Value })
    $expected = "$package/com.android.documentsui.picker.PickActivity"
    return ($resumed.Count -gt 0 -and @($resumed | Where-Object {
                $_ -cne $expected -and -not ($package -ceq 'com.android.documentsui' -and $_ -ceq 'com.android.documentsui/.picker.PickActivity')
            }).Count -eq 0)
}

function Assert-FirstPlayFiles {
    param([array]$Expected, [string]$Label)
    $state = Wait-FirstPlaySetup -Label $Label -Predicate { param($s) $s.active_set -ceq 'default' }
    if (-not $state.PSObject.Properties['set_files_recursive']) { throw 'Missing fresh file inventory' }
    $allowed = @('assets.json', 'content_state.json') + @($Expected | ForEach-Object File)
    $unexpected = @($state.set_files_recursive | Where-Object { $_ -cnotin $allowed })
    if ($unexpected.Count) { throw "Unexpected files could contaminate public first-play: $($unexpected -join ', ')" }
    $hashes = @(
        foreach ($file in $Expected) {
            if ($file.File -cnotin $state.set_files_recursive) { throw "Published public asset missing from fresh inventory: $($file.File)" }
            $path = "$($script:DEFAULT_SET_DIR)/$($file.File)"
            $actual = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', $path)
            if ($actual -cne "$($file.Sha256)  $path") { throw "Published public asset differs: $($file.File)" }
            [ordered]@{ file = $file.File; sha256 = $file.Sha256; source = 'production post-picker import' }
        }
    )
    ConvertTo-Json -InputObject $hashes -Depth 5 | Set-Content (Join-Path $firstPlayOutput "$Label-hashes.json") -Encoding utf8
}

function Send-FirstPlaySource {
    param([string]$Path, [string]$Name, [string]$Sha256)
    $temporary = "/data/local/tmp/first-play-${PID}-$Name"
    try {
        Adb -AdbArgs @('push', $Path, $temporary) -Seconds 60 | Out-Null
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $temporary, "files/saf_provider_source/$Name") | Out-Null
        $actual = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', "files/saf_provider_source/$Name")
        if ($actual -cne "$Sha256  files/saf_provider_source/$Name") { throw "SAF source staging failed: $Name" }
    } finally {
        Adb -AdbArgs @('shell', 'rm', '-f', $temporary) | Out-Null
    }
}

function Get-FirstPlayBytesHash {
    param([byte[]]$Bytes)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash($Bytes)).ToLowerInvariant() } finally { $sha.Dispose() }
}

function Get-FirstPlayHogMember {
    param([byte[]]$Bytes, [string]$Name, [ValidateSet(2292566, 2339773)][int]$MaxBytes = 2292566)
    if ($Bytes.Length -lt 3 -or $Bytes.Length -gt $MaxBytes -or
        [Text.Encoding]::ASCII.GetString($Bytes, 0, 3) -cne 'DHF') { throw 'Invalid bounded public HOG' }
    $stream = [IO.MemoryStream]::new($Bytes, $false)
    $reader = [IO.BinaryReader]::new($stream)
    $found = $null
    try {
        $stream.Position = 3
        while ($stream.Position -lt $stream.Length) {
            if ($stream.Length - $stream.Position -lt 17) { throw 'Truncated public HOG header' }
            $entry = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(13)).TrimEnd([char]0)
            $size = $reader.ReadUInt32()
            if ($size -gt $stream.Length - $stream.Position) { throw 'Truncated public HOG member' }
            if ($entry -ieq $Name) {
                if ($null -ne $found) { throw 'Duplicate public HOG member' }
                $found = $reader.ReadBytes([int]$size)
            } else { $stream.Position += $size }
        }
    } finally { $reader.Dispose(); $stream.Dispose() }
    if ($null -eq $found) { throw 'Required public HOG member missing' }
    return , $found
}

function New-FirstPlayMission {
    param([string]$SourceHog, [string]$OutputDirectory)
    # Derive only the original demo level; no authored geometry or replacement assets
    if ((Get-Item -LiteralPath $SourceHog).Length -ne 2292566) { throw 'Public mission source is not the pinned D2 demo HOG' }
    $bytes = [IO.File]::ReadAllBytes($SourceHog)
    $sourceHash = Get-FirstPlayBytesHash $bytes
    if ($bytes.Length -ne 2292566 -or
        $sourceHash -cne 'b6bf5514b7f2c25ff516c46e9d49eef5862b10667a95365631e7a64a10adc47e') {
        throw 'Public mission source is not the pinned D2 demo HOG'
    }
    $level = Get-FirstPlayHogMember -Bytes $bytes -Name 'd2leva-1.sl2'
    $levelHash = Get-FirstPlayBytesHash $level
    if ($level.Length -ne 75440 -or
        $levelHash -cne '133f34dff976ad7e5a84e2c4a16301f89bd833c954e9903c899613d6f7299302') {
        throw 'Public mission level differs from the original demo member'
    }
    $hog = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($hog)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('DHF'))
        $name = [byte[]]::new(13)
        [Text.Encoding]::ASCII.GetBytes('fpplay1.rl2').CopyTo($name, 0)
        $writer.Write($name)
        $writer.Write([uint32]$level.Length)
        $writer.Write($level)
        $writer.Flush()
        $missionHog = $hog.ToArray()
    } finally { $writer.Dispose(); $hog.Dispose() }
    $descriptor = [Text.Encoding]::ASCII.GetBytes("name = Public First Play`ntype = normal`nnum_levels = 1`nfpplay1.rl2`nnum_secrets = 0`n")
    $path = Join-Path $OutputDirectory 'fpplay.zip'
    $archive = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entry in @(@{ Name = 'fpplay.mn2'; Bytes = $descriptor }, @{ Name = 'fpplay.hog'; Bytes = $missionHog })) {
            $item = $archive.CreateEntry($entry.Name, [IO.Compression.CompressionLevel]::NoCompression)
            $item.LastWriteTime = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
            $output = $item.Open()
            try { $output.Write($entry.Bytes, 0, $entry.Bytes.Length) } finally { $output.Dispose() }
        }
    } finally { $archive.Dispose() }
    $identity = [ordered]@{
        file = 'fpplay.zip'; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        source_hog_sha256 = $sourceHash; source_member = 'd2leva-1.sl2'; level_sha256 = $levelHash
        level_size = $level.Length; mission_hog_sha256 = (Get-FirstPlayBytesHash $missionHog)
        descriptor = 'fpplay.mn2'; level_alias = 'fpplay1.rl2'; level_title = 'Ahayweh Gate'
        scope = 'Synthetic packaging of unchanged public level bytes; no retail or authored mission qualification'
    }
    $identity | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'mission-fixture.json') -Encoding utf8
    return @{ Path = $path; File = $identity.file; Sha256 = $identity.sha256 }
}

function Test-FirstPlayMissionPublication {
    param($State, [string]$Log, [string]$HashLine, [string]$Sha256)
    if (-not (Test-FirstPlayReadiness $State $true $true)) { return $false }
    $mods = @($State.content_entries | Where-Object { $_.id -like 'mod:*' })
    return ($mods.Count -eq 1 -and $mods[0].id -ceq 'mod:fpplay.zip' -and
        $mods[0].kind -ceq 'mission_zip' -and $mods[0].game -ceq 'd2' -and
        $mods[0].enabled -is [bool] -and $mods[0].enabled -and
        $mods[0].managed -is [bool] -and $mods[0].managed -and
        $mods[0].name -ceq 'Public First Play' -and
        $Log -match 'Mission ZIP import ok: fpplay\.zip \([0-9]+ bytes\)' -and
        $Sha256 -cmatch '^[0-9a-f]{64}$' -and
        $HashLine -ceq "$Sha256  $($script:DEFAULT_SET_DIR)/.content/mods/fpplay.zip")
}

function Import-FirstPlayMission {
    param([Parameter(Mandatory)][hashtable]$Fixture)
    Send-FirstPlaySource -Path $Fixture.Path -Name $Fixture.File -Sha256 $Fixture.Sha256
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    # Mission ZIPs import immediately through this production post-picker branch
    # Unlike base installers, this route has no Import to Current Set confirmation
    $uri = "content://$($script:PACKAGE).saf-test/seekable/fpplay.zip"
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
        '--es', 'command', 'import_picked_uris', '--esa', 'uris', $uri) | Out-Null
    $publication = @{ Log = ''; State = $null }
    $published = Wait-ForCondition -Description 'public mission production publication' -TimeoutSec 90 -PollMs 500 -Condition {
        $publication.Log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-Setup:I', '*:S')
        $publication.State = Get-FirstPlaySetup
        $hash = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', "$($script:DEFAULT_SET_DIR)/.content/mods/fpplay.zip")
        Test-FirstPlayMissionPublication -State $publication.State -Log $publication.Log -HashLine $hash -Sha256 $Fixture.Sha256
    }
    $publication.Log | Set-Content (Join-Path $firstPlayOutput 'mission-import.log') -Encoding utf8
    $publication.State | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $firstPlayOutput 'mission-import-setup.json') -Encoding utf8
    if (-not $published) { throw 'Public mission lacks fresh production import, exact bytes, or enabled ownership' }
}

function Import-FirstPlaySources {
    param([string[]]$Names, [ValidateSet('archive', 'loose')][string]$Kind, [string]$Label,
        [Parameter(Mandatory)][array]$Expected)
    try {
        # This is the production callback after a picker result, not DocumentsUI selection
        $uris = @($Names | ForEach-Object { "content://$($script:PACKAGE).saf-test/seekable/$_" }) -join ','
        Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
            '--es', 'command', 'import_picked_uris', '--esa', 'uris', $uris) | Out-Null
        $button = if ($Kind -eq 'archive') { 'Import to Current Set' } else { 'Import All' }
        # The result card can be below the viewport; the existing tap helper scrolls
        # to it and requires an actual click acknowledgment, not mere button absence
        Tap-FirstPlayButton $button
        $publication = @{ Log = '' }
        $published = Wait-ForCondition -Description "$Label production publication" -TimeoutSec 90 -PollMs 500 -Condition {
            $publication.Log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-Setup:I', 'DXX-MP:I', '*:S')
            if ($Kind -eq 'loose') {
                # The tap helper cleared logcat before confirmation; old files cannot prove a repeat
                foreach ($name in $Names) {
                    if ($publication.Log -notmatch ("Imported " + [regex]::Escape($name) + ' -> ' +
                            [regex]::Escape($name) + ' \([0-9]+ bytes\)')) { return $false }
                }
            }
            foreach ($file in $Expected) {
                $path = "$($script:DEFAULT_SET_DIR)/$($file.File)"
                $actual = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', $path)
                if ($actual -cne "$($file.Sha256)  $path") { return $false }
            }
            return $true
        }
        $publication.Log | Set-Content (Join-Path $firstPlayOutput "$Label-import.log") -Encoding utf8
        if (-not $published) { throw "$Label lacks fresh production publication with the expected hashes" }
        Assert-FirstPlayFiles -Expected $Expected -Label "$Label-published"
        [ordered]@{ kind = $Kind; files = $Names; callback = 'import_picked_uris'; provider = 'SafTestProvider'; confirmation = $button } |
            ConvertTo-Json -Depth 5 | Set-Content (Join-Path $firstPlayOutput "$Label-import.json") -Encoding utf8
    } catch {
        # Preserve the actual post-import UI before the owner's cleanup stops the app
        try {
            Get-FirstPlaySetup | ConvertTo-Json -Depth 100 |
                Set-Content (Join-Path $firstPlayOutput "$Label-failure-setup.json") -Encoding utf8
            Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/setup_buttons.json') | Out-Null
            Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT', '--ez', 'buttons_only', 'true') | Out-Null
            Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/setup_buttons.json') |
                Set-Content (Join-Path $firstPlayOutput "$Label-failure-buttons.json") -Encoding utf8
            $dump = '/sdcard/first-play-picker.xml'
            Adb -AdbArgs @('shell', 'rm', '-f', $dump) | Out-Null
            Adb -AdbArgs @('shell', 'uiautomator', 'dump', $dump) -Seconds 10 | Out-Null
            Adb -AdbArgs @('shell', 'cat', $dump) |
                Set-Content (Join-Path $firstPlayOutput "$Label-failure-ui.xml") -Encoding utf8
        } catch { Write-Warning "Could not capture import failure UI: $_" }
        throw
    }
}

function Test-FirstPlayDocumentsUiCancel {
    param([Parameter(Mandatory)][array]$Expected)
    # One click request only: retrying while the previous MP coroutine scrolls can
    # leave overlapping gestures after the dialog or system picker has opened
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Send-MpCommand -Serial $Serial -Command 'tap_button' -Extras @('--es', 'text', "'Select Game Files or Archive to Import'")
    $dump = '/sdcard/first-play-picker.xml'
    # The Compose chooser owns a separate window; observe the actual focused UI
    # rather than the MP helper's covered activity root (test_mac_extract_saf pattern)
    $choice = @{ Point = $null; DocumentsUi = $false }
    $chooserReady = Wait-ForCondition -Description 'exact chooser or system picker visible' -TimeoutSec 30 -PollMs 500 -Condition {
        Adb -AdbArgs @('shell', 'rm', '-f', $dump) | Out-Null
        Adb -AdbArgs @('shell', 'uiautomator', 'dump', $dump) -Seconds 10 | Out-Null
        $xml = Adb -AdbArgs @('shell', 'cat', $dump)
        $xml | Set-Content (Join-Path $firstPlayOutput 'import-chooser.xml') -Encoding utf8
        $activity = Adb -AdbArgs @('shell', 'dumpsys', 'activity', 'activities')
        $activity | Set-Content (Join-Path $firstPlayOutput 'import-chooser-activity.txt') -Encoding utf8
        $choice.DocumentsUi = Test-FirstPlayDocumentsUiRoot -XmlText $xml -ActivityText $activity
        $choice.Point = Get-FirstPlayPickerChoicePoint -XmlText $xml
        return $choice.DocumentsUi -or $null -ne $choice.Point
    }
    if (-not $chooserReady) { throw 'Neither the exact import chooser nor the system picker became visible' }
    if (-not $choice.DocumentsUi) {
        Adb -AdbArgs @('shell', 'input', 'tap', [string]$choice.Point.X, [string]$choice.Point.Y) | Out-Null
    }
    # Even an already-open picker requires a second fresh observation before Back
    $opened = Wait-ForCondition -Description 'system DocumentsUI opened' -TimeoutSec 30 -PollMs 500 -Condition {
        Adb -AdbArgs @('shell', 'rm', '-f', $dump) | Out-Null
        Adb -AdbArgs @('shell', 'uiautomator', 'dump', $dump) -Seconds 10 | Out-Null
        $xml = Adb -AdbArgs @('shell', 'cat', $dump)
        $xml | Set-Content (Join-Path $firstPlayOutput 'documentsui-open.xml') -Encoding utf8
        $activity = Adb -AdbArgs @('shell', 'dumpsys', 'activity', 'activities')
        $activity | Set-Content (Join-Path $firstPlayOutput 'documentsui-open-activity.txt') -Encoding utf8
        Test-FirstPlayDocumentsUiRoot -XmlText $xml -ActivityText $activity
    }
    if (-not $opened) { throw 'Import chooser did not open the system DocumentsUI' }
    Adb -AdbArgs @('shell', 'input', 'keyevent', 'KEYCODE_BACK') | Out-Null
    $returned = Wait-ForCondition -Description 'DocumentsUI cancellation returned to launcher' -TimeoutSec 30 -PollMs 500 -Condition {
        Adb -AdbArgs @('shell', 'rm', '-f', $dump) | Out-Null
        Adb -AdbArgs @('shell', 'uiautomator', 'dump', $dump) -Seconds 10 | Out-Null
        $xml = Adb -AdbArgs @('shell', 'cat', $dump)
        $xml | Set-Content (Join-Path $firstPlayOutput 'documentsui-cancel.xml') -Encoding utf8
        $activity = Adb -AdbArgs @('shell', 'dumpsys', 'activity', 'activities')
        $activity | Set-Content (Join-Path $firstPlayOutput 'documentsui-cancel-activity.txt') -Encoding utf8
        $xml -match ('package="' + [regex]::Escape($script:PACKAGE) + '"') -and
        $activity -match ('(?m)^\s*(?:topResumedActivity|mResumedActivity|ResumedActivity)\s*[:=].*\s' +
            [regex]::Escape($script:PACKAGE) + '/(?:\.SetupActivity|com\.dxxredux\.app\.SetupActivity)(?:\s|\})') -and
        -not (Test-FirstPlayDocumentsUiRoot -XmlText $xml -ActivityText $activity)
    }
    if (-not $returned) { throw 'Canceling DocumentsUI did not return to the launcher' }
    Wait-FirstPlaySetup -Label 'picker-cancel' -Predicate { param($s) Test-FirstPlayReadiness $s $true $true } | Out-Null
    Assert-FirstPlayFiles -Expected $Expected -Label 'picker-cancel'
    Write-Output 'PASS: actual DocumentsUI open/cancel; installer selection is separately injected through SafTestProvider'
}

function Assert-FirstPlayReplacementLevel {
    param([string]$Alias, [byte[]]$Descriptor, [byte[]]$Hog, [byte[]]$Level)
    # d1/main/gamemine.c selects the shareware decoder by the exact lowercase .sdl extension
    if ($Alias -cnotin @('fpold1.sdl', 'fpnew1.sdl')) { throw 'Replacement level must preserve the D1 shareware extension' }
    $title = if ($Alias -ceq 'fpold1.sdl') { 'Public Import Old' } else { 'Public Import New' }
    $expected = "name = $title`ntype = normal`nnum_levels = 1`n$Alias`nnum_secrets = 0`n"
    if ([Text.Encoding]::ASCII.GetString($Descriptor) -cne $expected -or
        $Level.Length -eq 0 -or $Hog.Length -ne $Level.Length + 20 -or
        [Text.Encoding]::ASCII.GetString($Hog, 3, 13).TrimEnd([char]0) -cne $Alias) {
        throw 'Replacement descriptor and single HOG member must name the same shareware level'
    }
    $member = Get-FirstPlayHogMember -Bytes $Hog -Name $Alias -MaxBytes 2339773
    if ((Get-FirstPlayBytesHash $member) -cne (Get-FirstPlayBytesHash $Level)) {
        throw 'Replacement HOG changed the original shareware level bytes'
    }
}

function New-FirstPlayReplacementMission {
    param([string]$SourceHog, [string]$OutputDirectory, [ValidateSet('old', 'new')][string]$Generation)
    # Both generations use unchanged level one from the pinned original D1 shareware HOG
    if ((Get-Item -LiteralPath $SourceHog).Length -ne 2339773) { throw 'Replacement source is not the pinned D1 HOG' }
    $bytes = [IO.File]::ReadAllBytes($SourceHog)
    $sourceHash = Get-FirstPlayBytesHash $bytes
    if ($sourceHash -cne '26d1e31e7709dfe6dddf17ccd37f5c82e866dce49a0faf07e90ba3213b288eab') {
        throw 'Replacement source is not the pinned D1 HOG'
    }
    $level = Get-FirstPlayHogMember -Bytes $bytes -Name 'level01.sdl' -MaxBytes 2339773
    $title = if ($Generation -ceq 'old') { 'Public Import Old' } else { 'Public Import New' }
    $alias = if ($Generation -ceq 'old') { 'fpold1.sdl' } else { 'fpnew1.sdl' }
    $hog = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($hog)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('DHF'))
        $name = [byte[]]::new(13)
        [Text.Encoding]::ASCII.GetBytes($alias).CopyTo($name, 0)
        $writer.Write($name)
        $writer.Write([uint32]$level.Length)
        $writer.Write($level)
        $writer.Flush()
        $missionHog = $hog.ToArray()
    } finally { $writer.Dispose(); $hog.Dispose() }
    $descriptor = [Text.Encoding]::ASCII.GetBytes("name = $title`ntype = normal`nnum_levels = 1`n$alias`nnum_secrets = 0`n")
    Assert-FirstPlayReplacementLevel -Alias $alias -Descriptor $descriptor -Hog $missionHog -Level $level
    $directory = Join-Path $OutputDirectory "interruption-$Generation"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $path = Join-Path $directory 'fpinterrupt.zip'
    $payloads = @(@{ Name = 'fpinterrupt.msn'; Bytes = $descriptor }, @{ Name = 'fpinterrupt.hog'; Bytes = $missionHog })
    $archive = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entry in $payloads) {
            $item = $archive.CreateEntry($entry.Name, [IO.Compression.CompressionLevel]::NoCompression)
            $item.LastWriteTime = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
            $output = $item.Open()
            try { $output.Write($entry.Bytes, 0, $entry.Bytes.Length) } finally { $output.Dispose() }
        }
        # Actual stored bytes cross the 10 MiB durable-extraction threshold, without a ZIP bomb
        $padding = $archive.CreateEntry('docs/import-padding.bin', [IO.Compression.CompressionLevel]::NoCompression)
        $padding.LastWriteTime = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
        $output = $padding.Open()
        $block = [byte[]]::new(65536)
        $block[0] = if ($Generation -ceq 'old') { 79 } else { 78 }
        $paddingDigest = [Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
        try {
            foreach ($index in 1..192) { $output.Write($block, 0, $block.Length); $paddingDigest.AppendData($block) }
            $paddingHash = [Convert]::ToHexString($paddingDigest.GetHashAndReset()).ToLowerInvariant()
        } finally { $output.Dispose(); $paddingDigest.Dispose() }
    } finally { $archive.Dispose() }
    $size = (Get-Item -LiteralPath $path).Length
    if ($size -le 12MB -or $size -ge 15MB) { throw 'Replacement fixture escaped its bounded size' }
    $fixture = @{
        Path = $path; File = 'fpinterrupt.zip'; Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        Size = $size; Title = $title; Alias = $alias; SourceSha256 = $sourceHash
        LevelSha256 = (Get-FirstPlayBytesHash $level); DescriptorSha256 = (Get-FirstPlayBytesHash $descriptor)
        HogSha256 = (Get-FirstPlayBytesHash $missionHog); PaddingSha256 = $paddingHash
        DescriptorSize = $descriptor.Length; HogSize = $missionHog.Length
    }
    $fixture | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $directory 'fixture.json') -Encoding utf8
    return $fixture
}

function Test-FirstPlayReplacementDiskIdentity {
    param($Manifest, [string]$HashLine, [hashtable]$Fixture)
    $mods = @($Manifest.mods)
    return ($Fixture.Sha256 -cmatch '^[a-f0-9]{64}$' -and $mods.Count -eq 1 -and
        $mods[0].filename -ceq 'fpinterrupt.zip' -and $mods[0].displayName -ceq $Fixture.Title -and
        $mods[0].kind -ceq 'mission_zip' -and $mods[0].game -ceq 'd1' -and
        $mods[0].enabled -is [bool] -and $mods[0].enabled -and $mods[0].importMode -ceq 'extracted_bundle' -and
        $mods[0].archiveSha256 -ceq $Fixture.Sha256 -and $mods[0].sizeBytes -eq $Fixture.Size -and
        $HashLine -ceq "$($Fixture.Sha256)  $($script:DEFAULT_SET_DIR)/.content/mods/fpinterrupt.zip")
}

function Assert-FirstPlayReplacementDisk {
    param([hashtable]$Fixture, [string]$Label)
    $manifestText = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "$($script:DEFAULT_SET_DIR)/.content/mods/mod_manifest.json")
    $hash = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', "$($script:DEFAULT_SET_DIR)/.content/mods/fpinterrupt.zip")
    $manifestText | Set-Content (Join-Path $firstPlayOutput "$Label-manifest.json") -Encoding utf8
    $hash | Set-Content (Join-Path $firstPlayOutput "$Label-archive-hash.txt") -Encoding utf8
    $manifest = $manifestText | ConvertFrom-Json
    if (-not (Test-FirstPlayReplacementDiskIdentity -Manifest $manifest -HashLine $hash -Fixture $Fixture)) {
        throw "$Label lost the exact published mission generation or has duplicate ownership"
    }
}

function Test-FirstPlayReplacementCacheIdentity {
    param($Manifest, [hashtable]$Fixture)
    $records = @($Manifest.entries)
    if ($records.Count -ne 1 -or $records[0].owner_filename -cne 'fpinterrupt.zip' -or
        $records[0].owner_sha256 -cne $Fixture.Sha256 -or $records[0].owner_size_bytes -ne $Fixture.Size -or
        $records[0].import_mode -cne 'extracted_bundle') { return $false }
    $expected = @{
        'fpinterrupt.msn' = @{ Path = 'missions/fpinterrupt.msn'; Size = $Fixture.DescriptorSize; Hash = $Fixture.DescriptorSha256 }
        'fpinterrupt.hog' = @{ Path = 'missions/fpinterrupt.hog'; Size = $Fixture.HogSize; Hash = $Fixture.HogSha256 }
        'docs/import-padding.bin' = @{ Path = 'docs/import-padding.bin'; Size = 12MB; Hash = $Fixture.PaddingSha256 }
    }
    $files = @($records[0].files)
    if ($files.Count -ne 3 -or @($files.entry_path | Sort-Object -Unique).Count -ne 3) { return $false }
    foreach ($file in $files) {
        $entry = $expected[$file.entry_path]
        if (-not $entry -or $file.relative_path -cne $entry.Path -or $file.size_bytes -ne $entry.Size -or
            $entry.Hash -cnotmatch '^[a-f0-9]{64}$' -or $file.sha256 -cne $entry.Hash) { return $false }
    }
    return $true
}

function Assert-FirstPlayReplacementCache {
    param([hashtable]$Fixture, [string]$Label)
    $root = "$($script:DEFAULT_SET_DIR)/.content/mod_support/mods/.extracted_mission_zips"
    $raw = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "$root/manifest.json")
    $raw | Set-Content (Join-Path $firstPlayOutput "$Label-extraction.json") -Encoding utf8
    $manifest = $raw | ConvertFrom-Json
    if (-not (Test-FirstPlayReplacementCacheIdentity -Manifest $manifest -Fixture $Fixture)) {
        throw "$Label has duplicate, incomplete or stale mission extraction identity"
    }
    foreach ($file in $manifest.entries[0].files) {
        $path = "$root/fpinterrupt.zip/$($file.relative_path)"
        $actual = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', $path)
        if ($actual -cne "$($file.sha256)  $path") { throw "$Label extracted mission bytes differ" }
    }
    $temporary = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sh', '-c',
        "'if test -e $root/fpinterrupt.zip.tmp; then echo STALE; else echo CLEAN; fi'")
    if ($temporary -cne 'CLEAN') { throw "$Label retained an incomplete extraction after successful import" }
}

function Assert-FirstPlayReplacementReady {
    param([hashtable]$Fixture, [string]$Label)
    Assert-FirstPlayReplacementDisk -Fixture $Fixture -Label $Label
    Wait-FirstPlaySetup -Label $Label -Predicate {
        param($state)
        if (-not (Test-FirstPlayReadiness $state $true $false)) { return $false }
        $mods = @($state.content_entries | Where-Object { $_.id -like 'mod:*' })
        return ($mods.Count -eq 1 -and $mods[0].id -ceq 'mod:fpinterrupt.zip' -and
            $mods[0].name -ceq $Fixture.Title -and $mods[0].game -ceq 'd1' -and $mods[0].kind -ceq 'mission_zip' -and
            $mods[0].enabled -is [bool] -and $mods[0].enabled -and $mods[0].managed -is [bool] -and $mods[0].managed)
    } | Out-Null
}

function Import-FirstPlayReplacement {
    param([hashtable]$Fixture, [string]$Label)
    Send-FirstPlaySource -Path $Fixture.Path -Name $Fixture.File -Sha256 $Fixture.Sha256
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
        '--es', 'command', 'import_picked_uris', '--esa', 'uris', "content://$($script:PACKAGE).saf-test/seekable/fpinterrupt.zip") | Out-Null
    $publication = @{ Log = '' }
    $published = Wait-ForCondition -Description "$Label mission publication" -TimeoutSec 90 -PollMs 500 -Condition {
        $publication.Log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-Setup:I', '*:S')
        if ($publication.Log -notmatch 'Mission ZIP import ok: fpinterrupt\.zip \([0-9]+ bytes\)') { return $false }
        $raw = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "$($script:DEFAULT_SET_DIR)/.content/mods/mod_manifest.json")
        try { $manifest = $raw | ConvertFrom-Json } catch { return $false }
        $hash = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', "$($script:DEFAULT_SET_DIR)/.content/mods/fpinterrupt.zip")
        Test-FirstPlayReplacementDiskIdentity -Manifest $manifest -HashLine $hash -Fixture $Fixture
    }
    $publication.Log | Set-Content (Join-Path $firstPlayOutput "$Label-import.log") -Encoding utf8
    if (-not $published) { throw "$Label lacks fresh production publication of the exact mission generation" }
    Assert-FirstPlayReplacementReady -Fixture $Fixture -Label $Label
    Assert-FirstPlayReplacementCache -Fixture $Fixture -Label $Label
}

function Test-FirstPlayInterruptionMarker {
    param($Marker, [string]$Nonce, [hashtable]$Previous, [hashtable]$Replacement, [string]$ProcessId)
    return ($Nonce -cmatch '^[a-f0-9]{32}$' -and $ProcessId -cmatch '^[1-9][0-9]*$' -and
        $Marker.run_id -ceq $Nonce -and $Marker.checkpoint -ceq 'extracting_before_publication' -and
        [string]$Marker.process_id -ceq $ProcessId -and
        $Marker.previous_sha256 -ceq $Previous.Sha256 -and $Marker.replacement_sha256 -ceq $Replacement.Sha256 -and
        $Previous.Sha256 -cmatch '^[a-f0-9]{64}$' -and $Replacement.Sha256 -cmatch '^[a-f0-9]{64}$' -and
        $Previous.Sha256 -cne $Replacement.Sha256 -and
        ($Marker.bytes_done -is [int] -or $Marker.bytes_done -is [long]) -and
        ($Marker.bytes_total -is [int] -or $Marker.bytes_total -is [long]) -and
        $Marker.bytes_done -ge 1MB -and $Marker.bytes_done -lt $Marker.bytes_total -and
        $Marker.bytes_total -gt 12MB -and $Marker.bytes_total -lt 15MB)
}

function Test-FirstPlayInterruptionDeadline {
    param([double]$ElapsedSeconds)
    # The 90-second device barrier cannot begin before the host starts instrumentation
    # Allow five seconds for a confirmed force-stop, retaining at least ten seconds of margin
    return ($ElapsedSeconds -ge 0 -and $ElapsedSeconds -lt 75)
}

function Test-FirstPlayProcessDead {
    param([AllowNull()][string]$Output, $Diagnostic)
    return ($Diagnostic -and $Diagnostic.exit_code -is [int] -and $Diagnostic.exit_code -eq 1 -and
        $Diagnostic.timed_out -is [bool] -and -not $Diagnostic.timed_out -and
        [string]::IsNullOrWhiteSpace($Output) -and [string]::IsNullOrWhiteSpace($Diagnostic.error) -and
        [string]::IsNullOrWhiteSpace($Diagnostic.stderr))
}

function Invoke-FirstPlayInterruptedReplacement {
    param([string]$SourceHog, [array]$BaseFiles)
    $previous = New-FirstPlayReplacementMission -SourceHog $SourceHog -OutputDirectory $firstPlayOutput -Generation old
    $replacement = New-FirstPlayReplacementMission -SourceHog $SourceHog -OutputDirectory $firstPlayOutput -Generation new
    $nonce = [Guid]::NewGuid().ToString('N')
    try {
        Import-FirstPlayReplacement -Fixture $previous -Label 'interruption-old-import'
        Invoke-FirstPlayPhase interruption_old -EvidenceLabel 'interruption-old-before'
        Assert-FirstPlayReplacementReady -Fixture $previous -Label 'interruption-old-before'
        Send-FirstPlaySource -Path $replacement.Path -Name $replacement.File -Sha256 $replacement.Sha256
        Assert-FirstPlayEmulator -Serial $Serial
        Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        $cleared = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sh', '-c',
            "'rm -f files/first_play_interrupt.json && echo FIRST_PLAY_INTERRUPT_FRESH'")
        if ($cleared -cne 'FIRST_PLAY_INTERRUPT_FRESH') { throw 'Could not invalidate interruption evidence' }
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        # No -w: the host must observe the running importer before killing it at its test-only barrier
        $interruptionClock = [Diagnostics.Stopwatch]::StartNew()
        Adb -AdbArgs @('shell', 'am', 'instrument', '-e', 'suite', 'demo_import_interruption',
            '-e', 'nonce', $nonce, '-e', 'previous_sha256', $previous.Sha256,
            '-e', 'replacement_sha256', $replacement.Sha256,
            "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation") |
            Set-Content (Join-Path $firstPlayOutput 'interruption-instrument-start.txt') -Encoding utf8
        $checkpoint = @{ Marker = $null; ProcessId = '' }
        $reached = Wait-ForCondition -Description 'nonzero incomplete extraction barrier' -TimeoutSec 60 -PollMs 500 -Condition {
            $raw = Adb -Seconds 5 -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/first_play_interrupt.json')
            try { $checkpoint.Marker = $raw | ConvertFrom-Json } catch { return $false }
            $checkpoint.ProcessId = [string](Adb -Seconds 5 -AdbArgs @('shell', 'pidof', $script:PACKAGE))
            Test-FirstPlayInterruptionMarker -Marker $checkpoint.Marker -Nonce $nonce -Previous $previous `
                -Replacement $replacement -ProcessId $checkpoint.ProcessId
        }
        Adb -Seconds 5 -AdbArgs @('logcat', '-d') | Set-Content (Join-Path $firstPlayOutput 'interruption-before-kill.log') -Encoding utf8
        $checkpoint.Marker | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $firstPlayOutput 'interruption-marker.json') -Encoding utf8
        if (-not $reached) { throw 'Importer never reached the correlated incomplete-extraction barrier; no intentional interruption claimed' }
        Assert-FirstPlayReplacementDisk -Fixture $previous -Label 'interruption-at-barrier'
        # Recheck the PID immediately before stopping the package; a stale marker is never sufficient
        $livePid = [string](Adb -Seconds 5 -AdbArgs @('shell', 'pidof', $script:PACKAGE))
        if (-not (Test-FirstPlayInterruptionMarker -Marker $checkpoint.Marker -Nonce $nonce -Previous $previous `
                    -Replacement $replacement -ProcessId $livePid)) { throw 'Importer exited before the intentional interruption' }
        if (-not (Test-FirstPlayInterruptionDeadline $interruptionClock.Elapsed.TotalSeconds)) {
            throw 'Interruption evidence arrived too late to prove the importer is still at its barrier'
        }
        $stopDiagnostic = $null
        Adb-Timeout -Seconds 5 -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) `
            -DiagnosticResult ([ref]$stopDiagnostic) | Out-Null
        $stopElapsed = $interruptionClock.Elapsed.TotalSeconds
        [ordered]@{ elapsed_seconds = $stopElapsed; diagnostic = $stopDiagnostic } | ConvertTo-Json -Depth 5 |
            Set-Content (Join-Path $firstPlayOutput 'interruption-force-stop.json') -Encoding utf8
        if ($stopDiagnostic.exit_code -ne 0 -or $stopDiagnostic.timed_out -or $stopDiagnostic.error -or
            $stopDiagnostic.stderr -or $stopElapsed -ge 80) {
            throw 'Force-stop did not complete successfully before the extraction barrier deadline'
        }
        $death = @{ Diagnostic = $null; Output = '' }
        $dead = Wait-ForCondition -Description 'interrupted importer process death' -TimeoutSec 15 -PollMs 250 -Condition {
            $diagnostic = $null
            $death.Output = Adb-Timeout -Seconds 5 -AdbArgs @('shell', 'pidof', $script:PACKAGE) -DiagnosticResult ([ref]$diagnostic)
            $death.Diagnostic = $diagnostic
            Test-FirstPlayProcessDead -Output $death.Output -Diagnostic $death.Diagnostic
        }
        $death | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $firstPlayOutput 'interruption-process-death.json') -Encoding utf8
        if (-not $dead) { throw 'Transport did not confirm the interrupted importer process exited' }
        Assert-FirstPlayReplacementDisk -Fixture $previous -Label 'interruption-after-kill'
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'find', "$($script:DEFAULT_SET_DIR)/.content", '-type', 'f') |
            Set-Content (Join-Path $firstPlayOutput 'interruption-after-kill-files.txt') -Encoding utf8
        # Cache regeneration is allowed; only old publication identity and actual usability are required
        Invoke-FirstPlayPhase interruption_old -EvidenceLabel 'interruption-old-after'
        Assert-FirstPlayReplacementReady -Fixture $previous -Label 'interruption-old-after'
        foreach ($file in $BaseFiles) {
            $path = "$($script:DEFAULT_SET_DIR)/$($file.File)"
            $actual = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', $path)
            if ($actual -cne "$($file.Sha256)  $path") { throw 'Interrupted replacement changed public base data' }
        }
        # The same archive bytes are retried by the real launcher callback, without the interruption hook
        Import-FirstPlayReplacement -Fixture $replacement -Label 'interruption-retry'
        Invoke-FirstPlayPhase interruption_new
        Assert-FirstPlayFiles -Expected $BaseFiles -Label 'interruption-cleanup'
        [ordered]@{
            result = 'PASS'; run_id = $nonce; interrupted_process_id = $checkpoint.ProcessId
            previous_sha256 = $previous.Sha256; replacement_sha256 = $replacement.Sha256
            checkpoint = 'nonzero incomplete durable-cache extraction, before ZIP and mod-manifest publication'
            first_attempt = 'androidTest invokes production SAF mission importer with a blocking progress callback; host force-stops app'
            recovery = 'old mission normal D1 button/menu launch before retry; cache regeneration permitted'
            retry = 'identical replacement archive through production post-picker callback, then normal D1 mission launch'
        } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $firstPlayOutput 'interruption-summary.json') -Encoding utf8
        Write-Output 'PASS: interrupted public mission replacement preserved playable old content and retry reached the new mission'
    } finally {
        foreach ($fixture in @($previous, $replacement)) {
            if (Test-Path -LiteralPath $fixture.Path) { Remove-Item -LiteralPath $fixture.Path -Force }
        }
    }
}

function Assert-FirstPlayMissionReturnProcess {
    param([string]$Log)
    # Reuse the isolation owner's log-PID oracle, scoped to this post-resume transition
    $markers = [regex]::Matches($Log,
        '(?m)^\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d+\s+(?<process>[1-9]\d*)\s+\d+\s+I\s+DXX-Automate:\s+SCRIPT: FIRST_PLAY_RETURN (?<phase>resumed_custom|builtin_live)\r?$')
    if ($markers.Count -ne 2 -or $markers[0].Groups['phase'].Value -cne 'resumed_custom' -or
        $markers[1].Groups['phase'].Value -cne 'builtin_live' -or
        $markers[0].Groups['process'].Value -cne $markers[1].Groups['process'].Value) {
        throw 'Public mission return lacks ordered same-process custom-to-base evidence'
    }
    return $markers[0].Groups['process'].Value
}

function Invoke-FirstPlayPhase {
    param([ValidateSet('empty', 'd1', 'd1_in_d2', 'd2', 'mission', 'interruption_old', 'interruption_new')][string]$Phase,
        [ValidatePattern('^[a-z][a-z0-9_-]*$')][string]$EvidenceLabel = $Phase)
    $phaseScripts = @{
        empty = 'test_public_first_play_empty.jsonc'
        d1 = 'test_public_first_play_d1.jsonc'
        d1_in_d2 = 'test_public_first_play_d1_in_d2.jsonc'
        d2 = 'test_public_first_play_d2.jsonc'
        mission = 'test_public_first_play_mission.jsonc'
        interruption_old = 'test_public_first_play_interruption_old.jsonc'
        interruption_new = 'test_public_first_play_interruption_new.jsonc'
    }
    $name = $phaseScripts[$Phase]
    $phaseOutput = Join-Path $firstPlayOutput $EvidenceLabel
    New-Item -ItemType Directory -Force -Path $phaseOutput | Out-Null
    $pwsh = Get-RegressionCurrentPwshPath
    $phaseTimeout = if ($Phase -like 'interruption_*') { 120 } else { 180 }
    # No Params or edition override: only these exact reviewed repository scripts bypass data recovery seeding
    & $pwsh -NoProfile -NonInteractive -File "$PSScriptRoot/../helpers/run_test.ps1" `
        -ScriptName $name -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds $phaseTimeout 2>&1 |
        Tee-Object -FilePath (Join-Path $phaseOutput 'runner.log') | Out-Host
    $exitCode = $LASTEXITCODE
    foreach ($file in @('automation_result.json', 'automation_log.jsonl', 'introspect.json', 'setup_introspect.json')) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "files/$file") |
            Set-Content (Join-Path $phaseOutput $file) -Encoding utf8
    }
    # run_test force-stops the previous process, then clears logcat before this phase
    $nativeLog = Adb -AdbArgs @('logcat', '-d', '-s', 'MainActivity:I', '*:S')
    $nativeLog | Set-Content (Join-Path $phaseOutput 'native-library.log') -Encoding utf8
    if ($Phase -eq 'mission') {
        $returnLog = Adb -AdbArgs @('logcat', '-d', '-v', 'threadtime', '-s', 'DXX-Automate:I', '*:S')
        $returnLog | Set-Content (Join-Path $phaseOutput 'mission-return.log') -Encoding utf8
    }
    if ($exitCode -ne 0) { throw "First-play $Phase automation failed (exit $exitCode)" }
    $result = Get-Content (Join-Path $phaseOutput 'automation_result.json') -Raw | ConvertFrom-Json
    if (-not (Test-AutomationResultComplete -Result $result) -or $result.run_id -notmatch '^[a-f0-9]{32}$' -or
        -not $firstPlayRunIds.Add([string]$result.run_id)) { throw "First-play $Phase lacks a fresh complete durable PASS" }
    $runnerLog = Get-Content (Join-Path $phaseOutput 'runner.log') -Raw
    $expectedPushName = "test_public_first_play_$Phase.run-$($result.run_id).jsonc"
    if (-not $runnerLog.Contains($expectedPushName)) { throw "First-play $Phase result is not correlated to its staged script" }
    if ($Phase -eq 'mission') {
        $returnProcess = Assert-FirstPlayMissionReturnProcess -Log $returnLog
        [ordered]@{ run_id = $result.run_id; process_id = $returnProcess; transition = 'resumed_custom -> native menu -> builtin_live' } |
            ConvertTo-Json | Set-Content (Join-Path $phaseOutput 'mission-return-process.json') -Encoding utf8
    }
    if ($Phase -eq 'empty') {
        if ($nativeLog -match 'Loaded native library:') { throw 'An engine started despite missing game files' }
    } else {
        # D1 has no asset_trace.runtime_engine field, so require the actual native-library load too
        $engine = if ($Phase -eq 'd1' -or $Phase -like 'interruption_*') { 'd1' } else { 'd2' }
        $otherEngine = if ($engine -eq 'd1') { 'd2' } else { 'd1' }
        if ($nativeLog -notmatch "Loaded native library: dxx-redux-$engine\b" -or
            $nativeLog -match "Loaded native library: dxx-redux-$otherEngine\b") { throw "First-play $Phase loaded the wrong native engine or lacks its load evidence" }
    }
    Write-Output "PASS: public first-play $Phase, run_id=$($result.run_id)"
}

function Invoke-PublicFirstPlay {
    param([string]$Serial)
    . "$PSScriptRoot/../helpers/shareware_test_data.ps1"
    # Verify both pinned originals before touching the emulator; no implicit download or corpus expansion
    $configs = @('d1', 'd2') | ForEach-Object { Get-SharewareTestGameConfig -RepoRoot $script:REPO_ROOT -Game $_ }
    foreach ($config in $configs) {
        $archive = Join-Path $script:REPO_ROOT "game_data/demo installers/$($config.Archive)"
        if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Original public first-play fixture missing: $archive" }
        Assert-DxxFileSha256 -Path $archive -ExpectedSha256 $config.ArchiveSha256 -Label $config.Archive | Out-Null
        if ((Get-Item -LiteralPath $archive).Length -ne $config.ArchiveSize) { throw "Original public installer size differs: $archive" }
    }
    $d1Loose = Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game d1
    $d2Loose = Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game d2
    Assert-FirstPlayEmulator -Serial $Serial
    Ensure-EmulatorHealthy | Out-Null
    # Health recovery can change the target state; recheck immediately before destructive clearing
    Assert-FirstPlayEmulator -Serial $Serial
    $firstPlayOutput = Join-Path $script:ANDROID_ROOT "temp/public-first-play-$(Get-Date -Format 'yyyyMMdd-HHmmss-fff')"
    & "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $firstPlayOutput | Out-Null
    New-Item -ItemType Directory -Force -Path $firstPlayOutput | Out-Null
    $firstPlayRunIds = New-Object 'System.Collections.Generic.HashSet[string]'
    Write-Output "Public first-play evidence: $firstPlayOutput"
    try {
        $cleared = Adb -AdbArgs @('shell', 'pm', 'clear', $script:PACKAGE)
        if ($cleared -cne 'Success') { throw 'Could not clear disposable emulator app state' }
        # Only source files are staged; every active-set asset must pass through production import
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/saf_provider_source') | Out-Null
        foreach ($config in $configs) {
            Send-FirstPlaySource -Path (Join-Path $script:REPO_ROOT "game_data/demo installers/$($config.Archive)") `
                -Name $config.Archive -Sha256 $config.ArchiveSha256
        }
        foreach ($file in $configs[0].RequiredHashes) {
            Send-FirstPlaySource -Path (Join-Path $d1Loose $file.File) -Name $file.File -Sha256 $file.Sha256
        }
        # run_test suppresses graphics onboarding and clears saves; this tests cold content/pilots, not that dialog
        Invoke-FirstPlayPhase empty
        Assert-FirstPlayFiles -Expected @() -Label 'cold-empty'
        Import-FirstPlaySources -Names @($configs[0].Archive) -Kind archive -Label 'd1-archive' -Expected $configs[0].RequiredHashes
        Wait-FirstPlaySetup -Label 'd1-archive' -Predicate { param($s) Test-FirstPlayReadiness $s $true $false } | Out-Null
        Assert-FirstPlayFiles -Expected $configs[0].RequiredHashes -Label 'd1-archive'
        Invoke-FirstPlayPhase d1
        Invoke-FirstPlayInterruptedReplacement -SourceHog (Join-Path $d1Loose 'descent.hog') -BaseFiles $configs[0].RequiredHashes

        Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
            '--es', 'command', 'clear_set', '--es', 'name', 'default') | Out-Null
        Wait-FirstPlaySetup -Label 'loose-empty' -Predicate { param($s) Test-FirstPlayReadiness $s $false $false } | Out-Null
        Assert-FirstPlayFiles -Expected @() -Label 'loose-empty'
        Import-FirstPlaySources -Names @('descent.hog') -Kind loose -Label 'incomplete-hog' `
            -Expected @($configs[0].RequiredHashes | Where-Object File -CEQ 'descent.hog')
        Wait-FirstPlaySetup -Label 'incomplete-hog' -Predicate { param($s) Test-FirstPlayReadiness $s $false $false } | Out-Null
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes | Where-Object File -CEQ 'descent.hog') -Label 'incomplete-hog'
        foreach ($label in @('loose-d1', 'repeat-loose-d1')) {
            Import-FirstPlaySources -Names $configs[0].RequiredFiles -Kind loose -Label $label -Expected $configs[0].RequiredHashes
            Wait-FirstPlaySetup -Label $label -Predicate { param($s) Test-FirstPlayReadiness $s $true $false } | Out-Null
            Assert-FirstPlayFiles -Expected $configs[0].RequiredHashes -Label $label
        }
        Invoke-FirstPlayPhase d1_in_d2
        Import-FirstPlaySources -Names @($configs[1].Archive) -Kind archive -Label 'd2-archive' `
            -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes)
        Wait-FirstPlaySetup -Label 'both-demos' -Predicate { param($s) Test-FirstPlayReadiness $s $true $true } | Out-Null
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes) -Label 'both-demos'
        Invoke-FirstPlayPhase d2
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes) -Label 'final'
        # Keep the independent picker smoke after the priority import/engine evidence
        Test-FirstPlayDocumentsUiCancel -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes)
        $missionFixture = New-FirstPlayMission -SourceHog (Join-Path $d2Loose 'd2demo.hog') -OutputDirectory $firstPlayOutput
        try { Import-FirstPlayMission -Fixture $missionFixture } finally { Remove-Item -LiteralPath $missionFixture.Path -Force }
        Invoke-FirstPlayPhase mission
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes) -Label 'mission-cleanup'
        [ordered]@{
            result = 'PASS'; phases = @('empty', 'd1', 'interruption_old', 'interruption_old', 'interruption_new', 'd1_in_d2', 'd2', 'mission'); run_ids = @($firstPlayRunIds | Sort-Object)
            installers = @($configs | ForEach-Object { @{ name = $_.Archive; sha256 = $_.ArchiveSha256 } })
            picker = 'actual DocumentsUI open/cancel only'; imports = 'SafTestProvider production post-picker callback'
            cold_state = 'pm clear on verified disposable emulator'; graphics_onboarding = 'suppressed by existing run_test marker'
            save_preservation = 'Custom public mission normal menu save/restore and Load Last Save inside one phase; saves reset between phases'
            mission = 'Generated ZIP, unchanged D2 demo level with unique fpplay1.rl2 alias; internal level title remains Ahayweh Gate'
            mission_identity = 'Pinned source/member and imported ZIP hashes, unique descriptor/member mapping, active mod owner and saved mission name; no direct level-filename introspection'
            exit_save = 'enter_launcher uses the synchronous automation auto-exit save path; Load Last Save is a real launcher button'
            mission_return = 'Resumed custom mission -> normal Abort Game -> native menu -> built-in D2 demo in the same log-verified process; custom HOG detached and new-game ammunition reset'
            interrupted_replacement = 'Instrumented extraction attempt killed before publication; old public D1 mission remains playable, identical ordinary retry launches replacement'
            native_d1_identity = 'fresh MainActivity native-library load plus correlated level-one PASS; D1 has no asset_trace'
        } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $firstPlayOutput 'summary.json') -Encoding utf8
        Write-Output 'PASS: public import and button launches, generated mission selection, normal menu save/restore and launcher resume'
    } finally {
        Adb -AdbArgs @('logcat', '-d') | Set-Content (Join-Path $firstPlayOutput 'final-logcat.txt') -Encoding utf8
        Stop-AppAndWait
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-rf', 'files/saf_provider_source') | Out-Null
        Adb -AdbArgs @('shell', 'rm', '-f', '/sdcard/first-play-picker.xml') | Out-Null
        $generatedMission = Join-Path $firstPlayOutput 'fpplay.zip'
        if (Test-Path -LiteralPath $generatedMission) { Remove-Item -LiteralPath $generatedMission -Force }
    }
}
