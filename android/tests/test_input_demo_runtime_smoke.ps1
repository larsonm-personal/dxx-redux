#!/usr/bin/env pwsh
param(
    [ValidateSet('both', 'd1', 'd2')]
    [string]$Game = 'both',
    [int]$TimeoutSeconds = 60,
    [Alias('HogDir')]
    [string]$DataDir
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $DataDir) {
    $DataDir = Join-Path $repoRoot 'game_data_to_copy_to_emulator\temp'
}

$outRoot = Join-Path $repoRoot 'temp\input_demo_runtime_smoke'

function Write-AsciiFile {
    param(
        [string]$Path,
        [string]$Content
    )

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.Encoding]::ASCII)
}

function Get-GameConfig {
    param([string]$Name)

    switch ($Name) {
        'd1' {
            return @{
                Exe = Join-Path $repoRoot 'buildd1\main\dxx-redux-d1.exe'
                Mission = 'd1'
                TitleArg = '-notitles'
                RequiredFiles = @('DESCENT.HOG', 'DESCENT.PIG')
            }
        }
        'd2' {
            return @{
                Exe = Join-Path $repoRoot 'buildd2\main\dxx-redux-d2.exe'
                Mission = 'd2'
                TitleArg = '-nomovies'
                RequiredFiles = @('DESCENT2.HOG', 'DESCENT2.HAM', 'GROUPA.PIG')
            }
        }
    }

    throw "Unsupported game: $Name"
}

function New-Fixture {
    param(
        [string]$GameName,
        [hashtable]$Config,
        [string]$FixtureDir
    )

    if (Test-Path $FixtureDir) {
        Remove-Item -LiteralPath $FixtureDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $FixtureDir | Out-Null

    $demoText = @"
{
    "version": 1,
    "game": "$GameName",
    "mission": "$($Config.Mission)",
    "level": 1,
    "difficulty": 2,
    "start_mode": "new_level",
    "rng_mode": "lcg_state",
    "frame_count": 3,
    "streams": [
        {
            "player": 0,
            "input": "inputs.p0.jsonl",
            "rng": "rng.p0.jsonl"
        }
    ],
    "result": "result.json"
}
"@
    $inputsText = @"
{"f":0,"ft":3276,"s":{"f":44}}
{"f":1,"p":{"f1":1}}
{"f":2,"s":{"f":0}}
"@
    $rngText = @"
{"f":0,"n":2,"s":100}
{"f":2,"s":102}
"@
    $resultText = @"
{
  "v": 1,
  "g": "$GameName",
  "m": "$($Config.Mission)",
  "l": 1,
  "d": 2,
  "fr": 3
}
"@

    Write-AsciiFile (Join-Path $FixtureDir 'demo.json5') $demoText
    Write-AsciiFile (Join-Path $FixtureDir 'inputs.p0.jsonl') $inputsText
    Write-AsciiFile (Join-Path $FixtureDir 'rng.p0.jsonl') $rngText
    Write-AsciiFile (Join-Path $FixtureDir 'result.json') $resultText
}

function Get-JsonFile {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        throw "Expected JSON file not found: $Path"
    }
    return ([System.IO.File]::ReadAllText($Path) | ConvertFrom-Json)
}

function Invoke-ReplaySmoke {
    param([string]$GameName)

    $config = Get-GameConfig $GameName
    $fixtureDir = Join-Path $outRoot $GameName
    $actualResultPath = Join-Path $fixtureDir 'result.actual.json'
    $expectedResultPath = Join-Path $fixtureDir 'result.json'
    $demoPath = Join-Path $fixtureDir 'demo.json5'

    if (-not (Test-Path $config.Exe)) {
        throw "Built executable not found: $($config.Exe)"
    }
    if (-not (Test-Path $DataDir)) {
        throw "Game data directory not found: $DataDir"
    }
    foreach ($requiredFile in $config.RequiredFiles) {
        $requiredPath = Join-Path $DataDir $requiredFile
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "Missing required game data file for ${GameName}: $($requiredPath)"
        }
    }

    New-Fixture -GameName $GameName -Config $config -FixtureDir $fixtureDir
    $args = @(
        '-hogdir', $DataDir,
        '-pilot', 'inputdemo',
        '-use_players_dir',
        '-window',
        $config.TitleArg,
        '-nomusic',
        '-nosound',
        '-inputdemo-replay', $demoPath
    )
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $config.Exe
    $startInfo.WorkingDirectory = $fixtureDir
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $false
    $startInfo.RedirectStandardError = $false
    $quotedArgs = $args | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"' + ($_ -replace '"', '\"') + '"'
        } else {
            $_
        }
    }
    $startInfo.Arguments = $quotedArgs -join ' '
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if (-not $process) {
        throw "Failed to start $GameName replay smoke process"
    }

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
        throw "Timed out waiting for $GameName replay smoke after $TimeoutSeconds seconds"
    }

    if ($process.ExitCode -ne 0) {
        $argText = ($args | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
        throw "$GameName replay smoke exited with code $($process.ExitCode)`nRepro: $($config.Exe) $argText"
    }
    if (-not (Test-Path $actualResultPath)) {
        throw "$GameName replay smoke did not write result.actual.json"
    }

    $expected = Get-JsonFile $expectedResultPath
    $actual = Get-JsonFile $actualResultPath
    foreach ($property in @('v', 'g', 'm', 'l', 'd', 'fr')) {
        if ($expected.$property -ne $actual.$property) {
            throw "$GameName replay smoke mismatch at ${property}: expected $($expected.$property), actual $($actual.$property)"
        }
    }

    Write-Host "PASS $GameName"
}

if (-not (Test-Path $outRoot)) {
    New-Item -ItemType Directory -Path $outRoot | Out-Null
}

$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
foreach ($gameName in $games) {
    Invoke-ReplaySmoke -GameName $gameName
}
