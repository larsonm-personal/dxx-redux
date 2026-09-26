param(
    [Parameter(Mandatory)][string]$DataDirectory,
    [ValidateSet('d1', 'd2')][string]$Game = 'd1',
    [ValidateSet('d1', 'd2')][string]$ContentGame,
    [string]$Serial = 'emulator-5554',
    [string]$AdbPath = 'C:/local/android-sdk/platform-tools/adb.exe',
    [string]$OutputDirectory,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Use a test emulator' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo "temp/android-metadata-worker-$Game" }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $OutputDirectory
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$package = 'com.dxxredux.app'
$backup = '.metadata-worker-check-backup'
$moved = @()
$created = @()
$observed = [Collections.Generic.List[object]]::new()
if (-not $ContentGame) { $ContentGame = $Game }
$level = if ($ContentGame -eq 'd1') { 'level01.rdl' } else { 'd2leva-1.rl2' }
$service = if ($Game -eq 'd1') { 'LevelMetadataD1AnalysisService' } else { 'LevelMetadataD2AnalysisService' }
$dataPath = "/data/user/0/$package/files/imported/sets/default"

function Device([string[]]$Arguments) {
    $result = & $AdbPath -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $result" }
    return ($result -join "`n")
}

function WorkerPid {
    $result = & $AdbPath -s $Serial shell pidof "$package`:levelmeta_$Game" 2>$null
    if ($LASTEXITCODE -eq 0) { return ($result -join '').Trim() }
    return ''
}

function Request([string]$Name, [string]$Data, [string]$LevelFile) {
    $requestId = "metadata-$Name-$([guid]::NewGuid().ToString('N'))"
    $work = "/data/user/0/$package/cache/level_metadata/$requestId"
    $request = [ordered]@{
        schema = 'dxx-level-metadata-request-v1'; request_id = $requestId; game = $Game
        source_name = $Name; source_type = 'active_level'; data_dir = $Data
        content_game = $ContentGame; mission_name = $(if ($ContentGame -eq 'd1' -and $Game -eq 'd2') { 'descent' } else { '' })
        level_file = $LevelFile; level_num = 1
        result_path = "$work/result.json"; checkpoint_path = "$work/checkpoint.json"
        priority = 'active'; cpu_duty_percent = 100
    }
    $requestFile = Join-Path $OutputDirectory "$Name-request.json"
    $request | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $requestFile -Encoding utf8
    Device @('shell', 'run-as', $package, 'mkdir', '-p', $work) | Out-Null
    Device @('push', $requestFile, '/data/local/tmp/metadata-worker-request.json') | Out-Null
    Device @('shell', 'run-as', $package, 'cp', '/data/local/tmp/metadata-worker-request.json', "$work/request.json") | Out-Null
    Device @('shell', 'run-as', $package, 'am', 'startservice', '--user', '0', '-n', "$package/.$service", '--es', 'request_path', "$work/request.json") | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 200
        $raw = & $AdbPath -s $Serial shell run-as $package cat "$work/result.json" 2>$null
        if ($LASTEXITCODE -eq 0) { break }
        $raw = $null
    } while ([DateTime]::UtcNow -lt $deadline)
    if (-not $raw) { throw "$Name produced no result" }
    $raw | Set-Content -LiteralPath (Join-Path $OutputDirectory "$Name-result.json") -Encoding utf8
    $result = ($raw -join "`n") | ConvertFrom-Json
    if ($result.request_id -cne $requestId) { throw 'Wrong result identity' }
    $observed.Add([ordered]@{ request = $Name; request_id = $requestId; status = $result.status; worker_pid = (WorkerPid) })
    return $result
}

