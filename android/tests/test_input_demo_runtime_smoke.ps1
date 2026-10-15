#!/usr/bin/env pwsh
param(
    [switch]$AllowUnverifiedAssets,
    [ValidateSet('both', 'd1', 'd2')]
    [string]$Game = 'both',
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 60,
    [Alias('HogDir')]
    [string]$DataDir,
    [ValidateSet('shareware', 'retail')]
    [string]$DataEdition = 'shareware',
    [string]$D1Exe,
    [string]$D2Exe,
    # Retail recordings must be supplied explicitly; shareware defaults to the public flight corpus
    [string]$D1DemoPath,
    [string]$D2DemoPath,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
. (Join-Path $PSScriptRoot 'input_demo_game_data.ps1')
. (Join-Path $repoRoot 'android/helpers/shareware_test_data.ps1')

$outRoot = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $repoRoot "temp/input_demo_runtime_smoke/$DataEdition" }

function Get-GameConfig {
    param([string]$Name)

    if ($DataEdition -eq 'shareware') {
        $config = Get-SharewareTestGameConfig -RepoRoot $repoRoot -Game $Name
        $config.Exe = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName $Name
        return $config
    }

    switch ($Name) {
        'd1' {
            return @{
                Exe = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd1'
                Name = 'd1'
                Mission = 'd1'
                RequiredFiles = @('DESCENT.HOG', 'DESCENT.PIG')
                RequiredHashes = @(
                    @{ File = 'DESCENT.HOG'; Sha256 = '83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052' },
                    @{ File = 'DESCENT.PIG'; Sha256 = '093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe' }
                )
                DefaultDataDirs = @(
                    (Join-RegressionPath $repoRoot 'game_data' 'extracted' 'd1 mac extracted'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp')
                )
            }
        }
        'd2' {
            return @{
                Exe = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd2'
                Name = 'd2'
                Mission = 'd2'
                RequiredFiles = @('DESCENT2.HOG', 'DESCENT2.HAM', 'GROUPA.PIG')
                RequiredHashes = @(
                    @{ File = 'DESCENT2.HOG'; Sha256 = 'f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703' },
                    @{ File = 'DESCENT2.HAM'; Sha256 = '5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d' },
                    @{ File = 'GROUPA.PIG'; Sha256 = 'facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b' }
                )
                DefaultDataDirs = @(
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp'),
                    (Join-RegressionPath $repoRoot 'game_data' 'extracted' 'descent 2 demo 1-0_extracted')
                )
            }
        }
    }

    throw "Unsupported game: $Name"
}

function Resolve-SmokeDataDir {
    param([hashtable]$Config)

    if ($DataEdition -eq 'shareware') {
        return Resolve-SharewareTestData -RepoRoot $repoRoot -Game $Config.Name -DataDir $DataDir
    }

    return Resolve-InputDemoDataDir `
        -RepoRoot $repoRoot `
        -Config $Config `
        -RequestedDataDir $DataDir `
        -Purpose 'runtime smoke test'
}

function Get-SmokeRecording {
    param([string]$GameName)

    $path = if ($GameName -eq 'd1') { $D1DemoPath } else { $D2DemoPath }
    if (-not $path) {
        if ($DataEdition -eq 'retail') {
            throw "Retail $GameName smoke requires -$($GameName.ToUpperInvariant())DemoPath with a real recording and its complete expected terminal result"
        }
        $path = Join-Path $PSScriptRoot "fixtures/public_level_replays/$GameName-public-flight.dximdemo"
    }
    $path = (Resolve-Path -LiteralPath $path).Path
    $records = @(Get-Content -LiteralPath $path | Where-Object { $_.Trim() -and -not $_.Trim().StartsWith('//') })
    if ($records.Count -lt 3) { throw "Incomplete smoke recording: $path" }
    $header = $records[0] | ConvertFrom-Json
    $trailer = $records[-1] | ConvertFrom-Json
    if ($header.type -cne 'header' -or $header.game -cne $GameName -or $header.frame_count -lt 1 -or
        $trailer.type -cne 'result' -or $trailer.result.frame_count -ne $header.frame_count) {
        throw "Smoke recording header or terminal result does not match $GameName`: $path"
    }
    return @{ Path = $path; Header = $header }
}

function Assert-SmokeTraces {
    param([object]$Header, [string]$StatePath, [string]$RngPath)

    $frameCount = 0
    $haveMetadata = $false
    foreach ($line in [IO.File]::ReadLines($StatePath)) {
        $record = $line | ConvertFrom-Json
        if (-not $haveMetadata) {
            if ($record.type -cne 'meta' -or $record.source -cne 'replay') { throw 'State trace lacks replay startup metadata' }
            foreach ($field in @('game', 'mission', 'level', 'difficulty', 'start_mode', 'frame_count')) {
                if ($record.$field -cne $Header.$field) { throw "State trace startup mismatch at $field" }
            }
            $haveMetadata = $true
        } elseif ($record.type -ceq 'frame_state') {
            if ($record.source -cne 'replay' -or $record.f -ne $frameCount) { throw 'State trace has missing or reordered frames' }
            ++$frameCount
        }
    }
    if (-not $haveMetadata -or $frameCount -ne $Header.frame_count) {
        throw "State trace frame count mismatch: expected $($Header.frame_count), actual $frameCount"
    }

    $eventCount = 0
    $declaredEvents = $null
    foreach ($line in [IO.File]::ReadLines($RngPath)) {
        $record = $line | ConvertFrom-Json
        if ($null -eq $declaredEvents) {
            if ($record.type -cne 'meta' -or $record.version -ne 1 -or $record.truncated -cne $false -or
                ($record.events -isnot [int] -and $record.events -isnot [long]) -or $record.events -lt 0) {
                throw 'RNG trace lacks complete, untruncated event metadata'
            }
            $declaredEvents = $record.events
        } else {
            if ($record.type -cnotin @('rand', 'srand') -or $record.seq -ne $eventCount) {
                throw 'RNG trace has malformed, missing or reordered events'
            }
            ++$eventCount
        }
    }
    if ($null -eq $declaredEvents -or $eventCount -ne $declaredEvents) {
        throw "Incomplete RNG trace: expected $declaredEvents events, actual $eventCount"
    }
}

function Invoke-ReplaySmoke {
    param([string]$GameName, [hashtable]$Recording)

    $config = Get-GameConfig $GameName
    $override = if ($GameName -eq 'd1') { $D1Exe } else { $D2Exe }
    if ($override) { $config.Exe = (Resolve-Path -LiteralPath $override).Path }
    if (-not (Test-Path -LiteralPath $config.Exe -PathType Leaf)) { throw "Built executable not found: $($config.Exe)" }
    $resolvedDataDir = Resolve-SmokeDataDir -Config $config
    $resultDir = Join-Path $outRoot "results/$GameName"
    New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
    $actualResultPath = Join-Path $resultDir 'result.actual.json'
    $actualStatePath = Join-Path $resultDir 'state.jsonl'
    $actualRngPath = Join-Path $resultDir 'rng.jsonl'
    $logPath = Join-Path $resultDir 'replay.log'
    $suffix = 'smoke-' + [guid]::NewGuid().ToString('N')
    $sandboxName = ([IO.Path]::GetFileNameWithoutExtension($Recording.Path) + '__' + $suffix) -replace '[^A-Za-z0-9_.-]', '_'
    $sandbox = Join-Path $repoRoot "temp/input_demo_runtime_wrapper/$GameName/$sandboxName"
    $pwsh = (Get-Process -Id $PID).Path
    Write-Host "SMOKE $GameName recording=$($Recording.Path) data=$resolvedDataDir"
    try {
        # The shared runner verifies the fixture result and rejects forced or nonzero exits
        & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') `
            -AllowUnverifiedAssets:$AllowUnverifiedAssets -DemoPath $Recording.Path -Game $GameName -DataDir $resolvedDataDir -DataEdition $DataEdition `
            -ExecutablePath $config.Exe -Runner visual -Mode accelerated -RenderProfile default `
            -TimeoutSeconds $TimeoutSeconds -StrictComparison -RequireCleanExit -KeepSandbox -SandboxSuffix $suffix `
            -ResultCopyPath $actualResultPath -StateLogPath $actualStatePath -RngLogPath $actualRngPath *> $logPath
        $exitCode = $LASTEXITCODE
        $diagnostics = [IO.File]::ReadAllText($logPath)
        foreach ($name in @('gamelog.txt', 'stdout.txt', 'stderr.txt')) {
            $source = Join-Path $sandbox $name
            if (Test-Path -LiteralPath $source) {
                Copy-Item -LiteralPath $source -Destination (Join-Path $resultDir $name) -Force
                $diagnostics += "`n" + [IO.File]::ReadAllText($source)
            }
        }
        if ($exitCode -ne 0) { throw "$GameName replay smoke failed with exit $exitCode`: $logPath" }
        if ($diagnostics -match 'AddressSanitizer|LeakSanitizer|runtime error:|Input demo replay[^\r\n]*(mismatch|stopped|failed|could not)') {
            throw "$GameName replay emitted failure diagnostics: $logPath"
        }
        # Trace metadata pins startup identity even when a Windows GUI binary has no stdout
        Assert-SmokeTraces -Header $Recording.Header -StatePath $actualStatePath -RngPath $actualRngPath
    } finally {
        if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
    }
    Write-Host "PASS $GameName frames=$($Recording.Header.frame_count)"
}

if (-not (Test-Path $outRoot)) {
    New-Item -ItemType Directory -Path $outRoot | Out-Null
}

$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
$recordings = @{}
# Resolve all required recordings before building or launching either game
foreach ($gameName in $games) { $recordings[$gameName] = Get-SmokeRecording -GameName $gameName }
foreach ($gameName in $games) {
    $override = if ($gameName -eq 'd1') { $D1Exe } else { $D2Exe }
    if (-not $override) {
        Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $gameName
    }
}
foreach ($gameName in $games) {
    Invoke-ReplaySmoke -GameName $gameName -Recording $recordings[$gameName]
}
