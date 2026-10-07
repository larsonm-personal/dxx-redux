# Validate complete physical-track identities before publishing a disc hash catalog
function Get-CueTrackDefinitions {
    param([Parameter(Mandatory)][string]$Path)

    $tracks = @()
    $seen = [Collections.Generic.HashSet[int]]::new()
    $text = [IO.File]::ReadAllText($Path)
    foreach ($match in [regex]::Matches($text, '(?im)^\s*TRACK\s+([0-9]+)\s+([^\s]+)')) {
        $number = [int]$match.Groups[1].Value
        if (-not $seen.Add($number)) { throw "Duplicate CUE track number: $number" }
        $tracks += [PSCustomObject]@{
            track = $number
            type = if ($match.Groups[2].Value -ieq 'AUDIO') { 'audio' } else { 'data' }
        }
    }
    if ($tracks.Count -eq 0) { throw "CUE contains no tracks: $Path" }
    return $tracks
}

function Get-ValidatedDiscTrackManifest {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Manifest,
        [Parameter(Mandatory, ParameterSetName = 'Cue')][string]$CuePath,
        [Parameter(Mandatory, ParameterSetName = 'Iso')][string]$IsoPath
    )
    $tracks = @(if ($IsoPath) {
            if (-not (Test-Path -LiteralPath $IsoPath -PathType Leaf)) { throw "ISO source not found: $IsoPath" }
            [pscustomobject]@{ track = 1; type = 'data' }
        } else { Get-CueTrackDefinitions -Path $CuePath })
    if ($Manifest.Count -ne $tracks.Count) { throw "Expected $($tracks.Count) track hashes, got $($Manifest.Count)" }
    $expected = @{}
    foreach ($track in $tracks) { $expected[$track.track] = $track.type }
    $seen = [Collections.Generic.HashSet[int]]::new()
    $validated = @()
    foreach ($entry in $Manifest) {
        if ($null -eq $entry -or [string]$entry.track -cnotmatch '^[1-9][0-9]*$') { throw 'Invalid physical track number' }
        $number = [int]$entry.track
        if (-not $expected.ContainsKey($number) -or -not $seen.Add($number)) { throw "Unexpected or duplicate track: $number" }
        if ([string]$entry.type -cne $expected[$number]) { throw "Track $number type does not match CUE" }
        if ($entry.PSObject.Properties['error'] -and -not [string]::IsNullOrWhiteSpace([string]$entry.error)) { throw "Track $number reported an error" }
        if ([string]$entry.sha1 -cnotmatch '^[0-9a-f]{40}$') { throw "Track $number has an invalid SHA-1" }
        $record = [ordered]@{ track = $number; type = $expected[$number]; sha1 = [string]$entry.sha1 }
        if ($entry.PSObject.Properties['source_format']) {
            $format = [string]$entry.source_format
            if ($format -cnotmatch '^[A-Za-z0-9_./+-]{1,64}$') { throw "Track $number has an invalid source format" }
            $record.source_format = $format
        }
        $validated += $record
    }
    return $validated | Sort-Object { $_.track }
}
