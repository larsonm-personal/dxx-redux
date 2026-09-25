param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$D2DataDirectory,
    [switch]$GameLog,
    [switch]$SoundCheck,
    [switch]$WeaponArt,
    [switch]$Guidebot,
    [switch]$NativeD1,
    [string]$WeaponArtReference,
    [string]$Serial = 'emulator-5554',
    [string]$AdbPath = 'C:\local\android-sdk\platform-tools\adb.exe',
    [string]$ApkPath,
    [int]$LauncherTimeoutSeconds = 60,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if ($Guidebot -and (-not $D2DataDirectory -or $NativeD1 -or $WeaponArt -or $SoundCheck)) { throw 'Guidebot requires D2 assets and its own imported-D1 run' }
if ($NativeD1 -and (-not $WeaponArt -or $D2DataDirectory)) { throw 'NativeD1 requires WeaponArt and D1-only data' }
if ($WeaponArt -and $SoundCheck) { throw 'Run weapon rendering and sound checks separately' }
if ($WeaponArt -and -not $NativeD1 -and -not $WeaponArtReference) { throw 'Imported weapon art requires a native-D1 WeaponArtReference directory' }
if ($SoundCheck) { $GameLog = $true }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$outputDirectory = Join-Path $repo ('temp/d1-launch-runtime-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts $outputDirectory
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Write-Output "Runtime evidence: $outputDirectory"
$package = 'com.dxxredux.app'
$backup = '.d1-in-d2-check-backup'
$moved = @()
$created = @()
$runId = [guid]::NewGuid().ToString('N')
$script:lastSceneSnapshot = ''
$script:sceneNumber = 0

function Invoke-Device {
    param([string[]]$Arguments, [switch]$AllowFailure)
    $result = & $AdbPath -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0 -and -not $AllowFailure) { throw "adb failed: $($Arguments -join ' '): $result" }
    return ($result -join "`n")
}

function Read-AppJson {
    param([string]$Name)
    $raw = Invoke-Device -Arguments @('shell', 'run-as', $package, 'cat', "files/$Name") -AllowFailure
    try { return $raw | ConvertFrom-Json } catch { return $null }
}

function Save-SceneSnapshot {
    $raw = Invoke-Device -Arguments @('shell', 'run-as', $package, 'cat', 'files/introspect.json') -AllowFailure
    if ($raw -eq $script:lastSceneSnapshot) { return }
    try { $null = $raw | ConvertFrom-Json } catch { return }
    $script:lastSceneSnapshot = $raw
    # adb capture is asynchronous with the script; do not pair an earlier state
    # with an image after the next introspection milestone has already arrived
    $pendingImage = Join-Path $outputDirectory 'scene-pending.png'
    & $AdbPath -s $Serial exec-out screencap -p > $pendingImage
    if ($LASTEXITCODE -ne 0) { throw 'Scene screenshot capture failed' }
    $afterCapture = Invoke-Device -Arguments @('shell', 'run-as', $package, 'cat', 'files/introspect.json') -AllowFailure
    if ($afterCapture -ne $raw) {
        Remove-Item -LiteralPath $pendingImage
        return
    }
    $script:sceneNumber++
    $stem = Join-Path $outputDirectory ('scene-{0:D2}' -f $script:sceneNumber)
    $raw | Set-Content "$stem.json" -Encoding utf8
    Move-Item -LiteralPath $pendingImage -Destination "$stem.png"
}

function Save-WeaponArt {
    $gameDirectory = if ($NativeD1) { 'd1x-redux' } else { 'd2x-redux' }
    & $AdbPath -s $Serial shell run-as $package test -d "files/$gameDirectory/weapon-art"
    if ($LASTEXITCODE -ne 0) { return }
    $archive = Join-Path $outputDirectory 'weapon-art.tar'
    & $AdbPath -s $Serial exec-out run-as $package tar -cf - -C "files/$gameDirectory/weapon-art" . 2> (Join-Path $outputDirectory 'weapon-art-error.txt') > $archive
    if ($LASTEXITCODE -ne 0) { return }
    $destination = Join-Path $outputDirectory 'weapon-art'
    New-Item -ItemType Directory -Force $destination | Out-Null
    & tar -xf $archive -C $destination
    if ($LASTEXITCODE -ne 0) { throw 'Cannot unpack weapon rendering evidence' }
}

