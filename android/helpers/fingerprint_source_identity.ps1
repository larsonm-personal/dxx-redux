function ConvertTo-DxxFingerprintSourceId {
    param([Parameter(Mandatory)][string]$Name)

    $id = ($Name.ToLowerInvariant() -replace '[^a-z0-9]+', '-').Trim('-')
    if (-not $id) {
        throw "Fingerprint source name '$Name' does not produce a nonempty ID"
    }
    return $id
}

function Get-DxxPortableSourceNameKey {
    param([Parameter(Mandatory)][string]$Name)

    $normalized = $Name.Normalize([Text.NormalizationForm]::FormKC).TrimEnd(' ', '.')
    if (-not $normalized) {
        throw "Fingerprint source name '$Name' is empty under portable path rules"
    }
    return $normalized.ToUpperInvariant()
}

function Assert-DxxUniqueFingerprintSourceIds {
    param(
        [Parameter(Mandatory)][object[]]$Sources,
        [object[]]$ReservedSources = @()
    )

    $seen = @{}
    foreach ($source in @($ReservedSources) + @($Sources)) {
        $id = [string]$source.Id
        $label = [string]$source.Label
        if (-not $id) {
            throw "Fingerprint source '$label' has an empty ID"
        }
        $key = $id.ToUpperInvariant()
        if ($seen.ContainsKey($key)) {
            $prior = $seen[$key]
            throw "Fingerprint source ID collision '$id' between '$($prior.Label)' and '$label'"
        }
        $seen[$key] = $source
    }
}
