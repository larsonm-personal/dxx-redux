$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\fingerprint_config.ps1')

$testRoot = Join-Path (Resolve-Path (Join-Path $repoRoot 'android\temp')).Path `
('fingerprint_threshold_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null

function Write-Config {
    param([string]$Name, [string]$Content)

    $path = Join-Path $testRoot $Name
    [IO.File]::WriteAllText($path, $Content, [Text.UTF8Encoding]::new($false))
    return $path
}

function Assert-ConfigRejected {
    param([string]$Name, [string]$Content)

    $rejected = $false
    try {
        Get-DxxFingerprintMatchingConfig -Path (Write-Config -Name $Name -Content $Content) | Out-Null
    } catch {
        $rejected = $true
    }
    if (-not $rejected) { throw "Invalid fingerprint configuration was accepted: $Name" }
}

function Invoke-Matcher {
    param([string[]]$MatcherArguments)

    $output = & $matchExe @MatcherArguments 2>&1 | Out-String
    return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
}

try {
    $canonicalPath = Join-Path $repoRoot 'android\app\src\main\assets\fingerprint_config.json5'
    $canonical = Get-DxxFingerprintMatchingConfig -Path $canonicalPath
    if ($canonical.MatchThreshold -ne 0.65) { throw 'Canonical match threshold is not 0.65' }

    foreach ($threshold in @(0.40, 0.65)) {
        $path = Write-Config -Name "valid_$threshold.json" `
            -Content "{`"match_threshold`":$threshold,`"duration_tolerance`":0.10}"
        $parsed = Get-DxxFingerprintMatchingConfig -Path $path
        if ($parsed.MatchThreshold -ne $threshold) {
            throw "Threshold changed while parsing: $threshold"
        }
    }

    $missingRejected = $false
    try {
        Get-DxxFingerprintMatchingConfig -Path (Join-Path $testRoot 'missing.json') | Out-Null
    } catch {
        $missingRejected = $true
    }
    if (-not $missingRejected) { throw 'Missing fingerprint configuration was accepted' }

    Assert-ConfigRejected -Name 'missing_threshold.json' `
        -Content '{"duration_tolerance":0.10}'
    Assert-ConfigRejected -Name 'malformed.json' -Content '{"match_threshold":'
    Assert-ConfigRejected -Name 'string.json' `
        -Content '{"match_threshold":"0.65","duration_tolerance":0.10}'
    Assert-ConfigRejected -Name 'trailing.json' `
        -Content '{"match_threshold":0.65,"duration_tolerance":0.10} trailing'
    Assert-ConfigRejected -Name 'nan.json' `
        -Content '{"match_threshold":"NaN","duration_tolerance":0.10}'
    Assert-ConfigRejected -Name 'infinity.json' `
        -Content '{"match_threshold":"Infinity","duration_tolerance":0.10}'

    $matchExe = Join-Path $repoRoot 'android\tests\build\Release\fingerprint_match.exe'
    if (-not (Test-Path -LiteralPath $matchExe -PathType Leaf)) {
        throw "fingerprint_match.exe not found: $matchExe"
    }
    $missingDb = Join-Path $testRoot 'missing-db.json'
    foreach ($threshold in @('0.40', '0.65')) {
        $result = Invoke-Matcher -MatcherArguments @($missingDb, $threshold, '0.10')
        if ($result.ExitCode -eq 0 -or $result.Output -notmatch 'Cannot open') {
            throw "Matcher did not accept strict threshold $threshold before checking the DB"
        }
    }
    $omitted = Invoke-Matcher -MatcherArguments @($missingDb)
    if ($omitted.ExitCode -eq 0 -or $omitted.Output -notmatch 'Usage:') {
        throw 'Matcher accepted an omitted threshold'
    }
    foreach ($threshold in @('0.65junk', 'nan', 'inf', 'Infinity', '0', '1.01')) {
        $result = Invoke-Matcher -MatcherArguments @($missingDb, $threshold, '0.10')
        if ($result.ExitCode -eq 0 -or $result.Output -notmatch 'Invalid threshold') {
            throw "Matcher accepted invalid threshold: $threshold"
        }
    }

    $discDbPath = Join-Path $repoRoot 'android\app\src\main\assets\known_discs.json5'
    $discDbText = Get-Content -LiteralPath $discDbPath -Raw
    $discDb = ($discDbText -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', '') | ConvertFrom-Json
    $fixtureTrack = $discDb.discs.tracks | Where-Object {
        $_.type -eq 'audio' -and $_.chromaprint -and $_.duration_ms
    } | Select-Object -First 1
    if (-not $fixtureTrack) { throw 'No checked-in audio fingerprint is available for matcher validation' }
    $emptyNameFixture = @([ordered]@{
            name = ''
            disc_id = 'unnamed-disc'
            track = 1
            duration_ms = $fixtureTrack.duration_ms
            chromaprint = $fixtureTrack.chromaprint
        })
    $emptyNameDb = Write-Config -Name 'empty_name.json' `
        -Content (ConvertTo-Json -InputObject $emptyNameFixture -Depth 3)
    $emptyNameResult = Invoke-Matcher -MatcherArguments @($emptyNameDb, '0.65', '0.10')
    if ($emptyNameResult.ExitCode -ne 0 -or $emptyNameResult.Output -notmatch 'Loaded 1 entries') {
        throw "Matcher rejected an unnamed physical CD track: $($emptyNameResult.Output)"
    }

    Write-Host 'fingerprint threshold tests passed'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
$global:LASTEXITCODE = 0
