$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/fingerprint_config.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $repoRoot 'android/helpers/headless_process_pool.ps1')

$testRoot = Join-Path $repoRoot ('android/temp/fingerprint_threshold/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $testRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

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

    $execution = @{ Result = $null }
    $task = [pscustomobject]@{ FilePath = $matchExe; Arguments = $MatcherArguments; WorkingDirectory = $repoRoot; TimeoutSeconds = 30 }
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($task, $result)
        $execution.Result = $result
    }
    if ($execution.Result.TimedOut) { throw 'Fingerprint matcher timed out' }
    return [pscustomobject]@{ ExitCode = $execution.Result.ExitCode; Output = $execution.Result.StandardOutput + $execution.Result.StandardError }
}

try {
    $canonicalPath = Join-Path $repoRoot 'android\app\src\main\assets\fingerprint_config.jsonc'
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

    $commentedConfig = @'
{
    "match_threshold": /* complete value */ 0.65,
    "duration_tolerance": 0.10,
    "note": "https://example.invalid/a//b,} /* literal */ quote \" and slash \\",
}
'@
    $commented = Get-DxxFingerprintMatchingConfig -Path (Write-Config -Name 'comments.jsonc' -Content $commentedConfig)
    if ($commented.MatchThreshold -ne 0.65 -or $commented.DurationTolerance -ne 0.10) {
        throw 'Valid comments and quoted comment markers changed fingerprint matching values'
    }
    foreach ($splitFraction in @('0/* gap */.65', '0.6/* gap */5', '6.5/* gap */e-1', '6.5e/* gap */-1')) {
        Assert-ConfigRejected -Name 'split_fraction.jsonc' `
            -Content ('{"match_threshold":' + $splitFraction + ',"duration_tolerance":0.10}')
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

    $buildDir = Join-Path $repoRoot 'android/tests/build'
    $matchExe = Resolve-RegressionBuildTool -Directory (Join-Path $buildDir 'Release') -BaseName fingerprint_match
    if (-not $matchExe) { $matchExe = Resolve-RegressionBuildTool -Directory $buildDir -BaseName fingerprint_match }
    if (-not $matchExe) { throw 'fingerprint_match is not built in android/tests/build' }
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

    $discDbPath = Join-Path $repoRoot 'android\app\src\main\assets\known_discs.jsonc'
    $discDb = Read-JsoncFile -Path $discDbPath
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
    $producerLock.Dispose()
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
$global:LASTEXITCODE = 0
