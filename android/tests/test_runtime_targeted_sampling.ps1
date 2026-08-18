#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers\runtime_targeted_sampling.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$items = @(
    @{ Name = 'host-a'; Requires = 'none'; Type = 'ps1'; EstimatedRuntime = 600 },
    @{ Name = 'host-b'; Requires = 'none'; Type = 'ps1'; EstimatedRuntime = 700 },
    @{ Name = 'emu-a'; Requires = 'emulator'; Type = 'json5'; EstimatedRuntime = 500 },
    @{ Name = 'emu-b'; Requires = 'emulator'; Type = 'ps1'; EstimatedRuntime = 800 },
    @{ Name = 'dual-a'; Requires = 'two_emulators'; Type = 'ps1'; EstimatedRuntime = 900 }
)
$first = Select-RuntimeTargetedItems -Items $items -TargetSeconds 2100 -GroupProperties Requires, Type -Seed 42
$second = Select-RuntimeTargetedItems -Items $items -TargetSeconds 2100 -GroupProperties Requires, Type -Seed 42
Assert-True (($first.Items.Name -join ',') -eq ($second.Items.Name -join ',')) 'A fixed seed should reproduce the same sample'
Assert-True ($first.EstimatedSeconds -eq 2100) 'The sample should fill the available historical-runtime budget'
Assert-True (@($first.Items | Where-Object Type -eq 'json5').Count -gt 0) 'The sample should cover the json5 type'
Assert-True (@($first.Items | Select-Object -ExpandProperty Requires -Unique).Count -ge 2) 'The sample should spread across infrastructure types'

$oversized = Select-RuntimeTargetedItems -Items @(@{ Name = 'large'; Kind = 'only'; EstimatedRuntime = 4000 }) `
    -TargetSeconds 2700 -GroupProperties Kind -Seed 7
Assert-True ($oversized.Items.Count -eq 1) 'A coarse item should still be selected when none fit the target'

$proportionalItems = @()
foreach ($definition in @(
        @{ Group = 'host|ps1'; Count = 4; Runtime = 100 },
        @{ Group = 'emulator|json5'; Count = 2; Runtime = 200 },
        @{ Group = 'emulator|ps1'; Count = 6; Runtime = 50 }
    )) {
    $parts = $definition.Group -split '\|'
    for ($index = 1; $index -le $definition.Count; $index++) {
        $proportionalItems += @{
            Name = "$($definition.Group)-$index"
            Requires = $parts[0]
            Type = $parts[1]
            EstimatedRuntime = $definition.Runtime
        }
    }
}
$proportional = Select-RuntimeProportionalItems -Items $proportionalItems -TargetSeconds 550 `
    -GroupProperties Requires, Type -Seed 17
Assert-True ($proportional.EstimatedSeconds -eq 550) 'Proportional sampling should use runtime estimates to approach the target'
Assert-True (($proportional.Groups.Selected -join ',') -eq '1,3,2') `
    'Proportional sampling should select the same one-half fraction from each group'
Assert-True (@($proportional.Groups | Where-Object Selected -eq 0).Count -eq 0) `
    'Proportional sampling should preserve every nonempty group'
$fractionItems = Select-RuntimeFractionItems -Items $proportionalItems -Fraction 0.25 -Seed 9
Assert-True ($fractionItems.Count -eq 3) 'Fraction sampling should round to the requested share'
Assert-True (($fractionItems.Name -join ',') -eq ((Select-RuntimeFractionItems `
                -Items $proportionalItems -Fraction 0.25 -Seed 9).Name -join ',')) `
    'Fraction sampling should be reproducible'

Write-Host 'Runtime-targeted sampling tests passed' -ForegroundColor Green
exit 0
