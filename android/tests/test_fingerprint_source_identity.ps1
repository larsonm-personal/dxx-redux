$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $repoRoot 'android\helpers\fingerprint_source_identity.ps1')

function Assert-ThrowsLike {
    param([scriptblock]$Action, [string]$Pattern)

    try {
        & $Action
    } catch {
        if ($_.Exception.Message -notlike $Pattern) {
            throw "Expected '$Pattern', got '$($_.Exception.Message)'"
        }
        return
    }
    throw "Expected failure matching '$Pattern'"
}

if ((ConvertTo-DxxFingerprintSourceId 'A+B') -ne 'a-b') {
    throw 'Punctuation slug conversion changed unexpectedly'
}
Assert-ThrowsLike { ConvertTo-DxxFingerprintSourceId '日本語' } '*nonempty ID*'

$punctuationCollision = @(
    [PSCustomObject]@{ Id = ConvertTo-DxxFingerprintSourceId 'A+B'; Label = 'A+B' }
    [PSCustomObject]@{ Id = ConvertTo-DxxFingerprintSourceId 'A B'; Label = 'A B' }
)
Assert-ThrowsLike {
    Assert-DxxUniqueFingerprintSourceIds -Sources $punctuationCollision
} '*ID collision*'

$album = @([PSCustomObject]@{ Id = 'descent'; Label = 'Descent album' })
$physical = @([PSCustomObject]@{ Id = 'DESCENT'; Label = 'Descent CD' })
Assert-ThrowsLike {
    Assert-DxxUniqueFingerprintSourceIds -Sources $album -ReservedSources $physical
} '*ID collision*'

$composed = "Caf$([char]0x00e9)"
$decomposed = "Cafe$([char]0x0301)"
if ((Get-DxxPortableSourceNameKey $composed) -ne (Get-DxxPortableSourceNameKey $decomposed)) {
    throw 'Unicode normalization did not produce one portable key'
}
if ((Get-DxxPortableSourceNameKey 'Album') -ne (Get-DxxPortableSourceNameKey 'album. ')) {
    throw 'Case and trailing separators did not produce one portable key'
}

$publisher = Get-Content (Join-Path $repoRoot 'game_data\update_known_discs_albums.ps1') -Raw
if ($publisher -notmatch 'IsAmbiguous' -or $publisher -notmatch 'ambiguousLookup') {
    throw 'Album publication does not exclude ambiguous fingerprint identities'
}

$discRaw = Get-Content (Join-Path $repoRoot 'android\app\src\main\assets\known_discs.jsonc') -Raw
$albumRaw = Get-Content (Join-Path $repoRoot 'android\app\src\main\assets\known_albums.jsonc') -Raw
$stripJsonc = {
    param([string]$Raw)
    return ($Raw -replace '//[^\r\n]*', '' -replace '/\*[\s\S]*?\*/', '' -replace ',(\s*[}\]])', '$1')
}
$discDb = (& $stripJsonc $discRaw) | ConvertFrom-Json
$albumDb = (& $stripJsonc $albumRaw) | ConvertFrom-Json
$discSources = @($discDb.discs | ForEach-Object {
        [PSCustomObject]@{ Id = [string]$_.id; Label = [string]$_.label }
    })
$albumSources = @($albumDb.albums | ForEach-Object {
        [PSCustomObject]@{ Id = [string]$_.id; Label = [string]$_.label }
    })
Assert-DxxUniqueFingerprintSourceIds -Sources $albumSources -ReservedSources $discSources

$fingerprints = @{}
foreach ($albumEntry in $albumDb.albums) {
    foreach ($track in @($albumEntry.tracks | Where-Object type -eq 'audio')) {
        if (-not $track.chromaprint) { continue }
        $candidate = [PSCustomObject]@{
            Identity = "$($albumEntry.id)|$($track.track)|$($track.name)"
            DurationMs = [int]$track.duration_ms
        }
        $key = [string]$track.chromaprint
        if ($fingerprints.ContainsKey($key)) {
            foreach ($prior in $fingerprints[$key]) {
                $ratio = [double]$candidate.DurationMs / [double]$prior.DurationMs
                if ($candidate.Identity -ne $prior.Identity -and $ratio -ge 0.9 -and $ratio -le 1.1) {
                    throw "Published ambiguous fingerprint identities '$($prior.Identity)' and '$($candidate.Identity)'"
                }
            }
            $fingerprints[$key] += $candidate
        } else {
            $fingerprints[$key] = @($candidate)
        }
    }
}

Write-Host 'Fingerprint source identity tests passed'
