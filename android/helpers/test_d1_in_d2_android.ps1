param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$D2DataDirectory,
    [switch]$GameLog,
    [string]$Serial = 'emulator-5554',
    [string]$AdbPath = 'C:\local\android-sdk\platform-tools\adb.exe',
    [string]$ApkPath,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
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

$testLabel = if ($D2DataDirectory) { 'D2-to-D1' } else { 'D1-only' }
$dataFiles = @('DESCENT.HOG', 'DESCENT.PIG') | ForEach-Object { Join-Path $D1DataDirectory $_ }
if ($D2DataDirectory) {
    $dataFiles += @('DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S22', 'GROUPA.PIG', 'ALIEN1.PIG', 'ALIEN2.PIG', 'FIRE.PIG', 'ICE.PIG', 'WATER.PIG') |
        ForEach-Object { Join-Path $D2DataDirectory $_ }
}
foreach ($dataFile in $dataFiles) {
    if (-not (Test-Path -LiteralPath $dataFile -PathType Leaf)) { throw "Missing $dataFile" }
}
$steps = Get-Content (Join-Path $repo 'android/game_scripts/test_d1_in_d2_standalone.jsonc') -Raw | ConvertFrom-Json
if ($D2DataDirectory) {
    $steps[1].game = 'd2'
    # Prove the initial D2 bank before selecting First Strike, then prove the
    # D1 bank is installed during its briefing, before level preparation
    $steps = @($steps[0..4]) + @(@{ action = 'assert'; expect = @{ 'asset_trace.mode' = 'd2' } }) + @($steps[5..8]) + @(
        @{ action = 'wait_for'; field = 'screen_advance_kind'; value = 'briefing'; timeout_ms = 15000 },
        @{ action = 'assert'; expect = @{ 'asset_trace.mode' = 'd1-in-d2'; 'asset_trace.d1_compat.robot_models' = 78 } }
    ) + @($steps[9..($steps.Count - 1)])
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
    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    do {
        Invoke-Device -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT') | Out-Null
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
    Invoke-Device -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_AUTOMATE', '--es', 'script', 'd1-in-d2-script.jsonc', '--es', 'run_id', $runId) | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $gameProcessId = ''
    do {
        Start-Sleep -Milliseconds 800
        $result = Read-AppJson 'automation_result.json'
        Save-SceneSnapshot
        if ($result -and $result.run_id -eq $runId -and $result.result -in @('PASS', 'FAIL')) { break }
        if (-not $gameProcessId) {
            $candidate = Invoke-Device -Arguments @('shell', 'pidof', "$package`:game") -AllowFailure
            if ($candidate -match '^\d+$') { $gameProcessId = $candidate.Trim() }
        }
        $crashes = Invoke-Device -Arguments @('shell', 'run-as', $package, 'ls', 'files/tombstones') -AllowFailure
        if ($gameProcessId -and $crashes -match "crash_error_$gameProcessId\.txt|crash_signal_.*_$gameProcessId\.txt") {
            throw 'Native error during isolated launch; see captured tombstones and logcat'
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    if (-not $result -or $result.run_id -ne $runId -or $result.result -ne 'PASS') {
        throw "$testLabel automation failed or timed out: $($result | ConvertTo-Json -Depth 10 -Compress)"
    }
    if ($GameLog) {
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
    Write-Output "$testLabel Android First Strike interaction and level-transition checks passed"
} finally {
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
