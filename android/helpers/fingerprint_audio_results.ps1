function Assert-DxxFingerprintAudioResults {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$ExpectedNames,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [object[]]$Results
    )

    $expected = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $ExpectedNames) {
        if ([string]::IsNullOrWhiteSpace($name) -or -not $expected.Add($name)) {
            throw "Invalid or duplicate expected audio filename: $name"
        }
    }

    $actual = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($result in $Results) {
        $name = [string]$result.filename
        if ([string]::IsNullOrWhiteSpace($name)) {
            throw 'Fingerprint result is missing a filename'
        }
        if (-not $actual.Add($name)) {
            throw "Duplicate fingerprint result: $name"
        }
        if (-not $expected.Contains($name)) {
            throw "Unexpected fingerprint result: $name"
        }
        if ([string]::IsNullOrWhiteSpace([string]$result.chromaprint) -or [long]$result.duration_ms -le 0) {
            throw "Incomplete fingerprint result: $name"
        }
    }

    $missing = @($expected | Where-Object { -not $actual.Contains($_) } | Sort-Object)
    if ($missing.Count -gt 0) {
        throw "Missing fingerprint result(s): $($missing -join ', ')"
    }
}
