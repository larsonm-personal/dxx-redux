#!/usr/bin/env pwsh

function Get-RuntimeSampleValue {
    param([Parameter(Mandatory)][object]$Item, [Parameter(Mandatory)][string]$Name)

    if ($Item -is [System.Collections.IDictionary]) {
        return $Item[$Name]
    }
    return $Item.$Name
}

function Select-RuntimeTargetedItems {
    param(
        [Parameter(Mandatory)][object[]]$Items,
        [Parameter(Mandatory)][ValidateRange(1, [int]::MaxValue)][int]$TargetSeconds,
        [string[]]$GroupProperties = @(),
        [int]$Seed = 0
    )

    if ($Items.Count -eq 0) {
        return [pscustomobject]@{ Items = @(); EstimatedSeconds = 0; Seed = $Seed }
    }
    if ($Seed -eq 0) {
        $Seed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue)
    }
    $random = [Random]::new($Seed)
    $remaining = [System.Collections.Generic.List[object]]::new()
    foreach ($item in $Items) { $remaining.Add($item) }
    for ($index = $remaining.Count - 1; $index -gt 0; $index--) {
        $swap = $random.Next($index + 1)
        $temporary = $remaining[$index]
        $remaining[$index] = $remaining[$swap]
        $remaining[$swap] = $temporary
    }

    $selected = [System.Collections.Generic.List[object]]::new()
    $selectedNames = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $estimatedSeconds = 0
    $addItem = {
        param([object]$Item)
        $name = [string](Get-RuntimeSampleValue -Item $Item -Name 'Name')
        if ($selectedNames.Add($name)) {
            $selected.Add($Item)
            $estimatedSeconds += [Math]::Max(1, [int](Get-RuntimeSampleValue -Item $Item -Name 'EstimatedRuntime'))
            [void]$remaining.Remove($Item)
        }
    }

    foreach ($property in $GroupProperties) {
        $values = @($remaining | ForEach-Object { [string](Get-RuntimeSampleValue -Item $_ -Name $property) } | Sort-Object -Unique)
        for ($index = $values.Count - 1; $index -gt 0; $index--) {
            $swap = $random.Next($index + 1)
            $temporary = $values[$index]
            $values[$index] = $values[$swap]
            $values[$swap] = $temporary
        }
        foreach ($value in $values) {
            $candidates = @($remaining | Where-Object { [string](Get-RuntimeSampleValue -Item $_ -Name $property) -eq $value })
            $candidate = @($candidates | Where-Object {
                    $estimatedSeconds + [Math]::Max(1, [int](Get-RuntimeSampleValue -Item $_ -Name 'EstimatedRuntime')) -le $TargetSeconds
                } | Select-Object -First 1)
            if ($candidate.Count -gt 0) {
                . $addItem $candidate[0]
            }
        }
    }

    foreach ($item in @($remaining)) {
        $seconds = [Math]::Max(1, [int](Get-RuntimeSampleValue -Item $item -Name 'EstimatedRuntime'))
        if ($estimatedSeconds + $seconds -le $TargetSeconds) {
            . $addItem $item
        }
    }

    if ($selected.Count -eq 0) {
        $nearest = $Items | Sort-Object { [Math]::Abs([int](Get-RuntimeSampleValue -Item $_ -Name 'EstimatedRuntime') - $TargetSeconds) } | Select-Object -First 1
        . $addItem $nearest
    } elseif ($remaining.Count -gt 0) {
        $nearest = $remaining | Sort-Object {
            [Math]::Abs($estimatedSeconds + [int](Get-RuntimeSampleValue -Item $_ -Name 'EstimatedRuntime') - $TargetSeconds)
        } | Select-Object -First 1
        $withNearest = $estimatedSeconds + [Math]::Max(1, [int](Get-RuntimeSampleValue -Item $nearest -Name 'EstimatedRuntime'))
        if ([Math]::Abs($withNearest - $TargetSeconds) -lt [Math]::Abs($estimatedSeconds - $TargetSeconds)) {
            . $addItem $nearest
        }
    }

    return [pscustomobject]@{
        Items = @($selected)
        EstimatedSeconds = $estimatedSeconds
        Seed = $Seed
    }
}

