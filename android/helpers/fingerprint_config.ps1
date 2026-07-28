function Get-DxxFingerprintMatchingConfig {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Fingerprint configuration not found: $Path"
    }
    $raw = Get-Content -LiteralPath $Path -Raw
    $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
    try {
        $config = $stripped | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "Invalid fingerprint configuration ${Path}: $($_.Exception.Message)"
    }

    function Get-RequiredFraction {
        param([Parameter(Mandatory = $true)][string]$Name)

        $property = $config.PSObject.Properties[$Name]
        if ($null -eq $property -or $property.Value -isnot [ValueType] -or $property.Value -is [bool]) {
            throw "Fingerprint configuration requires numeric $Name"
        }
        $value = [double]$property.Value
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or
            $value -le 0.0 -or $value -gt 1.0) {
            throw "Fingerprint configuration $Name must be finite and in (0, 1]"
        }
        return $value
    }

    return [pscustomobject]@{
        MatchThreshold = Get-RequiredFraction -Name 'match_threshold'
        DurationTolerance = Get-RequiredFraction -Name 'duration_tolerance'
    }
}
