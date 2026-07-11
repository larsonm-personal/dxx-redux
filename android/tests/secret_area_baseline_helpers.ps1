function ConvertTo-JsonPointerToken {
    param([string]$Token)

    return $Token.Replace('~', '~0').Replace('/', '~1')
}

function Add-JsonLeafPaths {
    param(
        [AllowNull()]$Value,
        [string]$Path,
        [Parameter(Mandatory)]$Map
    )

    if ($null -eq $Value) {
        $Map[$Path] = 'null'
        return
    }

    if ($Value -is [System.Collections.IDictionary]) {
        $keys = @($Value.Keys)
        if ($keys.Count -eq 0) {
            $Map[$Path] = 'object:{}'
            return
        }
        foreach ($key in $keys) {
            $childPath = "$Path/$(ConvertTo-JsonPointerToken -Token ([string]$key))"
            Add-JsonLeafPaths -Value $Value[$key] -Path $childPath -Map $Map
        }
        return
    }

    if ($Value -is [System.Management.Automation.PSCustomObject]) {
        $properties = @($Value.PSObject.Properties)
        if ($properties.Count -eq 0) {
            $Map[$Path] = 'object:{}'
            return
        }
        foreach ($property in $properties) {
            $childPath = "$Path/$(ConvertTo-JsonPointerToken -Token $property.Name)"
            Add-JsonLeafPaths -Value $property.Value -Path $childPath -Map $Map
        }
        return
    }

    if ($Value -is [System.Collections.IEnumerable] -and $Value -isnot [string]) {
        $items = @($Value)
        if ($items.Count -eq 0) {
            $Map[$Path] = 'array:[]'
            return
        }
        for ($index = 0; $index -lt $items.Count; $index++) {
            Add-JsonLeafPaths -Value $items[$index] -Path "$Path/$index" -Map $Map
        }
        return
    }

    $typeName = $Value.GetType().Name
    $json = $Value | ConvertTo-Json -Compress -Depth 5
    $Map[$Path] = "${typeName}:$json"
}

function Get-JsonLeafMap {
    param([AllowNull()]$Value)

    $map = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::Ordinal)
    Add-JsonLeafPaths -Value $Value -Path '$' -Map $map
    return $map
}

function Format-JsonDiffValue {
    param(
        [string]$Value,
        [int]$MaxLength = 160
    )

    if ($Value.Length -le $MaxLength) { return $Value }
    return $Value.Substring(0, $MaxLength - 3) + '...'
}

function Compare-JsonStructure {
    param(
        [AllowNull()]$Expected,
        [AllowNull()]$Actual
    )

    $expectedMap = Get-JsonLeafMap -Value $Expected
    $actualMap = Get-JsonLeafMap -Value $Actual
    $added = [System.Collections.Generic.List[string]]::new()
    $removed = [System.Collections.Generic.List[string]]::new()
    $changed = [System.Collections.Generic.List[string]]::new()

    foreach ($path in @($actualMap.Keys | Sort-Object -CaseSensitive)) {
        if (-not $expectedMap.ContainsKey($path)) {
            $added.Add("+ $path = $(Format-JsonDiffValue -Value $actualMap[$path])")
        } elseif ($expectedMap[$path] -cne $actualMap[$path]) {
            $changed.Add("~ $path expected $(Format-JsonDiffValue -Value $expectedMap[$path]) actual $(Format-JsonDiffValue -Value $actualMap[$path])")
        }
    }
    foreach ($path in @($expectedMap.Keys | Sort-Object -CaseSensitive)) {
        if (-not $actualMap.ContainsKey($path)) {
            $removed.Add("- $path = $(Format-JsonDiffValue -Value $expectedMap[$path])")
        }
    }

    return [pscustomobject]@{
        Added = $added.Count
        Removed = $removed.Count
        Changed = $changed.Count
        Total = $added.Count + $removed.Count + $changed.Count
        AddedDetails = @($added)
        RemovedDetails = @($removed)
        ChangedDetails = @($changed)
    }
}

function Format-JsonStructureDiff {
    param(
        [Parameter(Mandatory)]$Diff,
        [int]$MaxDetails = 40
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("Structural diff: added=$($Diff.Added) removed=$($Diff.Removed) changed=$($Diff.Changed)")
    if ($Diff.Total -gt 0) {
        $details = [System.Collections.Generic.List[string]]::new()
        $categories = @(
            @($Diff.AddedDetails),
            @($Diff.RemovedDetails),
            @($Diff.ChangedDetails)
        )
        for ($index = 0; $details.Count -lt $MaxDetails; $index++) {
            $found = $false
            foreach ($category in $categories) {
                if ($index -lt $category.Count) {
                    $details.Add($category[$index])
                    $found = $true
                    if ($details.Count -ge $MaxDetails) { break }
                }
            }
            if (-not $found) { break }
        }
        $lines.Add("Showing $($details.Count) of $($Diff.Total) differences")
        foreach ($detail in $details) { $lines.Add($detail) }
    }
    return $lines -join "`n"
}