Device @('shell', 'am', 'force-stop', $package) | Out-Null
Device @('shell', 'run-as', $package, 'mkdir', $backup) | Out-Null
try {
    # Preserve all launcher data and isolate the worker from earlier mount publications
    foreach ($name in @('files', 'shared_prefs', 'cache')) {
        & $AdbPath -s $Serial shell run-as $package test -d $name
        if ($LASTEXITCODE -eq 0) {
            Device @('shell', 'run-as', $package, 'mv', $name, "$backup/$name") | Out-Null
            $moved += $name
        }
        Device @('shell', 'run-as', $package, 'mkdir', $name) | Out-Null
        $created += $name
    }
    # Start before staging to keep background discovery separate from these service requests
    Device @('shell', 'am', 'start', '-W', '-n', "$package/.SetupActivity") | Out-Null
    Device @('shell', 'run-as', $package, 'mkdir', '-p', $dataPath, 'files/empty') | Out-Null
    $dataFiles = Get-ChildItem -LiteralPath $DataDirectory -File | Where-Object {
        if ($ContentGame -eq 'd1') { $_.Name -in @('descent.hog', 'descent.pig') }
        else { $_.Name -in @('descent2.hog', 'descent2.ham', 'descent2.s11', 'descent2.s22', 'groupa.pig') }
    }
    if ($dataFiles.Count -lt 2) { throw 'Missing base game fixture files' }
    foreach ($file in $dataFiles) {
        $deviceFile = '/data/local/tmp/metadata-worker-' + $file.Name.ToLowerInvariant()
        Device @('push', $file.FullName, $deviceFile) | Out-Null
        Device @('shell', 'run-as', $package, 'cp', $deviceFile, "$dataPath/$($file.Name.ToLowerInvariant())") | Out-Null
        Device @('shell', 'rm', $deviceFile) | Out-Null
    }
    Device @('logcat', '-c') | Out-Null
    $failed = Request 'missing-base' "/data/user/0/$package/files/empty" $level
    $expectedProblem = if ($Game -eq 'd1') { 'could not find descent.hog' } elseif ($ContentGame -eq 'd1') { 'could not find descent.hog with descent.pig' } else { 'could not find descent2.hog or d2demo.hog' }
    if ($failed.status -ne 'failed' -or $failed.problems[0] -cne $expectedProblem -or -not $failed.worker_restart_required) {
        throw 'Initialization failure did not preserve its original error and request retirement'
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while ((WorkerPid) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (WorkerPid) { throw 'Failed worker was not retired' }
    $first = Request 'valid' $dataPath $level
    if ($first.status -ne 'ok' -or $first.levels.Count -ne 1) { throw 'Valid request did not recover in a fresh worker' }
    $cacheFile = $first.levels[0].route_cache_file
    if ($cacheFile -notmatch "^route-cache/g[0-9]+/$Game-") { throw 'Wrong engine route-cache namespace' }
    Device @('shell', 'run-as', $package, 'test', '-f', "files/$($Game)x-redux/$cacheFile") | Out-Null
    $healthyPid = WorkerPid
    if (-not $healthyPid) { throw 'Healthy worker exited' }
    $badRequest = Request 'missing-level-name' $dataPath ''
    if ($badRequest.status -ne 'failed' -or (WorkerPid) -cne $healthyPid) {
        throw 'A request validation error incorrectly retired a healthy worker'
    }
    $repeat = Request 'repeat' $dataPath $level
    if ($repeat.status -ne 'ok' -or (WorkerPid) -cne $healthyPid -or
        ($first.levels | ConvertTo-Json -Depth 100 -Compress) -cne ($repeat.levels | ConvertTo-Json -Depth 100 -Compress)) {
        throw 'Healthy worker reuse changed the level result'
    }
    Write-Output "PASS Android $Game metadata initialization failure, recovery, request failure and healthy reuse"
} finally {
    Device @('logcat', '-d') | Set-Content -LiteralPath (Join-Path $OutputDirectory 'logcat.txt') -Encoding utf8
    $observed | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'observed.json') -Encoding utf8
    Device @('shell', 'am', 'force-stop', $package) | Out-Null
    foreach ($name in @('files', 'shared_prefs', 'cache')) {
        if ($created -contains $name) { Device @('shell', 'run-as', $package, 'rm', '-rf', $name) | Out-Null }
        if ($moved -contains $name) { Device @('shell', 'run-as', $package, 'mv', "$backup/$name", $name) | Out-Null }
    }
    Device @('shell', 'run-as', $package, 'rmdir', $backup) | Out-Null
    Device @('shell', 'rm', '-f', '/data/local/tmp/metadata-worker-request.json') | Out-Null
}
