function Get-CompatibleRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$TargetPath
    )

    $baseFull = [IO.Path]::GetFullPath($BasePath)
    $targetFull = [IO.Path]::GetFullPath($TargetPath)
    $baseRoot = [IO.Path]::GetPathRoot($baseFull)
    $targetRoot = [IO.Path]::GetPathRoot($targetFull)
    $comparison = if ([IO.Path]::DirectorySeparatorChar -eq '\') {
        [StringComparison]::OrdinalIgnoreCase
    } else {
        [StringComparison]::Ordinal
    }
    if (-not $baseRoot.Equals($targetRoot, $comparison)) {
        return $targetFull
    }

    $separators = [char[]]@([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $baseRemainder = $baseFull.Substring($baseRoot.Length).TrimEnd($separators)
    $targetRemainder = $targetFull.Substring($targetRoot.Length).TrimEnd($separators)
    $baseParts = if ($baseRemainder) {
        @($baseRemainder.Split($separators, [StringSplitOptions]::RemoveEmptyEntries))
    } else {
        @()
    }
    $targetParts = if ($targetRemainder) {
        @($targetRemainder.Split($separators, [StringSplitOptions]::RemoveEmptyEntries))
    } else {
        @()
    }

    $commonCount = 0
    while ($commonCount -lt $baseParts.Count -and
        $commonCount -lt $targetParts.Count -and
        $baseParts[$commonCount].Equals($targetParts[$commonCount], $comparison)) {
        $commonCount++
    }

    $relativeParts = New-Object System.Collections.Generic.List[string]
    for ($index = $commonCount; $index -lt $baseParts.Count; $index++) {
        $relativeParts.Add('..')
    }
    for ($index = $commonCount; $index -lt $targetParts.Count; $index++) {
        $relativeParts.Add($targetParts[$index])
    }
    if ($relativeParts.Count -eq 0) {
        return '.'
    }
    return [string]::Join([IO.Path]::DirectorySeparatorChar, $relativeParts)
}

function ConvertTo-CompatibleHashtableValue {
    param([AllowNull()][object]$Value)

    if ($Value -is [Collections.IDictionary]) {
        $result = [Collections.Hashtable]::new([StringComparer]::Ordinal)
        foreach ($key in $Value.Keys) {
            $result[[string]$key] = ConvertTo-CompatibleHashtableValue -Value $Value[$key]
        }
        return $result
    }
    if ($Value -is [Collections.IList] -and $Value -isnot [string]) {
        $result = [object[]]::new($Value.Count)
        for ($index = 0; $index -lt $Value.Count; $index++) {
            $result[$index] = ConvertTo-CompatibleHashtableValue -Value $Value[$index]
        }
        return , $result
    }
    return $Value
}

function ConvertFrom-CompatibleJsonHashtable {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Json)

    $parameters = @{ InputObject = $Json; ErrorAction = 'Stop' }
    if ((Get-Command ConvertFrom-Json).Parameters.ContainsKey('AsHashtable')) {
        $parameters['AsHashtable'] = $true
        return ConvertFrom-Json @parameters
    }

    Add-Type -AssemblyName System.Web.Extensions
    $serializer = [Web.Script.Serialization.JavaScriptSerializer]::new()
    $serializer.MaxJsonLength = [int]::MaxValue
    $serializer.RecursionLimit = 1024
    return ConvertTo-CompatibleHashtableValue -Value $serializer.DeserializeObject($Json)
}
