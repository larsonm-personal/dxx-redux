function ConvertTo-DxxAcoustIdTitleKey {
    param(
        [string]$Value,
        [switch]$ExternalLabel
    )
    if ([string]::IsNullOrWhiteSpace($Value)) { return "" }
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($Value)
    if ($ExternalLabel -and $baseName -match ' - ') {
        $baseName = ($baseName -split ' - ', 2)[1]
    }
    $key = (($baseName.ToLowerInvariant() -replace '[^\p{L}\p{Nd}]+', ' ').Trim() -replace '^\d+\s+', '')
    $key = $key -replace '^(level|stage)\s+0*\d+\s+', ''
    $key = $key -replace '\s+mn\d+$', ''
    return ($key -replace '\s+(psx|macplay)\s+(maximum\s+)?(mix|remix)$', '')
}

function Test-DxxAcoustIdTitleMatch {
    param(
        [string]$MaintainedLabel,
        [string]$ExternalLabel
    )
    $maintainedKey = ConvertTo-DxxAcoustIdTitleKey $MaintainedLabel
    return $maintainedKey -and $maintainedKey -eq
    (ConvertTo-DxxAcoustIdTitleKey $ExternalLabel -ExternalLabel)
}

function Get-DxxAcoustIdProperty {
    param(
        [object]$Value,
        [string]$Name
    )
    if ($null -eq $Value) { return $null }
    if ($Value -is [System.Collections.IDictionary]) {
        if ($Value.Contains($Name)) { return $Value[$Name] }
        return $null
    }
    $property = $Value.PSObject.Properties[$Name]
    if ($property) { return $property.Value }
    return $null
}

function Get-DxxReusableAcoustIdMetadata {
    param(
        [object]$Existing,
        [string]$Chromaprint,
        [string]$MaintainedLabel,
        [switch]$RequireReviewedFields
    )
    $existingFingerprint = [string](Get-DxxAcoustIdProperty $Existing 'chromaprint')
    $name = [string](Get-DxxAcoustIdProperty $Existing 'acoustid_name')
    $nameSource = [string](Get-DxxAcoustIdProperty $Existing 'name_source')
    if (-not $Existing -or -not $name -or $nameSource -eq 'tracklist' -or
        -not $Chromaprint -or $existingFingerprint -ne $Chromaprint) {
        return $null
    }
    if ($MaintainedLabel -and -not (Test-DxxAcoustIdTitleMatch $MaintainedLabel $name)) {
        return $null
    }

    $score = Get-DxxAcoustIdProperty $Existing 'acoustid_score'
    $recordingId = [string](Get-DxxAcoustIdProperty $Existing 'acoustid_recording_id')
    if ($RequireReviewedFields -and
        (($null -eq $score) -or [double]$score -lt 0.8 -or -not $recordingId)) {
        return $null
    }

    $metadata = [ordered]@{ acoustid_name = $name }
    foreach ($field in @('acoustid_album', 'acoustid_score', 'acoustid_recording_id', 'name_source')) {
        $value = Get-DxxAcoustIdProperty $Existing $field
        if ($null -ne $value -and [string]$value) {
            $metadata[$field] = $value
        }
    }
    return $metadata
}
