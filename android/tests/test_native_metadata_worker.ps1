param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$DataDirectory,
    [ValidateSet('d1', 'd2')][string]$Game = 'd1',
    [ValidateSet('d1', 'd2')][string]$ContentGame,
    [switch]$CheckProfileSwitch,
    [string]$CustomDirectory,
    [string]$OutputDirectory,
    [int]$TimeoutSeconds = 120
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $repo 'android/helpers/host_metadata_worker.ps1')
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo "temp/native-metadata-worker-$Game" }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $OutputDirectory
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$emptyDirectory = Join-Path $OutputDirectory 'empty'
New-Item -ItemType Directory -Force $emptyDirectory | Out-Null
$DataDirectory = (Resolve-Path -LiteralPath $DataDirectory).Path
$Executable = (Resolve-Path -LiteralPath $Executable).Path
if (-not $ContentGame) { $ContentGame = $Game }
$level = if ($ContentGame -eq 'd1') { 'level01.rdl' } else { 'd2leva-1.rl2' }
$observed = [Collections.Generic.List[object]]::new()

# Exercise the native guard even if a caller ignores the first retirement flag
$worker = New-MetadataWorker -Executable $Executable
try {
    $outputTask = $worker.Process.StandardOutput.ReadToEndAsync()
    foreach ($requestId in @('first-init-failure', 'repeated-init')) {
        $worker.Process.StandardInput.WriteLine(([ordered]@{
                    request_id = $requestId; game = $Game; source_type = 'level'; data_dir = $emptyDirectory
                    content_game = $ContentGame
                    level_file = $level
                } | ConvertTo-Json -Compress))
    }
    $worker.Process.StandardInput.Close()
    if (-not $worker.Process.WaitForExit($TimeoutSeconds * 1000)) { throw 'Native initialization guard hung' }
    $text = $outputTask.GetAwaiter().GetResult()
    $text | Set-Content -LiteralPath (Join-Path $OutputDirectory 'native-guard.log') -Encoding utf8
    $results = @($text -split '\r?\n' | Where-Object { $_.StartsWith("DXXMETA`t") } | ForEach-Object { $_.Substring(8) | ConvertFrom-Json })
    if ($worker.Process.ExitCode -ne 0 -or $results.Count -ne 2 -or
        -not $results[0].worker_restart_required -or -not $results[1].worker_restart_required -or
        $results[1].problems[0] -cne 'metadata worker requires restart') {
        throw 'Native worker repeated partial initialization instead of returning a structured failure'
    }
} finally {
    Stop-MetadataWorkerProcess -Worker $worker
}
$worker = New-MetadataWorker -Executable $Executable

function Request([string]$Name, [string]$Data, [string]$LevelFile, [string]$Content = $ContentGame,
    [string]$ExtraDirectory = '', [string]$MissionName = '') {
    if (-not $MissionName -and $Content -eq 'd1' -and $Game -eq 'd2') { $MissionName = 'descent' }
    $result = Invoke-MetadataWorker -Worker $worker -Request ([ordered]@{
            schema = 'dxx-level-metadata-request-v1'; request_id = $Name; game = $Game
            source_type = 'active_level'; source_name = $Name; data_dir = $Data
            content_game = $Content; mission_name = $MissionName; extra_data_dir = $ExtraDirectory
            level_file = $LevelFile; level_num = 1
        }) -RawOutputPath (Join-Path $OutputDirectory "$Name.json") `
        -LogPath (Join-Path $OutputDirectory "$Name.log") -TimeoutSeconds $TimeoutSeconds
    $processId = if ($null -ne $worker.Process) { $worker.Process.Id } else { $null }
    $observed.Add([ordered]@{ request = $Name; status = $result.status; worker_pid = $processId })
    return $result
}

try {
    $failed = Request 'missing-base' $emptyDirectory $level
    $expectedProblem = if ($Game -eq 'd1') { 'could not find descent.hog' } elseif ($ContentGame -eq 'd1') { 'could not find descent.hog with descent.pig' } else { 'could not find descent2.hog or d2demo.hog' }
    if ($failed.status -ne 'failed' -or $failed.problems[0] -cne $expectedProblem -or $null -ne $worker.Process) {
        throw 'Initialization failure did not preserve its original error and retire the worker'
    }
    $first = Request 'valid' $DataDirectory $level
    if ($first.status -ne 'ok' -or $first.levels.Count -ne 1) { throw 'Valid request did not recover in a fresh worker' }
    $healthyPid = $worker.Process.Id
    $badRequest = Request 'missing-level-name' $DataDirectory ''
    if ($badRequest.status -ne 'failed' -or $worker.Process.Id -ne $healthyPid) {
        throw 'A request validation error incorrectly retired a healthy worker'
    }
    $repeat = Request 'repeat' $DataDirectory $level
    if ($repeat.status -ne 'ok' -or $worker.Process.Id -ne $healthyPid -or
        ($first.levels | ConvertTo-Json -Depth 100 -Compress) -cne ($repeat.levels | ConvertTo-Json -Depth 100 -Compress)) {
        throw 'Healthy worker reuse changed the level result'
    }
    if ($CheckProfileSwitch) {
        if ($Game -ne 'd2' -or $ContentGame -ne 'd1') { throw 'Profile switch requires imported D1 with both base games present' }
        $d2 = Request 'switch-d2' $DataDirectory 'd2leva-1.rl2' 'd2'
        if ($d2.status -ne 'ok' -or $worker.Process.Id -eq $healthyPid) { throw 'D2 profile switch did not recover automatically' }
        $d2Pid = $worker.Process.Id
        $d1 = Request 'switch-back-d1' $DataDirectory $level 'd1'
        if ($d1.status -ne 'ok' -or $worker.Process.Id -eq $d2Pid -or
            ($first.levels | ConvertTo-Json -Depth 100 -Compress) -cne ($d1.levels | ConvertTo-Json -Depth 100 -Compress)) {
            throw 'Imported D1 profile switch did not recover its metadata'
        }
    }
    if ($CustomDirectory) {
        if ($Game -ne 'd2' -or $ContentGame -ne 'd1') { throw 'Custom assets require imported D1' }
        $fixture = Join-Path $OutputDirectory 'custom'
        New-Item -ItemType Directory -Force $fixture | Out-Null
        foreach ($extension in @('rdl', 'pg1', 'dtx', 'hx1')) {
            Copy-Item -LiteralPath (Join-Path $CustomDirectory "chklevel.$extension") -Destination $fixture
        }
        [IO.File]::WriteAllText((Join-Path $fixture 'mdcustom.msn'), "name = Metadata custom`nnum_levels = 1`nchklevel.rdl`n")
        $custom = Request 'custom' $DataDirectory 'chklevel.rdl' 'd1' $fixture 'mdcustom'
        if ($custom.status -ne 'ok') { throw "Custom mission failed: $($custom.problems -join '; ')" }
        if ($custom.status -ne 'ok' -or $custom.levels[0].replacement_groups.Count -eq 0) { throw 'D1 custom definitions were not applied' }
        $stock = Request 'stock-after-custom' $DataDirectory $level
        if ($stock.status -ne 'ok' -or
            ($first.levels | ConvertTo-Json -Depth 100 -Compress) -cne ($stock.levels | ConvertTo-Json -Depth 100 -Compress)) {
            throw 'Custom definitions leaked into the next stock request'
        }
    }
    Write-Output "PASS engine=$Game content=$ContentGame metadata initialization failure, recovery, request failure and healthy reuse"
} finally {
    $observed | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'observed.json') -Encoding utf8
    Stop-MetadataWorkerProcess -Worker $worker
}
