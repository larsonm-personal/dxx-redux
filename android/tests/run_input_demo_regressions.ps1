#!/usr/bin/env pwsh
param(
    [string]$DemoRoot,
    [string[]]$DemoFileName,
    [string]$ResultArchiveRoot,
    [string]$ReferenceResultRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('all', 'd1', 'd2')]
    [string]$RecordedGame = 'all',
    [ValidateSet('prompt', 'realtime', 'accelerated')]
    [string]$Mode = 'accelerated',
    [int]$TimeoutSeconds = 300,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$Pilot,
    [ValidateSet('headless', 'graphics')]
    [string]$RunMode = '',
    [switch]$NoRender,
    [switch]$ReuseSandbox,
    [switch]$KeepSandbox,
    [switch]$D1InD2,
    [switch]$ListOnly,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
$wrapper = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'

function Get-RegressionBuildGames {
    param(
        [object[]]$Demos,
        [string]$RequestedGame,
        [switch]$UseD1InD2
    )

    if ($UseD1InD2) {
        return @('d2')
    }

    if ($RequestedGame -ne 'auto') {
        return @($RequestedGame)
    }

    return @($Demos | ForEach-Object { Get-InputDemoRecordedGameName -DemoPath $_.FullName } | Sort-Object -Unique)
}

function Get-EffectiveRecordedGameFilter {
    param(
        [string]$RequestedGame,
        [string]$RequestedRecordedGame,
        [switch]$UseD1InD2
    )

    if ($UseD1InD2) {
        if ($RequestedRecordedGame -eq 'd2') {
            throw '-D1InD2 can only replay D1-recorded demos'
        }
        if ($RequestedGame -eq 'd1') {
            throw '-D1InD2 replays through the D2 engine; use -Game d2 or -Game auto'
        }
        return 'd1'
    }

    if ($RequestedRecordedGame -ne 'all') {
        return $RequestedRecordedGame
    }

    if ($RequestedGame -in @('d1', 'd2')) {
        return $RequestedGame
    }

    return 'all'
}

function Select-RegressionDemos {
    param(
        [object[]]$Demos,
        [string]$RecordedGameFilter
    )

    if ($RecordedGameFilter -eq 'all') {
        return @($Demos)
    }

    return @($Demos | Where-Object {
            (Get-InputDemoRecordedGameName -DemoPath $_.FullName) -eq $RecordedGameFilter
        })
}

function Select-NamedRegressionDemos {
    param(
        [object[]]$Demos,
        [string[]]$RequestedFileNames
    )

    if (-not $RequestedFileNames -or $RequestedFileNames.Count -eq 0) {
        return @($Demos)
    }

    $requested = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($fileName in $RequestedFileNames) {
        if ([System.IO.Path]::GetFileName($fileName) -ne $fileName) {
            throw "-DemoFileName accepts file names, not paths: $fileName"
        }
        $null = $requested.Add($fileName)
    }

    $selected = @($Demos | Where-Object { $requested.Contains($_.Name) })
    $found = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($demo in $selected) {
        $null = $found.Add($demo.Name)
    }
    $missing = @($requested | Where-Object { -not $found.Contains($_) } | Sort-Object)
    if ($missing.Count -gt 0) {
        throw "Requested regression demo(s) not found after game filtering: $($missing -join ', ')"
    }

    return @($selected)
}

if (-not $DemoRoot) {
    $DemoRoot = Join-Path $repoRoot 'android\regression_demos'
}
if (-not (Test-Path -LiteralPath $DemoRoot)) {
    throw "Regression demo directory not found: $DemoRoot"
}
$resolvedDemoRoot = (Resolve-Path -LiteralPath $DemoRoot).Path
$allDemos = @(Get-ChildItem -LiteralPath $resolvedDemoRoot -Recurse -Filter '*.dximdemo' -File | Sort-Object FullName)
$effectiveRecordedGame = Get-EffectiveRecordedGameFilter -RequestedGame $Game -RequestedRecordedGame $RecordedGame -UseD1InD2:$D1InD2
$demos = @(Select-RegressionDemos -Demos $allDemos -RecordedGameFilter $effectiveRecordedGame)
$demos = @(Select-NamedRegressionDemos -Demos $demos -RequestedFileNames $DemoFileName)

if ($ReferenceResultRoot) {
    foreach ($demo in $demos) {
        $referencePath = Join-Path $ReferenceResultRoot "$($demo.Name).actual.json"
        if (-not (Test-Path -LiteralPath $referencePath -PathType Leaf)) {
            Write-Host "RESULT: SKIP (primary replay result unavailable: $referencePath)"
            exit 0
        }
    }
}