function Assert-WeaponArt {
    $actualDirectory = Join-Path $outputDirectory 'weapon-art'
    $actual = Get-Content (Join-Path $actualDirectory 'weapon-art.json') -Raw | ConvertFrom-Json
    $engine = if ($NativeD1) { 'd1' } else { 'd2' }
    if ($actual.engine -ne $engine -or ($actual.frames.frame -join ',') -cne 'first,restored,rewound,second') {
        throw 'Incomplete weapon rendering report or incorrect capture engine'
    }
    foreach ($extension in @('indexed', 'palette', 'ppm')) {
        $before = (Get-FileHash (Join-Path $actualDirectory "first.$extension") -Algorithm SHA256).Hash
        foreach ($restored in @('restored', 'rewound')) {
            $after = (Get-FileHash (Join-Path $actualDirectory "$restored.$extension") -Algorithm SHA256).Hash
            if ($before -cne $after) { throw "$restored changed Spreadfire $extension" }
        }
    }
    if (-not $NativeD1) {
        $reference = Get-Content (Join-Path $WeaponArtReference 'weapon-art.json') -Raw | ConvertFrom-Json
        if ($reference.engine -ne 'd1') { throw 'Weapon reference is not native D1' }
        if (($reference.frames | ConvertTo-Json -Depth 40 -Compress) -cne ($actual.frames | ConvertTo-Json -Depth 40 -Compress)) {
            throw 'Android Spreadfire state or asset identity differs from native D1'
        }
        foreach ($frame in $actual.frames) {
            foreach ($extension in @('indexed', 'palette', 'ppm')) {
                $name = "$($frame.frame).$extension"
                $expected = (Get-FileHash (Join-Path $WeaponArtReference $name) -Algorithm SHA256).Hash
                $observed = (Get-FileHash (Join-Path $actualDirectory $name) -Algorithm SHA256).Hash
                if ($expected -cne $observed) { throw "Android Spreadfire differs from native D1: $name" }
            }
        }
    }
    Write-Output "PASS: $engine Android Spreadfire firing, live checkpoint restore and GPU pixels$(if (-not $NativeD1) { ' match native D1' })"
}

$testLabel = if ($D2DataDirectory) { 'D2-to-D1' } else { 'D1-only' }
$dataFiles = @('DESCENT.HOG', 'DESCENT.PIG') | ForEach-Object { Join-Path $D1DataDirectory $_ }
if ($D2DataDirectory) {
    $dataFiles += @('DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S22', 'GROUPA.PIG', 'ALIEN1.PIG', 'ALIEN2.PIG', 'FIRE.PIG', 'ICE.PIG', 'WATER.PIG') |
        ForEach-Object { Join-Path $D2DataDirectory $_ }
}
foreach ($dataFile in $dataFiles) {
    if (-not (Test-Path -LiteralPath $dataFile -PathType Leaf)) { throw "Missing $dataFile" }
}
$steps = @(Get-Content (Join-Path $repo 'android/game_scripts/test_d1_in_d2_standalone.jsonc') -Raw | ConvertFrom-Json |
        Where-Object { -not $_._info })
