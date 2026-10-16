#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
$helperPath = Join-Path $PSScriptRoot '../helpers/test_helpers.ps1'
$helperSource = Get-Content -LiteralPath $helperPath -Raw
# Load the real predicate without initializing an Android SDK or device
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($helperPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Cannot parse automation helpers: $parseErrors" }
$definition = $ast.Find({
        param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-AutomationResultComplete'
    }, $true)
if (-not $definition) { throw 'Missing automation completion predicate' }
. ([scriptblock]::Create($definition.Extent.Text))

$completeResult = '{"result":"PASS","steps_completed":3,"total_steps":3}' | ConvertFrom-Json
if (-not (Test-AutomationResultComplete -Result $completeResult)) { throw 'Complete PASS was rejected' }
foreach ($json in @(
        '{"result":"PASS"}',
        '{"result":"PASS","steps_completed":0,"total_steps":0}',
        '{"result":"PASS","steps_completed":1,"total_steps":2}',
        '{"result":"PASS","steps_completed":3,"total_steps":2}',
        '{"result":"PASS","steps_completed":-1,"total_steps":-1}',
        '{"result":"PASS","steps_completed":"2","total_steps":2}',
        '{"result":"PASS","steps_completed":true,"total_steps":1}',
        '{"result":"PASS","steps_completed":1.5,"total_steps":1.5}',
        '{"result":"PASS","steps_completed":null,"total_steps":1}',
        '{"result":"FAIL","steps_completed":2,"total_steps":2}',
        '{"result":"LAUNCHER_CONTINUE","steps_completed":2,"total_steps":3}'
    )) {
    if (Test-AutomationResultComplete -Result ($json | ConvertFrom-Json)) { throw "Invalid terminal PASS accepted: $json" }
}
if ([regex]::Matches($helperSource, 'Test-AutomationResultComplete -Result \$(terminalResult|resultObj)').Count -ne 2) {
    throw 'Both automation PASS acceptance paths must validate completion'
}

Write-Host 'Automation completion evidence controls passed'
