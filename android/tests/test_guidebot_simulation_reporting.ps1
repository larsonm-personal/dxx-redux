#!/usr/bin/env pwsh
# Exercise the runner's real completion callback with native result/exit combinations
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/guidebot_simulation_regression.ps1')
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$ast = [Management.Automation.Language.Parser]::ParseFile($runner, [ref]$null, [ref]$null)
$infrastructureFunction = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'New-GuidebotInfrastructureErrorResult'
    }, $true)
. ([scriptblock]::Create($infrastructureFunction.Extent.Text))
$pool = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'Invoke-HeadlessProcessPool'
    }, $true)
$callback = $pool.CommandElements[-1].ScriptBlock.GetScriptBlock()
$root = Join-Path $repoRoot ('android/temp/guidebot_reporting/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root
New-Item -ItemType Directory -Path $root -Force | Out-Null
function Write-GuidebotStatus {
    param([string]$Message, [string]$Color = 'Cyan')
    $messages.Add([pscustomobject]@{ Message = $Message; Color = $Color })
}
function Publish-GuidebotResult {
    param($Item)
    Write-GuidebotSimulationJson -Path (Join-Path $root 'published.json') -Value $resultsByIdentity[$Item.Identity]
}
$mission = [pscustomobject]@{ mission_filename = 'fixture'; target_index = 0 }
$level = [pscustomobject]@{ level_num = 1; level_file = 'fixture.rl2'; route_status = 'ok'; route_steps = @() }
$item = [pscustomobject]@{ Identity = 'fixture.json|0|1|fixture.rl2'; Mission = $mission; Level = $level }
$selectedItems = @($item)
$Repeat = 1
$LevelTimeoutSeconds = 180
foreach ($case in @(
        @{ Status = 'failed'; Exit = 2; Infrastructure = $false },
        @{ Status = 'timeout'; Exit = 2; Infrastructure = $false },
        @{ Status = 'failed'; Exit = -1073741819; Infrastructure = $true },
        @{ Status = 'failed'; Exit = 2; Infrastructure = $true; Missing = $true }
    )) {
    $task = [pscustomobject]@{
        Item = $item; Run = 1
        Output = Join-Path $root ('raw-' + [guid]::NewGuid().ToString('N') + '.json')
        Log = Join-Path $root 'native.log'
    }
    if (-not $case.Missing) {
        Write-GuidebotSimulationJson -Path $task.Output -Value ([ordered]@{
                status = $case.Status; problem = 'fixture route blocked'; frames = 120; objectives = @()
            })
    }
    $runStates = @{ $item.Identity = [pscustomobject]@{
            Runs = @{}; Problems = [Collections.Generic.List[string]]::new(); Completed = 0
        }
    }
    $resultsByIdentity = @{}
    $infrastructureFailures = [Collections.Generic.List[object]]::new()
    $messages = [Collections.Generic.List[object]]::new()
    $progressState = @{ Retired = 0 }
    & $callback $task ([pscustomobject]@{
            ExitCode = $case.Exit; TimedOut = $false; StartError = ''; StandardOutput = ''; StandardError = ''
        })
    $published = Get-Content -LiteralPath (Join-Path $root 'published.json') -Raw | ConvertFrom-Json
    if ($case.Infrastructure) {
        if ($infrastructureFailures.Count -ne 1 -or $published.status -ne 'infrastructure_error' -or $messages[0].Color -ne 'Red') {
            throw 'Native crash/missing output must remain an infrastructure error'
        }
    } elseif ($infrastructureFailures.Count -ne 0 -or $published.status -ne $case.Status -or
        $messages[0].Color -ne 'Cyan' -or $messages[0].Message -notmatch 'Recorded:' -or
        $messages[0].Message -match 'FAILED|TIMEOUT|fixture route blocked') {
        throw 'Ordinary route outcomes must be published without console error reporting'
    }
}
Write-Host 'PASS: route outcomes are data; native crashes and missing results are infrastructure failures'
