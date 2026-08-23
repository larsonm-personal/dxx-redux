#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers\runtime_targeted_sampling.ps1')
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers\run_all_tests_profile_menu.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$items = @(
    @{ Name = 'host-a'; Requires = 'none'; Type = 'ps1'; EstimatedRuntime = 600 },
    @{ Name = 'host-b'; Requires = 'none'; Type = 'ps1'; EstimatedRuntime = 700 },
    @{ Name = 'emu-a'; Requires = 'emulator'; Type = 'jsonc'; EstimatedRuntime = 500 },
    @{ Name = 'emu-b'; Requires = 'emulator'; Type = 'ps1'; EstimatedRuntime = 800 },
    @{ Name = 'dual-a'; Requires = 'two_emulators'; Type = 'ps1'; EstimatedRuntime = 900 }
)
$membersWithoutDependency = @(Join-RuntimeSampleMembers -Primary $items[0] -Dependencies @($null))
Assert-True ($membersWithoutDependency.Count -eq 1 -and $membersWithoutDependency[0].Name -eq 'host-a') `
    'A sample unit without a dependency should not contain a null member'
$first = Select-RuntimeTargetedItems -Items $items -TargetSeconds 2100 -GroupProperties Requires, Type -Seed 42
$second = Select-RuntimeTargetedItems -Items $items -TargetSeconds 2100 -GroupProperties Requires, Type -Seed 42
Assert-True (($first.Items.Name -join ',') -eq ($second.Items.Name -join ',')) 'A fixed seed should reproduce the same sample'
Assert-True ($first.EstimatedSeconds -eq 2100) 'The sample should fill the available historical-runtime budget'
Assert-True (@($first.Items | Where-Object Type -eq 'jsonc').Count -gt 0) 'The sample should cover the jsonc type'
Assert-True (@($first.Items | ForEach-Object { Get-RuntimeSampleValue $_ 'Requires' } | Sort-Object -Unique).Count -ge 2) `
    'The sample should spread across infrastructure types'

$oversized = Select-RuntimeTargetedItems -Items @(@{ Name = 'large'; Kind = 'only'; EstimatedRuntime = 4000 }) `
    -TargetSeconds 2700 -GroupProperties Kind -Seed 7
Assert-True ($oversized.Items.Count -eq 1) 'A coarse item should still be selected when none fit the target'

$proportionalItems = @()
foreach ($definition in @(
        @{ Group = 'host|ps1'; Count = 4; Runtime = 100 },
        @{ Group = 'emulator|jsonc'; Count = 2; Runtime = 200 },
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

$cursorPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'temp\runtime_sample_cursor_test.json'
Remove-Item -LiteralPath $cursorPath -Force -ErrorAction SilentlyContinue
try {
    $ringFirst = Select-RuntimeHashRingItems -Items $proportionalItems -TargetSeconds 450 `
        -GroupProperties Requires, Type -StatePath $cursorPath -RingName tests -Seed 23
    $ringSecond = Select-RuntimeHashRingItems -Items $proportionalItems -TargetSeconds 450 `
        -GroupProperties Requires, Type -StatePath $cursorPath -RingName tests -Seed 23
    Assert-True ($ringFirst.EstimatedSeconds -ge 450) `
        'Hash-ring sampling should include the first target that crosses the runtime cutoff'
    Assert-True ($ringSecond.PreviousTargetRecovered) 'A later hash-ring batch should recover the prior cursor'
    Assert-True ($ringSecond.StartTarget -ne $ringFirst.StartTarget) `
        'A later hash-ring batch should resume after the prior batch'
    Assert-True (@($ringFirst.Items.Name | Where-Object { $_ -in $ringSecond.Items.Name }).Count -eq 0) `
        'Hash-ring batches should not overlap before the ring wraps'
    Assert-True (@($ringFirst.Items | ForEach-Object { Get-RuntimeSampleValue $_ 'Requires' } | Sort-Object -Unique).Count -ge 2) `
        'The interleaved hash order should spread a batch across infrastructure groups'
} finally {
    Remove-Item -LiteralPath $cursorPath -Force -ErrorAction SilentlyContinue
}

$choices = [Collections.Generic.Queue[string]]::new()
$choices.Enqueue('invalid')
$choices.Enqueue('T')
$profile = Select-RunAllTestsProfile -ReadChoice { $choices.Dequeue() }
Assert-True ($profile -eq 'Target45') 'The opening menu should expose the T profile'
Assert-True ((Select-RunAllTestsProfile -ReadChoice { '1' }) -eq 'Full') `
    'The opening menu should preserve the full-suite choice'
Assert-True ((Select-RunAllTestsProfile -ReadChoice { 'q' }) -eq 'Cancel') `
    'The opening menu should support cancellation'
Assert-True (Test-RunAllTestsProfileMenuEnabled -ExplicitParameterCount 0 `
        -UserInteractive $true -InputRedirected $false) `
    'A zero-parameter interactive run should show the opening menu'
Assert-True (-not (Test-RunAllTestsProfileMenuEnabled -ExplicitParameterCount 1 `
            -UserInteractive $true -InputRedirected $false)) `
    'Parameterized runs should bypass the opening menu'
Assert-True (-not (Test-RunAllTestsProfileMenuEnabled -ExplicitParameterCount 0 `
            -UserInteractive $true -InputRedirected $true)) `
    'Redirected unattended runs should bypass the opening menu'

Write-Host 'Runtime-targeted sampling tests passed' -ForegroundColor Green
exit 0
