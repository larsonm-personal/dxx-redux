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
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
. (Join-Path $PSScriptRoot 'input_demo_game_data.ps1')

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
                Exe = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd1'
                Name = 'd1'
                Mission = 'd1'
                TitleArg = '-notitles'
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
                TitleArg = '-nomovies'
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

    return Resolve-InputDemoDataDir `
        -RepoRoot $repoRoot `
        -Config $Config `
        -RequestedDataDir $DataDir `
        -Purpose 'runtime smoke test'
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
    $resolvedDataDir = Resolve-SmokeDataDir -Config $config
    $fixtureDir = Join-Path $outRoot $GameName
    $demoPath = Join-Path $fixtureDir 'smoke.dximdemo'
    $actualResultDir = Join-Path $outRoot "results\$GameName"
    $actualResultPath = Join-Path $actualResultDir 'result.actual.json'
    $actualStatePath = $demoPath + '.actual_state.jsonl'
    $actualRngTracePath = $demoPath + '.actual_rngtrace.jsonl'

    if (-not (Test-Path $config.Exe)) {
        throw "Built executable not found: $($config.Exe)"
    }
    New-Fixture -GameName $GameName -Config $config -FixtureDir $fixtureDir
    $sandboxExe = New-LaunchSandbox -GameName $GameName -Config $config
    New-Item -ItemType Directory -Path $actualResultDir -Force | Out-Null
    if (Test-Path -LiteralPath $actualResultPath) {
        Remove-Item -LiteralPath $actualResultPath -Force
    }
    $launchArgs = @(
        '-hogdir', $resolvedDataDir,
        '-window',
        $config.TitleArg,
        '-nomusic',
        '-nosound',
        '-inputdemo-replay', $demoPath,
        '-inputdemo-actual-result', $actualResultPath,
        '-inputdemo-state-log', $actualStatePath,
        '-inputdemo-rng-trace', $actualRngTracePath
    )
    Write-Host "SMOKE $GameName sandbox=$sandboxExe data=$resolvedDataDir fixture=$fixtureDir"
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
    if (-not (Test-Path -LiteralPath $actualStatePath)) {
        throw "$GameName replay smoke did not write a state trace"
    }
    if (-not (Select-String -LiteralPath $actualStatePath -Pattern '"type":"frame_state"' -Quiet)) {
        throw "$GameName replay smoke state trace is missing frame_state records"
    }
    if (-not (Test-Path -LiteralPath $actualRngTracePath)) {
        throw "$GameName replay smoke did not write an rng trace"
    }
    if (-not (Select-String -LiteralPath $actualRngTracePath -Pattern '"type":"meta"' -Quiet)) {
        throw "$GameName replay smoke rng trace is missing a meta record"
    }
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
    Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $gameName
}
foreach ($gameName in $games) {
    Invoke-ReplaySmoke -GameName $gameName
}