function Select-RuntimeProportionalItems {
    param(
        [Parameter(Mandatory)][object[]]$Items,
        [Parameter(Mandatory)][ValidateRange(1, [int]::MaxValue)][int]$TargetSeconds,
        [Parameter(Mandatory)][string[]]$GroupProperties,
        [int]$Seed = 0
    )

    if ($Items.Count -eq 0) {
        return [pscustomobject]@{ Items = @(); EstimatedSeconds = 0; Seed = $Seed; Fraction = 0; Groups = @() }
    }
    if ($Seed -eq 0) { $Seed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
    $random = [Random]::new($Seed)
    $grouped = @{}
    foreach ($item in $Items) {
        $keyParts = foreach ($property in $GroupProperties) {
            [string](Get-RuntimeSampleValue -Item $item -Name $property)
        }
        $key = $keyParts -join '|'
        if (-not $grouped.ContainsKey($key)) { $grouped[$key] = @() }
        $grouped[$key] += $item
    }
    $groups = @($grouped.Keys | Sort-Object | ForEach-Object {
            $shuffled = @($grouped[$_])
            for ($index = $shuffled.Count - 1; $index -gt 0; $index--) {
                $swap = $random.Next($index + 1)
                $temporary = $shuffled[$index]
                $shuffled[$index] = $shuffled[$swap]
                $shuffled[$swap] = $temporary
            }
            [pscustomobject]@{ Name = $_; Items = $shuffled }
        })

    $boundaries = [Collections.Generic.HashSet[double]]::new()
    [void]$boundaries.Add(0.0)
    [void]$boundaries.Add(1.0)
    foreach ($group in $groups) {
        for ($count = 1; $count -lt $group.Items.Count; $count++) {
            [void]$boundaries.Add(($count + 0.5) / $group.Items.Count)
        }
    }
    $sortedBoundaries = @($boundaries | Sort-Object)
    $fractions = [Collections.Generic.HashSet[double]]::new()
    [void]$fractions.Add(1.0)
    for ($index = 0; $index -lt $sortedBoundaries.Count - 1; $index++) {
        [void]$fractions.Add(($sortedBoundaries[$index] + $sortedBoundaries[$index + 1]) / 2.0)
        if ($sortedBoundaries[$index] -gt 0) { [void]$fractions.Add($sortedBoundaries[$index]) }
    }
    $best = $null
    foreach ($fraction in @($fractions | Sort-Object)) {
        $selected = @()
        $groupResults = @()
        $seconds = 0
        foreach ($group in $groups) {
            $count = [Math]::Max(1, [Math]::Min($group.Items.Count,
                    [int][Math]::Round($fraction * $group.Items.Count, [MidpointRounding]::AwayFromZero)))
            $chosen = @($group.Items | Select-Object -First $count)
            $selected += $chosen
            $groupSeconds = @($chosen | ForEach-Object {
                    [Math]::Max(1, [int](Get-RuntimeSampleValue -Item $_ -Name 'EstimatedRuntime'))
                } | Measure-Object -Sum).Sum
            $seconds += $groupSeconds
            $groupResults += [pscustomobject]@{
                Name = $group.Name
                Selected = $count
                Available = $group.Items.Count
                Fraction = $count / $group.Items.Count
                EstimatedSeconds = $groupSeconds
            }
        }
        $distance = [Math]::Abs($seconds - $TargetSeconds)
        if (-not $best -or $distance -lt $best.Distance -or
            ($distance -eq $best.Distance -and $seconds -lt $best.EstimatedSeconds)) {
            $best = [pscustomobject]@{
                Items = $selected
                EstimatedSeconds = $seconds
                Seed = $Seed
                Fraction = $fraction
                Groups = $groupResults
                Distance = $distance
            }
        }
    }
    return $best
}

function Select-RuntimeFractionItems {
    param(
        [Parameter(Mandatory)][object[]]$Items,
        [Parameter(Mandatory)][ValidateRange(0.000001, 1.0)][double]$Fraction,
        [int]$Seed = 0,
        [string]$NameProperty = 'Name'
    )

    if ($Items.Count -eq 0) { return @() }
    if ($Seed -eq 0) { $Seed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
    $random = [Random]::new($Seed)
    $shuffled = @($Items | Sort-Object { [string](Get-RuntimeSampleValue -Item $_ -Name $NameProperty) })
    for ($index = $shuffled.Count - 1; $index -gt 0; $index--) {
        $swap = $random.Next($index + 1)
        $temporary = $shuffled[$index]
        $shuffled[$index] = $shuffled[$swap]
        $shuffled[$swap] = $temporary
    }
    $count = [Math]::Max(1, [Math]::Min($shuffled.Count,
            [int][Math]::Round($Fraction * $shuffled.Count, [MidpointRounding]::AwayFromZero)))
    return @($shuffled | Select-Object -First $count)
}