if ($SoundCheck) {
    # Reuse cold startup and real laser firing, ending before the travel scenario
    $last = -1
    for ($i = 0; $i -lt $steps.Count; $i++) {
        if ($steps[$i].action -eq 'assert' -and $steps[$i].expect -and
            $steps[$i].expect.PSObject.Properties.Name -contains 'player.energy' -and
            $steps[$i].expect.PSObject.Properties.Name -contains 'player_dead') { $last = $i; break }
    }
    if ($last -lt 0) { throw 'Sound scenario cannot locate the completed laser-firing assertion' }
    $steps = @($steps[0..$last])
}
if ($D2DataDirectory -and -not $WeaponArt -and -not $Guidebot) {
    $steps[1].game = 'd2'
    # Registered D2 contributes four independently owned companion models
    # The source/publication integration fixture verifies the 78 original models
    foreach ($step in $steps) {
        if ($step.action -eq 'assert' -and $step.expect.PSObject.Properties.Name -contains 'asset_trace.d1_compat.robot_models') {
            $step.expect.'asset_trace.d1_compat.robot_models' = 82
        }
    }
    # Prove the initial D2 bank before selecting First Strike, then prove the
    # D1 bank is installed during its briefing, before level preparation
    $steps = @($steps[0..4]) + @(@{ action = 'assert'; expect = @{ 'asset_trace.mode' = 'd2' } }) + @($steps[5..8]) + @(
        @{ action = 'wait_for'; field = 'screen_advance_kind'; value = 'briefing'; timeout_ms = 15000 },
        @{ action = 'assert'; expect = @{ 'asset_trace.mode' = 'd1-in-d2'; 'asset_trace.d1_compat.robot_models' = 82 } }
    ) + @($steps[9..($steps.Count - 1)])
}
if ($WeaponArt) {
    $steps = @($steps[0..11])
    # Use the same normal difficulty and loaded mine in both engines
    $steps[8].text = 'Hotshot'
    if ($NativeD1) {
        $steps[1].game = 'd1'
        # Native D1 confirms the selected level input with Enter; it has no Ok item
        $steps[7] = @{ action = 'key'; key = 'enter'; post_delay_ms = 100 }
    } elseif ($D2DataDirectory) { $steps[1].game = 'd2' }
    $steps += @(Get-Content (Join-Path $repo 'android/game_scripts/test_d1_weapon_art.jsonc') -Raw | ConvertFrom-Json |
            Where-Object { -not $_._info })
}
if ($Guidebot) {
    $steps = @($steps[0..11]) + @(Get-Content (Join-Path $repo 'android/game_scripts/test_d1_optional_guidebot.jsonc') -Raw | ConvertFrom-Json |
            Where-Object { -not $_._info })
}
$scriptFile = Join-Path $outputDirectory 'script.json'
$steps | ConvertTo-Json -Depth 40 | Set-Content $scriptFile -Encoding utf8
Invoke-Device -Arguments @('get-state') | Out-Null
if ($ApkPath) { Invoke-Device -Arguments @('install', '-r', '-t', $ApkPath) | Out-Null }
Invoke-Device -Arguments @('shell', 'am', 'force-stop', $package) | Out-Null
Invoke-Device -Arguments @('shell', 'run-as', $package, 'mkdir', $backup) | Out-Null
try {
    # Preserve the original installation; fresh files/preferences make hidden
    # game-directory, mod and external-import fallbacks unavailable to this run
    foreach ($name in @('files', 'shared_prefs')) {
        & $AdbPath -s $Serial shell run-as $package test -d $name
        if ($LASTEXITCODE -eq 0) {
            Invoke-Device -Arguments @('shell', 'run-as', $package, 'mv', $name, "$backup/$name") | Out-Null
            $moved += $name
        }
        Invoke-Device -Arguments @('shell', 'run-as', $package, 'mkdir', $name) | Out-Null
        $created += $name
    }
    Invoke-Device -Arguments @('shell', 'run-as', $package, 'mkdir', '-p', 'files/imported/sets/default') | Out-Null
    foreach ($dataFile in $dataFiles) {
        $name = Split-Path $dataFile -Leaf
        $deviceFile = "/data/local/tmp/d1-in-d2-$($name.ToLowerInvariant())"
        Invoke-Device -Arguments @('push', $dataFile, $deviceFile) | Out-Null
        Invoke-Device -Arguments @('shell', 'run-as', $package, 'cp', $deviceFile, "files/imported/sets/default/$($name.ToLowerInvariant())") | Out-Null
        Invoke-Device -Arguments @('shell', 'rm', $deviceFile) | Out-Null
    }
    if ($GameLog) {
        # DebugLogCategory.prefKey(GAME) in the fresh installation only
        $preferences = Join-Path $outputDirectory 'dxx_prefs.xml'
        '<?xml version="1.0" encoding="utf-8"?><map><boolean name="dlog_game logs_enabled" value="true" /></map>' |
            Set-Content $preferences -Encoding utf8
        Invoke-Device -Arguments @('push', $preferences, '/data/local/tmp/d1-in-d2-prefs.xml') | Out-Null
        Invoke-Device -Arguments @('shell', 'run-as', $package, 'cp', '/data/local/tmp/d1-in-d2-prefs.xml', 'shared_prefs/dxx_prefs.xml') | Out-Null
        Invoke-Device -Arguments @('shell', 'rm', '/data/local/tmp/d1-in-d2-prefs.xml') | Out-Null
    }
    Invoke-Device -Arguments @('push', $scriptFile, '/data/local/tmp/d1-in-d2-script.jsonc') | Out-Null
    Invoke-Device -Arguments @('shell', 'run-as', $package, 'cp', '/data/local/tmp/d1-in-d2-script.jsonc', 'files/d1-in-d2-script.jsonc') | Out-Null
    Invoke-Device -Arguments @('logcat', '-c') | Out-Null
    Invoke-Device -Arguments @('shell', 'am', 'start', '-n', "$package/.SetupActivity") | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds($LauncherTimeoutSeconds)
    $nextIntrospection = [DateTime]::MinValue
    do {
        # Read the durable result below; a synchronous broadcast can outlive
        # this deadline while the freshly installed launcher verifies classes
        # Limit queued requests while the main thread is still starting
        if ([DateTime]::UtcNow -ge $nextIntrospection) {
            Invoke-Device -Arguments @('shell', 'am', 'broadcast', '--async', '-a', 'com.dxxredux.SETUP_INTROSPECT') | Out-Null
            $nextIntrospection = [DateTime]::UtcNow.AddSeconds(10)
        }
        Start-Sleep -Milliseconds 700
        $setup = Read-AppJson 'setup_introspect.json'
    } while (-not $setup -and [DateTime]::UtcNow -lt $deadline)
    if (-not $setup) { throw 'Launcher did not produce introspection' }
    if ($GameLog -and -not $setup.debug_prefs.game_log_enabled) { throw 'Game logging was not enabled in the isolated installation' }
    $setup | ConvertTo-Json -Depth 40 | Set-Content (Join-Path $outputDirectory 'setup.json') -Encoding utf8
    if (-not ($setup.launch_targets | Where-Object { $_.id -eq 'd1-in-d2' -and $_.engine -eq 'd2' })) {
        throw 'Installed APK does not expose the D1-in-D2 launch target; supply the current Gradle artifact with -ApkPath'
    }
    if (-not $setup.d1.ready -or $setup.d2.ready -ne [bool]$D2DataDirectory -or -not $setup.d1_in_d2.ready) { throw "Incorrect $testLabel launcher readiness" }
    # Dispatch once, then bound the wait by the durable result and run identity
    Invoke-Device -Arguments @('shell', 'am', 'broadcast', '--async', '-a', 'com.dxxredux.SETUP_AUTOMATE', '--es', 'script', 'd1-in-d2-script.jsonc', '--es', 'run_id', $runId) | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $gameProcessId = ''
    do {
        Start-Sleep -Milliseconds 800
        $result = Read-AppJson 'automation_result.json'
        Save-SceneSnapshot
        if ($result -and $result.run_id -eq $runId -and $result.result -in @('PASS', 'FAIL')) { break }
        $candidate = Invoke-Device -Arguments @('shell', 'pidof', "$package`:game") -AllowFailure
        if ($gameProcessId -and (-not $candidate.Trim() -or ($candidate -match '^\d+$' -and $candidate.Trim() -ne $gameProcessId))) {
            throw "Game process $gameProcessId exited before the matching automation result; see captured logcat"
        }
        if (-not $gameProcessId -and $candidate -match '^\d+$') { $gameProcessId = $candidate.Trim() }
        $crashes = Invoke-Device -Arguments @('shell', 'run-as', $package, 'ls', 'files/tombstones') -AllowFailure
        if ($gameProcessId -and $crashes -match "crash_error_$gameProcessId\.txt|crash_signal_.*_$gameProcessId\.txt") {
            throw 'Native error during isolated launch; see captured tombstones and logcat'
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    if (-not $result -or $result.run_id -ne $runId -or $result.result -ne 'PASS') {
        throw "$testLabel automation failed or timed out: $($result | ConvertTo-Json -Depth 10 -Compress)"
    }
    if ($WeaponArt) {
        Save-WeaponArt
        Assert-WeaponArt
    } elseif ($SoundCheck) {
        $trace = Invoke-Device -Arguments @('logcat', '-d', '-s', 'DXX-DLOG')
        $samples = [regex]::Matches($trace, '\[SFX\] context=[1-9]\d* sample=(?<sample>\d+) cache_input_hash=\w+ cache_input_bytes=(?<input>\d+).*? output_bytes=(?<output>\d+) source_rate=(?<source>\d+) cached_rate=(?<cached>\d+) output_rate=(?<rate>\d+) output_format=(?<format>\d+) channels=(?<channels>\d+)')
        $checked = 0
        $laser = $false
        foreach ($sample in $samples) {
            if ([int]$sample.Groups['sample'].Value -ge 98) { continue }
            if ($sample.Groups['source'].Value -ne '11025' -or $sample.Groups['cached'].Value -ne '11025') {
                throw "Original D1 sample converted at the wrong rate: $($sample.Value)"
            }
            $frames = [Math]::Max(1, [Math]::Floor(([long]$sample.Groups['input'].Value * [long]$sample.Groups['rate'].Value + 5512) / 11025))
            $bytesPerFrame = ([int]$sample.Groups['format'].Value -band 255) / 8 * [int]$sample.Groups['channels'].Value
            if ([long]$sample.Groups['output'].Value -ne $frames * $bytesPerFrame) {
                throw "Original D1 sample duration changed at the device output rate: $($sample.Value)"
            }
            $checked++
            $laser = $laser -or $sample.Groups['sample'].Value -eq '0'
        }
        if (-not $checked -or -not $laser) { throw 'Missing converted original laser sound evidence' }
        Write-Output "PASS: $checked original D1 sample conversions retain source rate and duration, including laser playback"
    } elseif ($GameLog) {
        $trace = Invoke-Device -Arguments @('logcat', '-d', '-s', 'DXX-DLOG')
        $samples = [regex]::Matches($trace, '\[FLYOUT\].*? seg=(?<segment>\d+) located=(?<located>[01]) exit=(?<exit>\d+)')
        if (-not $samples.Count) { throw 'Requested flyout diagnostics were not captured' }
        foreach ($sample in $samples) {
            if ($sample.Groups['located'].Value -eq '0' -and $sample.Groups['segment'].Value -ne $sample.Groups['exit'].Value) {
                throw "Flyout left the tunnel before reaching its exit: $($sample.Value)"
            }
        }
    }
    & $AdbPath -s $Serial exec-out screencap -p > (Join-Path $outputDirectory 'first-strike.png')
    if ($WeaponArt) { Write-Output "Android weapon rendering evidence: $outputDirectory/weapon-art" }
    elseif ($SoundCheck) { Write-Output "$testLabel Android First Strike sound conversion checks passed" }
    elseif ($Guidebot) { Write-Output 'PASS: Android optional Guide-Bot cold deploy, save/restore and D1/D2/D1 lifecycle' }
    else { Write-Output "$testLabel Android First Strike interaction and level-transition checks passed" }
} finally {
    if ($WeaponArt) {
        try { Save-WeaponArt } catch { Write-Warning "Could not preserve weapon art: $_" }
    }
    $runningGame = Invoke-Device -Arguments @('shell', 'pidof', "$package`:game") -AllowFailure
    if ($runningGame -match '^\d+$') {
        Invoke-Device -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT') -AllowFailure | Out-Null
        Start-Sleep -Milliseconds 500
    }
    foreach ($name in @('automation_result.json', 'automation_log.jsonl', 'introspect.json')) {
        Invoke-Device -Arguments @('shell', 'run-as', $package, 'cat', "files/$name") -AllowFailure |
            Set-Content (Join-Path $outputDirectory $name) -Encoding utf8
    }
    Invoke-Device -Arguments @('logcat', '-d') -AllowFailure | Set-Content (Join-Path $outputDirectory 'logcat.txt') -Encoding utf8
    # Capture the complete crash directory before discarding the isolated data
    & $AdbPath -s $Serial exec-out run-as $package tar -cf - files/tombstones 2> (Join-Path $outputDirectory 'tombstones-error.txt') > (Join-Path $outputDirectory 'tombstones.tar')
    Invoke-Device -Arguments @('shell', 'am', 'force-stop', $package) | Out-Null
    foreach ($name in @('shared_prefs', 'files')) {
        # These exact directories were created above; original data is in backup
        if ($created -contains $name) { Invoke-Device -Arguments @('shell', 'run-as', $package, 'rm', '-rf', $name) | Out-Null }
        if ($moved -contains $name) { Invoke-Device -Arguments @('shell', 'run-as', $package, 'mv', "$backup/$name", $name) | Out-Null }
    }
    Invoke-Device -Arguments @('shell', 'run-as', $package, 'rmdir', $backup) | Out-Null
    Invoke-Device -Arguments @('shell', 'am', 'start', '-n', "$package/.SetupActivity") | Out-Null
}
