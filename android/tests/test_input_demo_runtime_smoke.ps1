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

function Write-SmokeConfig {
    param([string]$Path)

    $configText = @"
ResolutionX=640
ResolutionY=480
WindowMode=1
BorderlessWindow=0
GrabInput=0
"@

    Write-AsciiFile -Path $Path -Content $configText
}

function New-LaunchSandbox {
    param([string]$GameName, [hashtable]$Config)

    $sourceDir = Split-Path $Config.Exe -Parent
    $sandboxDir = Join-Path $outRoot "runtime\$GameName"
    $sandboxExe = Join-Path $sandboxDir (Split-Path $Config.Exe -Leaf)

    if (Test-Path -LiteralPath $sandboxDir) {
        Remove-Item -LiteralPath $sandboxDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $sandboxDir | Out-Null

    Copy-Item -LiteralPath $Config.Exe -Destination $sandboxExe
    Get-ChildItem -LiteralPath $sourceDir -File |
        Where-Object { $_.Extension -eq '.dll' } |
        Copy-Item -Destination $sandboxDir

    Write-SmokeConfig -Path (Join-Path $sandboxDir 'descent.cfg')

    return $sandboxExe
}

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

    $arch = if ($env:PROCESSOR_ARCHITECTURE) { $env:PROCESSOR_ARCHITECTURE } else { 'unknown' }

    if (Test-Path $FixtureDir) {
        Remove-Item -LiteralPath $FixtureDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $FixtureDir | Out-Null

    $demoText = @"
{"type":"header","version":2,"game":"$GameName","mission":"$($Config.Mission)","build_number":0,"git_version":"unknown","arch":"$arch","level":1,"difficulty":2,"start_mode":"new_level","rng_mode":"lcg_state","frame_count":3}
{"type":"frame","f":0,"ft":3276,"input":{"s":{"f":44}},"rng":{"s":100}}
{"type":"frame","f":1,"input":{"p":{"f1":1}},"rng":{"s":100}}
{"type":"frame","f":2,"input":{"s":{"f":0}},"rng":{"s":102}}
{"type":"result","result":{"version":2,"game":"$GameName","mission":"$($Config.Mission)","level":1,"difficulty":2,"frame_count":3}}
"@

    Write-AsciiFile (Join-Path $FixtureDir 'smoke.dximdemo') $demoText
}

function Get-ExpectedResultFromDemo {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Expected demo file not found: $Path"
    }
    $trailer = Get-Content -LiteralPath $Path |
        Where-Object { $_.Trim().Length -gt 0 } |
        Select-Object -Last 1
    $record = $trailer | ConvertFrom-Json
    if ($record.type -ne 'result') {
        throw "Demo file does not end with a result trailer: $Path"
    }
    return $record.result
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
    $demoPath = Join-Path $fixtureDir 'smoke.dximdemo'
    $actualResultPath = $demoPath + '.actual.json'

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
    $sandboxExe = New-LaunchSandbox -GameName $GameName -Config $config
    $launchArgs = @(
        '-hogdir', $DataDir,
        '-window',
        $config.TitleArg,
        '-nomusic',
        '-nosound',
        '-inputdemo-replay', $demoPath
    )
    Write-Host "SMOKE $GameName sandbox=$sandboxExe data=$DataDir fixture=$fixtureDir"
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $sandboxExe
    $startInfo.WorkingDirectory = $fixtureDir
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $false
    $startInfo.RedirectStandardError = $false
    $quotedArgs = $launchArgs | ForEach-Object {
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

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $actualResultPath) {
            break
        }
        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
    }

    if (-not (Test-Path -LiteralPath $actualResultPath)) {
        if (-not $process.HasExited) {
            try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
        }
        if ($process.HasExited -and $process.ExitCode -ne 0) {
            $argText = ($launchArgs | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
            throw "$GameName replay smoke exited with code $($process.ExitCode)`nRepro: $sandboxExe $argText"
        }
        try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
        throw "Timed out waiting for $GameName replay smoke result after $TimeoutSeconds seconds"
    }

    if (-not $process.HasExited) {
        try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
    }

    $expected = Get-ExpectedResultFromDemo $demoPath
    $actual = Get-JsonFile $actualResultPath
    foreach ($property in @('version', 'game', 'mission', 'level', 'difficulty', 'frame_count')) {
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
