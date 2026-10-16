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
    param([string]$Text)
    if (-not (Send-TapButton -Serial $Serial -Text $Text -TimeoutSec 30 -PollMs 500)) {
        throw "Could not tap first-play button: $Text"
    }
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

function Import-FirstPlaySources {
    param([string[]]$Names, [ValidateSet('archive', 'loose')][string]$Kind, [string]$Label)
    # This is the production callback after a picker result, not DocumentsUI selection
    $uris = @($Names | ForEach-Object { "content://$($script:PACKAGE).saf-test/seekable/$_" }) -join ','
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
        '--es', 'command', 'import_picked_uris', '--esa', 'uris', $uris) | Out-Null
    $button = if ($Kind -eq 'archive') { 'Import to Current Set' } else { 'Import All' }
    if (-not (Wait-SetupCondition -ButtonsOnly -TimeoutSeconds 120 -PollMs 500 -Predicate {
                param($s) @($s.buttons | Where-Object { $_.text -ceq $button -and $_.enabled }).Count -eq 1
            })) { throw "$Label did not expose the production import confirmation" }
    Tap-FirstPlayButton $button
    if (-not (Wait-SetupCondition -ButtonsOnly -TimeoutSeconds 90 -PollMs 500 -Predicate {
                param($s) @($s.buttons | Where-Object text -CEQ $button).Count -eq 0 -and
                @($s.buttons | Where-Object { $_.text -like '*Select Game Files or Archive to Import*' -and $_.enabled }).Count -eq 1
            })) { throw "$Label did not finish publishing its selected files" }
    $importLog = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-Setup:I', '*:S')
    $importLog | Set-Content (Join-Path $firstPlayOutput "$Label-import.log") -Encoding utf8
    if ($Kind -eq 'loose') {
        # Tap helper cleared logcat immediately before confirmation; old files cannot prove a successful repeat
        foreach ($name in $Names) {
            $escaped = [regex]::Escape($name)
            if ($importLog -notmatch "Imported $escaped -> $escaped \([0-9]+ bytes\)") {
                throw "$Label lacks a fresh production copy completion for $name"
            }
        }
    }
    [ordered]@{ kind = $Kind; files = $Names; callback = 'import_picked_uris'; provider = 'SafTestProvider'; confirmation = $button } |
        ConvertTo-Json -Depth 5 | Set-Content (Join-Path $firstPlayOutput "$Label-import.json") -Encoding utf8
}

function Test-FirstPlayDocumentsUiCancel {
    Tap-FirstPlayButton 'Select Game Files or Archive to Import'
    Tap-FirstPlayButton 'Pick One or More Files'
    $dump = '/sdcard/first-play-picker.xml'
    $opened = Wait-ForCondition -Description 'system DocumentsUI opened' -TimeoutSec 30 -PollMs 500 -Condition {
        Adb -AdbArgs @('shell', 'uiautomator', 'dump', $dump) -Seconds 10 | Out-Null
        $xml = Adb -AdbArgs @('shell', 'cat', $dump)
        $xml | Set-Content (Join-Path $firstPlayOutput 'documentsui-open.xml') -Encoding utf8
        $xml -match 'package="com\.(?:google\.)?android\.documentsui"'
    }
    if (-not $opened) { throw 'Import chooser did not open the system DocumentsUI' }
    Adb -AdbArgs @('shell', 'input', 'keyevent', 'KEYCODE_BACK') | Out-Null
    $returned = Wait-ForCondition -Description 'DocumentsUI cancellation returned to launcher' -TimeoutSec 30 -PollMs 500 -Condition {
        Adb -AdbArgs @('shell', 'uiautomator', 'dump', $dump) -Seconds 10 | Out-Null
        $xml = Adb -AdbArgs @('shell', 'cat', $dump)
        $xml | Set-Content (Join-Path $firstPlayOutput 'documentsui-cancel.xml') -Encoding utf8
        $xml -match ('package="' + [regex]::Escape($script:PACKAGE) + '"') -and
            $xml -notmatch 'package="com\.(?:google\.)?android\.documentsui"'
    }
    if (-not $returned) { throw 'Canceling DocumentsUI did not return to the launcher' }
    Wait-FirstPlaySetup -Label 'picker-cancel' -Predicate { param($s) Test-FirstPlayReadiness $s $false $false } | Out-Null
    Assert-FirstPlayFiles -Expected @() -Label 'picker-cancel'
    Write-Output 'PASS: actual DocumentsUI open/cancel; installer selection is separately injected through SafTestProvider'
}

