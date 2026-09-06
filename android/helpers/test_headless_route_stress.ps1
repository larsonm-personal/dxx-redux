[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string[]]$EngineArguments,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(1, 10000)][int]$Runs = 100,
    [ValidateRange(1, 128)][int]$MaxParallel = 8,
    [ValidateRange(1, 7200)][int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'headless_process_pool.ps1')
Initialize-RegressionProcessLifetime
$exe = (Resolve-Path -LiteralPath $Executable).Path
$root = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
if (Test-Path -LiteralPath $root) { throw "Use a new output directory: $root" }
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts $root
New-Item -ItemType Directory -Path $root | Out-Null
$encoding = [Text.UTF8Encoding]::new($false)
$manifest = [ordered]@{
    executable = $exe
    executable_sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    arguments = $EngineArguments
    working_directory = (Get-Location).Path
    runs = $Runs
    max_parallel = $MaxParallel
    timeout_seconds = $TimeoutSeconds
}
[IO.File]::WriteAllText((Join-Path $root 'manifest.json'), ($manifest | ConvertTo-Json -Depth 10), $encoding)
$tasks = @(1..$Runs | ForEach-Object {
        [pscustomobject]@{
            FilePath = $exe
            Arguments = @($EngineArguments) + @('-route-confirm-json-out', (Join-Path $root "$_.json"))
            WorkingDirectory = (Get-Location).Path
            TimeoutSeconds = $TimeoutSeconds
            Index = $_
        }
    })
$results = [Collections.Generic.List[object]]::new()
Invoke-HeadlessProcessPool -Tasks $tasks -MaxParallel $MaxParallel -OnCompleted {
    param($task, $result)
    [IO.File]::WriteAllText((Join-Path $root "$($task.Index).stdout.log"), $result.StandardOutput, $encoding)
    [IO.File]::WriteAllText((Join-Path $root "$($task.Index).stderr.log"), $result.StandardError, $encoding)
    $record = [ordered]@{
        run = $task.Index
        exit_code = $result.ExitCode
        timed_out = $result.TimedOut
        start_error = $result.StartError
        result_present = Test-Path -LiteralPath (Join-Path $root "$($task.Index).json")
    }
    $results.Add($record)
    [IO.File]::WriteAllText((Join-Path $root "$($task.Index).process.json"), ($record | ConvertTo-Json), $encoding)
    Write-Host "[$($results.Count)/$Runs] run=$($task.Index) exit=$($result.ExitCode) timeout=$($result.TimedOut)"
}
[IO.File]::WriteAllText((Join-Path $root 'summary.json'), (ConvertTo-Json -InputObject @($results) -Depth 10), $encoding)
if (@($results | Where-Object { $_.exit_code -notin @(0, 2) -or $_.timed_out -or -not $_.result_present }).Count) { exit 1 }