if ($ListOnly) {
    if ($demos.Count -eq 0) {
        Write-Host 'No regression demos found'
        exit 0
    }
    foreach ($demo in $demos) {
        Write-Host ([System.IO.Path]::GetRelativePath($repoRoot, $demo.FullName))
    }
    exit 0
}

if ($demos.Count -eq 0) {
    $filterDescription = if ($effectiveRecordedGame -eq 'all') { 'any recorded game' } else { "recorded game '$effectiveRecordedGame'" }
    throw "No .dximdemo files found under $resolvedDemoRoot for $filterDescription"
}

# Prompt user for run mode if not specified; list-only mode exits before this
# so noninteractive inventory checks do not hang
if (-not $RunMode) {
    Write-Host ''
    Write-Host 'Select demo run mode:' -ForegroundColor Cyan
    Write-Host '  1) Headless (default, faster)' -ForegroundColor White
    Write-Host '  2) Graphics (full game binary)' -ForegroundColor White
    Write-Host ''
    $choice = Read-Host 'Enter choice [1]'
    if (-not $choice) {
        $choice = '1'
    }
    switch ($choice) {
        '1' { $RunMode = 'headless' }
        '2' { $RunMode = 'graphics' }
        default {
            Write-Host 'Invalid choice' -ForegroundColor Red
            exit 1
        }
    }
    Write-Host "Using: $RunMode mode" -ForegroundColor Green
    Write-Host ''
}

$buildGames = Get-RegressionBuildGames -Demos $demos -RequestedGame $Game -UseD1InD2:$D1InD2
foreach ($buildGame in $buildGames) {
    $preferHeadless = ($RunMode -eq 'headless') -and -not $D1InD2
    Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $buildGame -PreferHeadlessConsole:$preferHeadless
}

$pwsh = Get-RegressionCurrentPwshPath
$failures = New-Object System.Collections.Generic.List[string]
$batchStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

for ($index = 0; $index -lt $demos.Count; $index++) {
    $demo = $demos[$index]
    $relativeDemo = [System.IO.Path]::GetRelativePath($repoRoot, $demo.FullName)
    Write-Host ''
    Write-Host ("[{0}/{1}] {2}" -f ($index + 1), $demos.Count, $relativeDemo)

    $effectiveGame = if ($D1InD2) { 'd2' } else { $Game }
    $args = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $wrapper,
        '-DemoPath', $demo.FullName,
        '-Game', $effectiveGame,
        '-Mode', $Mode,
        '-TimeoutSeconds', [string]$TimeoutSeconds
    )
    if ($D1InD2) {
        $args += '-D1InD2'
    }
    if ($RunMode -eq 'headless' -and -not $D1InD2) {
        $args += '-PreferHeadlessConsole'
    }
    if ($RunMode -eq 'headless' -and $D1InD2 -and -not $NoRender) {
        $args += '-NoRender'
    }
    if ($DataDir) {
        $args += @('-DataDir', $DataDir)
    }
    if ($Pilot) {
        $args += @('-Pilot', $Pilot)
    }
    if ($NoRender) {
        $args += '-NoRender'
    }
    if ($ReuseSandbox) {
        $args += '-ReuseSandbox'
    }
    if ($KeepSandbox) {
        $args += '-KeepSandbox'
    }
    if ($ResultArchiveRoot) {
        $args += @('-ResultCopyPath', (Join-Path $ResultArchiveRoot "$($demo.Name).actual.json"))
    }
    if ($ReferenceResultRoot) {
        $args += @('-ReferenceResultPath', (Join-Path $ReferenceResultRoot "$($demo.Name).actual.json"))
    }

    & $pwsh @args
    if ($LASTEXITCODE -ne 0) {
        $failures.Add($relativeDemo)
        if ($StopOnFirstFailure) {
            break
        }
    }
}

Write-Host ''
$batchStopwatch.Stop()
Write-Host ("Elapsed: {0}s" -f [Math]::Round($batchStopwatch.Elapsed.TotalSeconds, 3))
if ($failures.Count -gt 0) {
    Write-Host 'RESULT: FAIL' -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "Failed: $failure"
    }
    exit 1
}

Write-Host ("RESULT: PASS ({0} regression demo(s))" -f $demos.Count) -ForegroundColor Green
exit 0
