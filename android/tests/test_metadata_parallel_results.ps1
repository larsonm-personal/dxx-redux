#!/usr/bin/env pwsh
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$helpers = Join-Path (Split-Path $PSScriptRoot) 'helpers'
. (Join-Path $helpers 'atomic_text_file.ps1')
. (Join-Path $helpers 'host_metadata_workspace.ps1')
$ast = [Management.Automation.Language.Parser]::ParseFile((Join-Path $helpers 'regenerate_all_mission_metadata_host.ps1'), [ref]$null, [ref]$null)
$getter = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-Prop' }, $true)
. ([scriptblock]::Create($getter.Extent.Text))
$pool = $ast.Find({ param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'Invoke-HeadlessProcessPool' }, $true)
$callback = $null
for ($i = 0; $i -lt $pool.CommandElements.Count - 1; $i++) {
    if ($pool.CommandElements[$i] -is [Management.Automation.Language.CommandParameterAst] -and $pool.CommandElements[$i].ParameterName -eq 'OnCompleted') {
        $callback = [scriptblock]::Create($pool.CommandElements[$i + 1].ScriptBlock.Extent.Text.Trim('{', '}'))
    }
}
if ($null -eq $callback) { throw 'Metadata completion callback not found' }
$root = Join-Path (Split-Path $PSScriptRoot) 'temp/test_metadata_parallel_results'
& (Join-Path $helpers 'retain-recent-artifacts.ps1') -Artifacts $root
$metadataDir = Join-Path $root 'metadata'
New-Item -ItemType Directory -Path $metadataDir -Force | Out-Null
$parallelResults = [Collections.Generic.List[object]]::new()
$parallelProgress = @{ Retired = 0 }
$archives = @('fixture.rar')
$summaries = [Collections.Generic.List[object]]::new()
function Write-Status { param($Message, $Color) }
function Write-SummaryRecord { param($Record) $summaries.Add($Record) }
foreach ($case in @('passed', 'failed', 'missing', 'empty', 'timeout', 'start_error')) {
    $workerRoot = Join-Path $root ($case + '-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $workerRoot | Out-Null
    $summaryPath = Join-Path $workerRoot 'summary.json'
    if ($case -in @('passed', 'failed')) {
        $record = [ordered]@{ name = 'fixture.rar'; status = $case }
        if ($case -eq 'failed') { $record.reason = 'fixture rejected' }
        [IO.File]::WriteAllText($summaryPath, (ConvertTo-Json -InputObject @($record)))
    } elseif ($case -eq 'empty') { [IO.File]::WriteAllText($summaryPath, '[]') }
    $task = [pscustomobject]@{
        WorkerRoot = $workerRoot
        ArchiveRecord = [pscustomobject]@{
            Archive = [pscustomobject]@{ FullName = 'fixture.rar'; Name = 'fixture.rar'; Length = 1 }
            Source = [pscustomobject]@{ Id = 'fixture' }
        }
    }
    $processResult = [pscustomobject]@{
        StartError = if ($case -eq 'start_error') { 'fixture start failed' } else { '' }
        TimedOut = $case -eq 'timeout'
        ExitCode = if ($case -eq 'passed') { 0 } else { 1 }
        StandardOutput = ''; StandardError = ''
    }
    & $callback $task $processResult
    $result = $parallelResults[$parallelResults.Count - 1]
    $expected = if ($case -eq 'passed') { 'passed' } else { 'failed' }
    if ($result.status -ne $expected) { throw "Incorrect result for $case" }
    if ($expected -eq 'failed' -and -not $result.reason) { throw "Missing failure context for $case" }
}
if ($parallelResults.Count -ne 6 -or $summaries.Count -ne 6) { throw 'Parallel results were lost' }
Write-Host 'Strict metadata parallel result tests passed'