function Invoke-FirstPlayPhase {
    param([ValidateSet('empty', 'd1', 'd1_in_d2', 'd2')][string]$Phase)
    $name = "test_public_first_play_$Phase.jsonc"
    $phaseOutput = Join-Path $firstPlayOutput $Phase
    New-Item -ItemType Directory -Force -Path $phaseOutput | Out-Null
    $pwsh = Get-RegressionCurrentPwshPath
    # No Params or edition override: only these exact reviewed repository scripts bypass data recovery seeding
    & $pwsh -NoProfile -NonInteractive -File "$PSScriptRoot/../helpers/run_test.ps1" `
        -ScriptName $name -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 180 2>&1 |
        Tee-Object -FilePath (Join-Path $phaseOutput 'runner.log') | Out-Host
    $exitCode = $LASTEXITCODE
    foreach ($file in @('automation_result.json', 'automation_log.jsonl', 'introspect.json', 'setup_introspect.json')) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "files/$file") |
            Set-Content (Join-Path $phaseOutput $file) -Encoding utf8
    }
    # run_test force-stops the previous process, then clears logcat before this phase
    $nativeLog = Adb -AdbArgs @('logcat', '-d', '-s', 'MainActivity:I', '*:S')
    $nativeLog | Set-Content (Join-Path $phaseOutput 'native-library.log') -Encoding utf8
    if ($exitCode -ne 0) { throw "First-play $Phase automation failed (exit $exitCode)" }
    $result = Get-Content (Join-Path $phaseOutput 'automation_result.json') -Raw | ConvertFrom-Json
    if (-not (Test-AutomationResultComplete -Result $result) -or $result.run_id -notmatch '^[a-f0-9]{32}$' -or
        -not $firstPlayRunIds.Add([string]$result.run_id)) { throw "First-play $Phase lacks a fresh complete durable PASS" }
    $runnerLog = Get-Content (Join-Path $phaseOutput 'runner.log') -Raw
    $expectedPushName = "test_public_first_play_$Phase.run-$($result.run_id).jsonc"
    if (-not $runnerLog.Contains($expectedPushName)) { throw "First-play $Phase result is not correlated to its staged script" }
    if ($Phase -eq 'empty') {
        if ($nativeLog -match 'Loaded native library:') { throw 'An engine started despite missing game files' }
    } else {
        # D1 has no asset_trace.runtime_engine field, so require the actual native-library load too
        $engine = if ($Phase -eq 'd1') { 'd1' } else { 'd2' }
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
        Test-FirstPlayDocumentsUiCancel
        Import-FirstPlaySources -Names @($configs[0].Archive) -Kind archive -Label 'd1-archive'
        Wait-FirstPlaySetup -Label 'd1-archive' -Predicate { param($s) Test-FirstPlayReadiness $s $true $false } | Out-Null
        Assert-FirstPlayFiles -Expected $configs[0].RequiredHashes -Label 'd1-archive'
        Invoke-FirstPlayPhase d1

        Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
            '--es', 'command', 'clear_set', '--es', 'name', 'default') | Out-Null
        Wait-FirstPlaySetup -Label 'loose-empty' -Predicate { param($s) Test-FirstPlayReadiness $s $false $false } | Out-Null
        Assert-FirstPlayFiles -Expected @() -Label 'loose-empty'
        Import-FirstPlaySources -Names @('descent.hog') -Kind loose -Label 'incomplete-hog'
        Wait-FirstPlaySetup -Label 'incomplete-hog' -Predicate { param($s) Test-FirstPlayReadiness $s $false $false } | Out-Null
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes | Where-Object File -CEQ 'descent.hog') -Label 'incomplete-hog'
        foreach ($label in @('loose-d1', 'repeat-loose-d1')) {
            Import-FirstPlaySources -Names $configs[0].RequiredFiles -Kind loose -Label $label
            Wait-FirstPlaySetup -Label $label -Predicate { param($s) Test-FirstPlayReadiness $s $true $false } | Out-Null
            Assert-FirstPlayFiles -Expected $configs[0].RequiredHashes -Label $label
        }
        Invoke-FirstPlayPhase d1_in_d2
        Import-FirstPlaySources -Names @($configs[1].Archive) -Kind archive -Label 'd2-archive'
        Wait-FirstPlaySetup -Label 'both-demos' -Predicate { param($s) Test-FirstPlayReadiness $s $true $true } | Out-Null
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes) -Label 'both-demos'
        Invoke-FirstPlayPhase d2
        Assert-FirstPlayFiles -Expected @($configs[0].RequiredHashes + $configs[1].RequiredHashes) -Label 'final'
        [ordered]@{
            result = 'PASS'; phases = @('empty', 'd1', 'd1_in_d2', 'd2'); run_ids = @($firstPlayRunIds | Sort-Object)
            installers = @($configs | ForEach-Object { @{ name = $_.Archive; sha256 = $_.ArchiveSha256 } })
            picker = 'actual DocumentsUI open/cancel only'; imports = 'SafTestProvider production post-picker callback'
            cold_state = 'pm clear on verified disposable emulator'; graphics_onboarding = 'suppressed by existing run_test marker'
            save_preservation = 'not tested; run_test clears saves between phases'
            native_d1_identity = 'fresh MainActivity native-library load plus correlated level-one PASS; D1 has no asset_trace'
        } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $firstPlayOutput 'summary.json') -Encoding utf8
        Write-Output 'PASS: cold public import, incomplete recovery, repeated multi-file import, D1/D1-in-D2/D2 button launches to level 1'
    } finally {
        Adb -AdbArgs @('logcat', '-d') | Set-Content (Join-Path $firstPlayOutput 'final-logcat.txt') -Encoding utf8
        Stop-AppAndWait
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-rf', 'files/saf_provider_source') | Out-Null
        Adb -AdbArgs @('shell', 'rm', '-f', '/sdcard/first-play-picker.xml') | Out-Null
    }
}
