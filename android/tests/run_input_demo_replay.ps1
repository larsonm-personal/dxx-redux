#!/usr/bin/env pwsh
param(
    [string]$DemoPath,
    [string]$SearchRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('prompt', 'realtime', 'accelerated')]
    [string]$Mode = 'prompt',
    [ValidateSet('auto', 'default', 'legacy-texmerge', 'compat-texture-formats', 'lowres-assets')]
    [string]$RenderProfile = 'auto',
    [int]$TimeoutSeconds = 300,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$Pilot,
    [switch]$NoRender,
    [switch]$PreferHeadlessConsole,
    [ValidateSet('auto', 'hide', 'show')]
    [string]$ReplayRobotLabels = 'auto',
    [switch]$ReplayDebugLog,
    [ValidateSet('auto', 'visual', 'fast', 'windowed-no-present', 'headless-console')]
    [string]$Runner = 'auto',
    [switch]$ReuseSandbox,
    [switch]$KeepSandbox,
    [switch]$ListOnly,
    [string]$StateLogPath,
    [switch]$TraceState,
    [switch]$CompareStateTrace,
    [switch]$ResolveDataDirOnly,
    [string]$RngLogPath,
    [switch]$TraceRng,
    [switch]$CompareRngTrace,
    [switch]$SkipExpectedChecks,
    [switch]$D1InD2,
    [switch]$D1InD2StartFromLevel,
    [switch]$AllowMissingActualResult,
    [ValidateSet(1, 2)]
    [int]$HeadlessConsoleOutput = 1,
    [Alias('RebuildBeforeRun')]
    [switch]$BuildBeforeRun,
    [switch]$RequireFreshBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
$outRoot = Join-Path $repoRoot 'temp\input_demo_runtime_wrapper'

function Write-AsciiFile {
    param(
        [string]$Path,
        [string]$Content
    )

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.Encoding]::ASCII)
}

function Write-ReplayConfig {
    param([string]$Path)

    $configText = @"
ResolutionX=640
ResolutionY=480
WindowMode=1
BorderlessWindow=0
GrabInput=0
VSync=0
FPSIndicator=0
"@

    Write-AsciiFile -Path $Path -Content $configText
}

function ConvertFrom-JsonLine {
    param([string]$Line)

    if (-not $Line) {
        throw 'Missing json line content'
    }
    return $Line.Trim() | ConvertFrom-Json -AsHashtable
}

function Test-DemoJsonRecordLine {
    param([string]$Line)

    if ($null -eq $Line) {
        return $false
    }
    $trimmed = $Line.Trim()
    return $trimmed.Length -gt 0 -and -not $trimmed.StartsWith('//')
}

function Get-RelativeRepoPath {
    param([string]$Path)

    return [System.IO.Path]::GetRelativePath($repoRoot, $Path)
}

function Test-PathUnderDirectory {
    param(
        [string]$Path,
        [string]$Directory
    )

    if (-not $Path -or -not $Directory) {
        return $false
    }

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $trimChars = @([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $fullDirectory = [System.IO.Path]::GetFullPath($Directory).TrimEnd($trimChars)
    $fullDirectory = $fullDirectory + [System.IO.Path]::DirectorySeparatorChar
    return $fullPath.StartsWith($fullDirectory, [System.StringComparison]::OrdinalIgnoreCase)
}

function Stop-ReplayProcess {
    param(
        [System.Diagnostics.Process]$Process
    )

    if (-not $Process) {
        return
    }
    try {
        $Process.Refresh()
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            $Process.WaitForExit(2000) | Out-Null
        }
    } catch {
        try { Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
}

function Stop-StaleReplayProcessesInDirectory {
    param([string]$Directory)

    if (-not $Directory) {
        return
    }

    $targets = @(Get-Process -ErrorAction SilentlyContinue |
            Where-Object {
                $_.ProcessName -like 'dxx-redux-d*' -and
                (Test-PathUnderDirectory -Path $_.Path -Directory $Directory)
            })

    foreach ($target in $targets) {
        Stop-ReplayProcess -Process $target
    }
}

function Resolve-AbsolutePath {
    param([string]$Path)

    if (-not $Path) {
        return $null
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function Invoke-StateTraceComparison {
    param(
        [string]$DemoPath,
        [string]$ActualPath,
        [switch]$Silent
    )

    $expectedDir = Split-Path -Path $ActualPath -Parent
    $expectedPath = Join-Path $expectedDir ([System.IO.Path]::GetFileNameWithoutExtension($DemoPath) + '.expected_state.jsonl')
    $pwsh = (Get-Process -Id $PID).Path
    $exportScript = Join-Path $PSScriptRoot 'export_input_demo_state_trace.ps1'
    $compareScript = Join-Path $PSScriptRoot 'compare_input_demo_state_trace.ps1'

    & $pwsh -NoProfile -ExecutionPolicy Bypass -File $exportScript -DemoPath $DemoPath -OutputPath $expectedPath
    if ($LASTEXITCODE -ne 0) {
        throw "Expected state trace export failed with exit code $LASTEXITCODE"
    }

    $compareOutput = & $pwsh -NoProfile -ExecutionPolicy Bypass -File $compareScript -ExpectedPath $expectedPath -ActualPath $ActualPath -CompareFrameMetadata 2>&1
    $compareExitCode = $LASTEXITCODE
    $mismatchLines = New-Object System.Collections.Generic.List[string]
    foreach ($line in $compareOutput) {
        $text = [string]$line
        if ($text) {
            if (-not $Silent) {
                Write-Host $text
            }
            if ($text -match '^frame=') {
                $mismatchLines.Add($text.Trim())
            }
        }
    }
    return @{
        ExpectedPath = $expectedPath
        ExitCode = $compareExitCode
        MismatchLines = $mismatchLines
    }
}

function Invoke-RngTraceComparison {
    param(
        [string]$DemoPath,
        [string]$ActualPath,
        [switch]$Silent
    )

    $expectedPath = [System.IO.Path]::GetFullPath($DemoPath + '.rngtrace.jsonl')
    $pwsh = (Get-Process -Id $PID).Path
    $compareScript = Join-Path $PSScriptRoot 'compare_input_demo_rng_trace.ps1'

    if (-not (Test-Path -LiteralPath $expectedPath)) {
        throw "Recorded rng trace not found: $expectedPath"
    }

    $compareOutput = & $pwsh -NoProfile -ExecutionPolicy Bypass -File $compareScript -ExpectedPath $expectedPath -ActualPath $ActualPath 2>&1
    if (-not $Silent) {
        foreach ($line in $compareOutput) {
            if ($line) {
                Write-Host ([string]$line)
            }
        }
    }
    return @{
        ExpectedPath = $expectedPath
        ExitCode = $LASTEXITCODE
    }
}

function Get-SearchRoots {
    param([string]$RequestedRoot)

    if ($RequestedRoot) {
        if (-not (Test-Path -LiteralPath $RequestedRoot)) {
            throw "Search root not found: $RequestedRoot"
        }
        return @((Resolve-Path -LiteralPath $RequestedRoot).Path)
    }

    $roots = @(
        (Join-Path $repoRoot 'android\regression_demos'),
        (Join-Path $repoRoot 'android\temp_game_logs'),
        (Join-Path $repoRoot 'temp')
    )
    return $roots | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique
}

function Get-DemoHeader {
    param([string]$Path)

    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if (-not (Test-DemoJsonRecordLine -Line $line)) {
            continue
        }
        $record = ConvertFrom-JsonLine $line
        if ($record.type -ne 'header') {
            throw "Demo file does not start with a header record: $Path"
        }
        return $record
    }
    throw "Demo file is missing a header record: $Path"
}

function Get-DemoCheckpointRecord {
    param([string]$Path)

    $recordIndex = 0
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if (-not (Test-DemoJsonRecordLine -Line $line)) {
            continue
        }
        $recordIndex++
        if ($recordIndex -ne 2) {
            continue
        }
        $record = ConvertFrom-JsonLine $line
        if ($record.type -eq 'checkpoint') {
            return $record
        }
        return $null
    }
    return $null
}

function Get-DemoResultRecord {
    param([string]$Path)

    $line = $null
    foreach ($candidate in [System.IO.File]::ReadLines($Path)) {
        if (Test-DemoJsonRecordLine -Line $candidate) {
            $line = $candidate
        }
    }
    if (-not $line) {
        throw "Demo file is missing a result record: $Path"
    }
    $record = ConvertFrom-JsonLine $line
    if ($record.type -ne 'result') {
        throw "Demo file does not end with a result record: $Path"
    }
    return $record.result
}

function Get-DemoApproximateReplayFps {
    param(
        [string]$Path,
        [int]$MaxFps
    )

    $totalFrameTime = [int64]0
    $count = 0
    $lastFrameTime = $null

    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        $trimmed = $line.Trim()
        if (-not $trimmed.StartsWith('{"type":"frame"')) {
            continue
        }
        $record = ConvertFrom-JsonLine $trimmed
        if ($record.ContainsKey('ft')) {
            $lastFrameTime = [int]$record.ft
        }
        if ($null -ne $lastFrameTime -and $lastFrameTime -gt 0) {
            $totalFrameTime += [int64]$lastFrameTime
            $count++
        }
    }

    if ($count -le 0 -or $totalFrameTime -le 0) {
        return 25
    }

    $fps = [int][Math]::Round((65536.0 * $count) / $totalFrameTime)
    if ($fps -lt 25) {
        $fps = 25
    }
    if ($fps -gt $MaxFps) {
        $fps = $MaxFps
    }
    return $fps
}

function Get-GameConfig {
    param([string]$Name)

    switch ($Name) {
        'd1' {
            return @{
                Name = 'd1'
                Exe = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd1'
                TitleArg = '-notitles'
                RequiredFiles = @('DESCENT.HOG', 'DESCENT.PIG')
                RequiredHashes = @(
                    @{ File = 'DESCENT.HOG'; Sha256 = '83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052' },
                    @{ File = 'DESCENT.PIG'; Sha256 = '093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe' }
                )
                DefaultDataDirs = @(
                    (Join-RegressionPath $repoRoot 'game_data' 'extracted' 'd1 mac extracted'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'data'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp')
                )
                MaxFps = 200
            }
        }
        'd2' {
            return @{
                Name = 'd2'
                Exe = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd2'
                TitleArg = '-nomovies'
                RequiredFiles = @('DESCENT2.HOG', 'DESCENT2.HAM', 'GROUPA.PIG')
                RequiredHashes = @(
                    @{ File = 'DESCENT2.HOG'; Sha256 = 'f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703' },
                    @{ File = 'DESCENT2.HAM'; Sha256 = '5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d' },
                    @{ File = 'GROUPA.PIG'; Sha256 = 'facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b' }
                )
                DefaultDataDirs = @(
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'data'),
                    (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp'),
                    (Join-RegressionPath $repoRoot 'game_data' 'extracted' 'descent 2 demo 1-0_extracted')
                )
                MaxFps = 1000
            }
        }
    }

    throw "Unsupported game: $Name"
}

function Get-D1InD2GameConfig {
    $config = Get-GameConfig -Name 'd2'
    $config.RequiredFiles = @(
        'DESCENT2.HOG',
        'DESCENT2.HAM',
        'GROUPA.PIG',
        'DESCENT.HOG',
        'DESCENT.PIG'
    )
    $config.RequiredHashes = @()
    $config.DefaultDataDirs = @(
        (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'temp'),
        (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator' 'data')
    ) + $config.DefaultDataDirs
    return $config
}

function Read-GameDataHashIndex {
    $indexFile = Join-RegressionPath $repoRoot 'game_data' 'game_data_index.txt'
    $generator = Join-RegressionPath $repoRoot 'game_data' 'generate_game_data_index.ps1'

    if (-not (Test-Path -LiteralPath $indexFile) -and (Test-Path -LiteralPath $generator)) {
        $pwsh = Get-RegressionCurrentPwshPath
        & $pwsh -NoProfile -ExecutionPolicy Bypass -File $generator | Out-Null
    }
    if (-not (Test-Path -LiteralPath $indexFile)) {
        return $null
    }

    $index = @{}
    foreach ($line in [System.IO.File]::ReadLines($indexFile)) {
        if ($line -match '^\s*(#|$)') {
            continue
        }
        $parts = $line -split '\s{2}', 2
        if ($parts.Count -ne 2) {
            continue
        }
        $path = Join-Path $repoRoot $parts[1]
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $index[$parts[0].ToLowerInvariant()] = (Resolve-Path -LiteralPath $path).Path
        }
    }
    return $index
}

function Get-IndexedDataDirCandidates {
    param([hashtable]$Config)

    if (-not $Config.ContainsKey('RequiredHashes')) {
        return @()
    }

    $index = Read-GameDataHashIndex
    if (-not $index) {
        return @()
    }

    $dirs = @()
    foreach ($entry in $Config.RequiredHashes) {
        $path = $index[$entry.Sha256.ToLowerInvariant()]
        if (-not $path) {
            return @()
        }
        $dirs += (Split-Path -Parent $path)
    }

    $uniqueDirs = @($dirs | Select-Object -Unique)
    if ($uniqueDirs.Count -eq 1) {
        return $uniqueDirs
    }
    return @()
}

function Get-CaseInsensitiveChildFile {
    param(
        [string]$Directory,
        [string]$Name
    )

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return $null
    }
    return Get-ChildItem -LiteralPath $Directory -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } |
        Select-Object -First 1
}

function Get-DiscoveredDataDirCandidates {
    param([hashtable]$Config)

    $firstRequired = $Config.RequiredFiles[0]
    $roots = @(
        (Join-RegressionPath $repoRoot 'game_data_to_copy_to_emulator'),
        (Join-RegressionPath $repoRoot 'game_data' 'extracted'),
        (Join-RegressionPath $repoRoot 'game_data' 'CD images'),
        (Join-RegressionPath $repoRoot 'game_data' 'gog installers')
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Select-Object -Unique

    $dirs = New-Object System.Collections.Generic.List[string]
    foreach ($root in $roots) {
        Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name.Equals($firstRequired, [System.StringComparison]::OrdinalIgnoreCase) } |
            ForEach-Object {
                if (-not $dirs.Contains($_.DirectoryName)) {
                    $dirs.Add($_.DirectoryName)
                }
            }
    }
    return @($dirs | Sort-Object)
}

function Get-HeadlessConsoleExe {
    param([string]$GameName)

    switch ($GameName) {
        'd2' {
            return Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName 'd2' -PreferHeadlessConsole
        }
    }

    return $null
}

function Resolve-DemoGame {
    param(
        [hashtable]$Header,
        [string]$RequestedGame
    )

    if ($RequestedGame -ne 'auto') {
        return $RequestedGame
    }
    if (-not $Header.ContainsKey('game') -or [string]::IsNullOrWhiteSpace([string]$Header.game)) {
        throw 'Demo header is missing game metadata'
    }
    return [string]$Header.game
}

function Test-DataDirMatchesGame {
    param(
        [string]$Path,
        [hashtable]$Config
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    foreach ($requiredFile in $Config.RequiredFiles) {
        if (-not (Get-CaseInsensitiveChildFile -Directory $Path -Name $requiredFile)) {
            return $false
        }
    }
    return $true
}

function Resolve-DataDir {
    param(
        [hashtable]$Config,
        [string]$RequestedDataDir
    )

    $candidates = @()
    if ($RequestedDataDir) {
        $candidates += $RequestedDataDir
    }
    $candidates += Get-IndexedDataDirCandidates -Config $Config
    $candidates += $Config.DefaultDataDirs

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-DataDirMatchesGame -Path $candidate -Config $Config) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $candidates += Get-DiscoveredDataDirCandidates -Config $Config
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-DataDirMatchesGame -Path $candidate -Config $Config) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $missingSummary = foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-Path -LiteralPath $candidate) {
            $missing = @()
            foreach ($requiredFile in $Config.RequiredFiles) {
                if (-not (Get-CaseInsensitiveChildFile -Directory $candidate -Name $requiredFile)) {
                    $missing += $requiredFile
                }
            }
            if ($missing.Count -gt 0) {
                "${candidate} missing: $($missing -join ', ')"
            }
        } else {
            "${candidate} missing: directory not found"
        }
    }
    throw "Could not find a valid data dir for $($Config.Name)`n$($missingSummary -join "`n")"
}

function New-LaunchSandbox {
    param(
        [hashtable]$Config,
        [string]$SandboxName,
        [switch]$ReuseSandbox,
        [switch]$SkipExecutableCopy
    )

    $safeName = ($SandboxName -replace '[^A-Za-z0-9_.-]', '_')
    $sandboxDir = Join-Path $outRoot "$($Config.Name)\$safeName"
    $sandboxExe = $null

    Stop-StaleReplayProcessesInDirectory -Directory $sandboxDir

    if ((Test-Path -LiteralPath $sandboxDir) -and -not $ReuseSandbox) {
        Remove-Item -LiteralPath $sandboxDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $sandboxDir -Force | Out-Null

    if (-not $SkipExecutableCopy) {
        if (-not (Test-Path -LiteralPath $Config.Exe)) {
            throw "Built executable not found: $($Config.Exe)"
        }

        $sourceDir = Split-Path $Config.Exe -Parent
        $sandboxExe = Join-Path $sandboxDir (Split-Path $Config.Exe -Leaf)

        Copy-Item -LiteralPath $Config.Exe -Destination $sandboxExe -Force
        Get-ChildItem -LiteralPath $sourceDir -File |
            Where-Object { $_.Extension -eq '.dll' } |
            Copy-Item -Destination $sandboxDir -Force
    }

    Write-ReplayConfig -Path (Join-Path $sandboxDir 'descent.cfg')

    return @{
        Directory = $sandboxDir
        Exe = $sandboxExe
    }
}

function Test-CanUseHeadlessConsoleRunner {
    param(
        [string]$ResolvedGame,
        [hashtable]$Header,
        [hashtable]$LaunchMode,
        [string]$Pilot,
        [switch]$D1InD2
    )

    if ($D1InD2) {
        return $false
    }
    if ($Pilot) {
        return $false
    }
    if ($ResolvedGame -ne 'd2') {
        return $false
    }
    if ($LaunchMode.Name -ne 'accelerated') {
        return $false
    }
    if (-not $Header.ContainsKey('start_mode') -or [string]$Header.start_mode -ne 'save_checkpoint') {
        return $false
    }

    $headlessExe = Get-HeadlessConsoleExe -GameName $ResolvedGame
    return $headlessExe -and (Test-Path -LiteralPath $headlessExe)
}

function Resolve-ReplayRunnerSelection {
    param(
        [string]$RequestedRunner,
        [string]$ResolvedGame,
        [hashtable]$Header,
        [hashtable]$LaunchMode,
        [string]$Pilot,
        [switch]$NoRender,
        [switch]$PreferHeadlessConsole,
        [switch]$D1InD2
    )

    $headlessAvailable = Test-CanUseHeadlessConsoleRunner -ResolvedGame $ResolvedGame -Header $Header -LaunchMode $LaunchMode -Pilot $Pilot -D1InD2:$D1InD2

    switch ($RequestedRunner) {
        'visual' {
            if ($NoRender) {
                throw '-Runner visual cannot be combined with -NoRender'
            }
            if ($PreferHeadlessConsole) {
                throw '-Runner visual cannot be combined with -PreferHeadlessConsole'
            }
            return @{
                UseHeadlessConsole = $false
                UseNoRender = $false
                Name = 'visual-windowed'
                Description = 'full game binary with normal window, event, and render path'
                Selection = 'explicit'
            }
        }
        'windowed-no-present' {
            if ($PreferHeadlessConsole) {
                throw '-Runner windowed-no-present cannot be combined with -PreferHeadlessConsole'
            }
            return @{
                UseHeadlessConsole = $false
                UseNoRender = $true
                Name = 'fast-windowed-no-present'
                Description = 'full game binary with draw/present bypassed via -inputdemo-norender'
                Selection = 'explicit'
            }
        }
        'headless-console' {
            if ($D1InD2) {
                throw '-Runner headless-console cannot be combined with -D1InD2'
            }
            if ($NoRender) {
                throw '-Runner headless-console cannot be combined with -NoRender'
            }
            if (-not $headlessAvailable) {
                throw '-Runner headless-console is unavailable for this replay (requires d2 accelerated checkpoint replay with headless build present)'
            }
            return @{
                UseHeadlessConsole = $true
                UseNoRender = $false
                Name = 'fast-headless-console'
                Description = 'headless replay binary with dedicated console main'
                Selection = 'explicit'
            }
        }
        'fast' {
            if ($headlessAvailable -and -not $NoRender) {
                return @{
                    UseHeadlessConsole = $true
                    UseNoRender = $false
                    Name = 'fast-headless-console'
                    Description = 'headless replay binary with dedicated console main'
                    Selection = 'explicit'
                }
            }
            return @{
                UseHeadlessConsole = $false
                UseNoRender = $true
                Name = 'fast-windowed-no-present'
                Description = 'full game binary with draw/present bypassed via -inputdemo-norender'
                Selection = 'explicit'
            }
        }
        'auto' {
            if ($NoRender) {
                return @{
                    UseHeadlessConsole = $false
                    UseNoRender = $true
                    Name = 'fast-windowed-no-present'
                    Description = 'legacy -NoRender selection'
                    Selection = 'legacy'
                }
            }
            if ($PreferHeadlessConsole -and $headlessAvailable) {
                return @{
                    UseHeadlessConsole = $true
                    UseNoRender = $false
                    Name = 'fast-headless-console'
                    Description = 'legacy -PreferHeadlessConsole selection'
                    Selection = 'legacy'
                }
            }
            return @{
                UseHeadlessConsole = $false
                UseNoRender = $false
                Name = 'visual-windowed'
                Description = 'default visual replay path using the full game binary'
                Selection = 'auto-default'
            }
        }
    }

    throw "Unsupported runner selection: $RequestedRunner"
}

function Get-HeadlessConsoleLaunchArguments {
    param(
        [string]$ResolvedDataDir,
        [string]$ResolvedDemoPath,
        [string]$ActualResultPath,
        [string]$ResolvedStateLogPath,
        [string]$ResolvedRngLogPath,
        [switch]$ReplayDebugLog,
        [int]$HeadlessConsoleOutput
    )

    $launchParameters = @(
        '-hogdir', $ResolvedDataDir,
        '-inputdemo-replay', $ResolvedDemoPath,
        '-inputdemo-actual-result', $ActualResultPath
    )
    if ($resolvedStateLogPath) {
        $launchParameters += @('-inputdemo-state-log', $ResolvedStateLogPath)
    }
    if ($ResolvedRngLogPath) {
        $launchParameters += @('-inputdemo-rng-trace', $ResolvedRngLogPath)
    }
    if ($ReplayDebugLog) {
        $launchParameters += '-inputdemo-debug-log'
    }
    $launchParameters += @('-headless-console-output', [string]$HeadlessConsoleOutput)
    return $launchParameters
}

function Read-NumberedChoice {
    param(
        [string]$Prompt,
        [int]$OptionCount,
        [int]$DefaultChoice = 1
    )

    while ($true) {
        $choice = Read-Host $Prompt
        if ([string]::IsNullOrWhiteSpace($choice)) {
            return $DefaultChoice
        }

        $selected = 0
        if ([int]::TryParse($choice, [ref]$selected) -and $selected -ge 1 -and $selected -le $OptionCount) {
            return $selected
        }

        if ($OptionCount -eq 2) {
            Write-Host 'Enter 1 or 2'
        } else {
            Write-Host "Enter a number between 1 and $OptionCount"
        }
    }
}

function Get-RunnerPromptDefaults {
    param(
        [string]$RequestedRunner,
        [string]$RequestedMode,
        [string]$RequestedProfile
    )

    $effectiveMode = $RequestedMode
    $effectiveProfile = $RequestedProfile

    if ($RequestedRunner -eq 'headless-console') {
        if ($effectiveMode -eq 'prompt') {
            $effectiveMode = 'accelerated'
        }
        if ($effectiveProfile -eq 'auto') {
            $effectiveProfile = 'default'
        }
    }

    return @{
        Mode = $effectiveMode
        RenderProfile = $effectiveProfile
    }
}

function Get-LaunchMode {
    param(
        [string]$RequestedMode,
        [string]$Path,
        [hashtable]$Config
    )

    $realTimeFps = Get-DemoApproximateReplayFps -Path $Path -MaxFps $Config.MaxFps

    if ($RequestedMode -eq 'prompt') {
        Write-Host ''
        Write-Host 'Replay mode:'
        Write-Host "  [1] Real time     maxfps=$realTimeFps"
        Write-Host "  [2] Accelerated   maxfps=$($Config.MaxFps) + -nonicefps"
        while ($RequestedMode -eq 'prompt') {
            $choice = Read-NumberedChoice -Prompt 'Choose replay mode (1 or 2)' -OptionCount 2
            switch ($choice) {
                1 { $RequestedMode = 'realtime' }
                2 { $RequestedMode = 'accelerated' }
            }
        }
    }

    if ($RequestedMode -eq 'realtime') {
        return @{
            Name = 'realtime'
            MaxFps = $realTimeFps
            ExtraArgs = @()
        }
    }

    return @{
        Name = 'accelerated'
        MaxFps = $Config.MaxFps
        ExtraArgs = @('-nonicefps')
    }
}

function Get-RenderProfile {
    param(
        [string]$RequestedProfile,
        [string]$RequestedMode
    )

    if ($RequestedProfile -eq 'auto' -and $RequestedMode -eq 'prompt') {
        Write-Host ''
        Write-Host 'Render profile:'
        Write-Host '  [1] Default                  baseline OpenGL path'
        Write-Host '  [2] Legacy texmerge          adds -gl_oldtexmerge'
        Write-Host '  [3] Compat texture formats   disables legacy 4-bit/2-bit internal formats'
        Write-Host '  [4] Lowres assets            adds -lowresgraphics'
        while ($RequestedProfile -eq 'auto') {
            $choice = Read-NumberedChoice -Prompt 'Choose render profile (1-4)' -OptionCount 4
            switch ($choice) {
                1 { $RequestedProfile = 'default' }
                2 { $RequestedProfile = 'legacy-texmerge' }
                3 { $RequestedProfile = 'compat-texture-formats' }
                4 { $RequestedProfile = 'lowres-assets' }
            }
        }
    }

    if ($RequestedProfile -eq 'auto') {
        $RequestedProfile = 'default'
    }

    switch ($RequestedProfile) {
        'default' {
            return @{
                Name = 'default'
                Description = 'baseline OpenGL path'
                ExtraArgs = @()
            }
        }
        'legacy-texmerge' {
            return @{
                Name = 'legacy-texmerge'
                Description = 'forces legacy texmerge path'
                ExtraArgs = @('-gl_oldtexmerge')
            }
        }
        'compat-texture-formats' {
            return @{
                Name = 'compat-texture-formats'
                Description = 'disables legacy internal texture formats'
                ExtraArgs = @(
                    '-gl_intensity4_ok', '0',
                    '-gl_luminance4_alpha4_ok', '0',
                    '-gl_rgba2_ok', '0'
                )
            }
        }
        'lowres-assets' {
            return @{
                Name = 'lowres-assets'
                Description = 'forces lowres graphics pack'
                ExtraArgs = @('-lowresgraphics')
            }
        }
    }

    throw "Unsupported render profile: $RequestedProfile"
}

function Get-LaunchArguments {
    param(
        [hashtable]$Config,
        [string]$ResolvedDataDir,
        [string]$ResolvedDemoPath,
        [string]$ActualResultPath,
        [hashtable]$LaunchMode,
        [hashtable]$RenderProfileSelection,
        [string]$Pilot,
        [switch]$NoRender,
        [switch]$ShowReplayRobotLabels,
        [switch]$ReplayDebugLog,
        [switch]$D1InD2,
        [switch]$D1InD2StartFromLevel,
        [string]$ResolvedStateLogPath,
        [string]$ResolvedRngLogPath
    )

    $launchParameters = @(
        '-hogdir', $ResolvedDataDir,
        '-window',
        $Config.TitleArg,
        '-nomusic',
        '-nosound',
        '-maxfps', [string]$LaunchMode.MaxFps,
        '-inputdemo-replay', $ResolvedDemoPath,
        '-inputdemo-actual-result', $ActualResultPath
    )
    if ($LaunchMode.ExtraArgs.Count -gt 0) {
        $launchParameters += $LaunchMode.ExtraArgs
    }
    if ($RenderProfileSelection.ExtraArgs.Count -gt 0) {
        $launchParameters += $RenderProfileSelection.ExtraArgs
    }
    if ($NoRender) {
        $launchParameters += '-inputdemo-norender'
    }
    if ($Pilot) {
        $launchParameters += @('-pilot', $Pilot)
    }
    if ($ShowReplayRobotLabels) {
        $launchParameters += '-inputdemo-replay-labels'
    }
    if ($ReplayDebugLog) {
        $launchParameters += '-inputdemo-debug-log'
    }
    if ($D1InD2) {
        $launchParameters += '-inputdemo-d1-in-d2'
    }
    if ($D1InD2StartFromLevel) {
        $launchParameters += '-inputdemo-d1-in-d2-start-from-level'
    }
    if ($ResolvedStateLogPath) {
        $launchParameters += @('-inputdemo-state-log', $ResolvedStateLogPath)
    }
    if ($resolvedRngLogPath) {
        $launchParameters += @('-inputdemo-rng-trace', $ResolvedRngLogPath)
    }
    return $launchParameters
}

function Get-QuotedArgumentString {
    param([string[]]$Arguments)

    return (($Arguments | ForEach-Object {
                if ($_ -match '[\s"]') {
                    '"' + ($_ -replace '"', '\"') + '"'
                } else {
                    $_
                }
            }) -join ' ')
}

function Get-DemoCandidates {
    param([string]$RequestedRoot)

    $items = New-Object System.Collections.Generic.List[object]

    foreach ($root in (Get-SearchRoots -RequestedRoot $RequestedRoot)) {
        foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -Filter '*.dximdemo' -File -ErrorAction SilentlyContinue)) {
            $header = Get-DemoHeader -Path $file.FullName
            $items.Add([pscustomobject]@{
                    Path = $file.FullName
                    RelativePath = Get-RelativeRepoPath -Path $file.FullName
                    Game = if ($header.ContainsKey('game')) { [string]$header.game } else { '?' }
                    Mission = if ($header.ContainsKey('mission')) { [string]$header.mission } else { '?' }
                    Level = if ($header.ContainsKey('level')) { [int]$header.level } else { 0 }
                    StartMode = if ($header.ContainsKey('start_mode')) { [string]$header.start_mode } else { '?' }
                    FrameCount = if ($header.ContainsKey('frame_count')) { [int]$header.frame_count } else { 0 }
                    SizeKb = [Math]::Round($file.Length / 1KB, 1)
                    Modified = $file.LastWriteTime
                })
        }
    }

    return $items | Sort-Object -Property Modified, RelativePath -Descending
}

function Show-DemoCandidates {
    param([object[]]$Candidates)

    if (-not $Candidates -or $Candidates.Count -eq 0) {
        Write-Host 'No .dximdemo files found'
        return
    }

    for ($i = 0; $i -lt $Candidates.Count; $i++) {
        $item = $Candidates[$i]
        $formatArgs = @(
            ($i + 1)
            $item.Game
            $item.Mission
            $item.Level
            $item.StartMode
            $item.FrameCount
            $item.SizeKb
            $item.RelativePath
        )
        $line = '[{0}] {1} {2} lvl={3} {4} fr={5} {6}KB {7}' -f $formatArgs
        Write-Host $line
    }
}

function Select-DemoCandidate {
    param([object[]]$Candidates)

    if (-not $Candidates -or $Candidates.Count -eq 0) {
        throw 'No .dximdemo files are available to replay'
    }

    Show-DemoCandidates -Candidates $Candidates
    $index = Read-NumberedChoice -Prompt 'Choose demo number' -OptionCount $Candidates.Count
    return $Candidates[$index - 1]
}

function ConvertTo-DeepHashtableClone {
    param([hashtable]$Value)

    return ($Value | ConvertTo-Json -Depth 20 | ConvertFrom-Json -AsHashtable)
}

function Normalize-ExpectedResult {
    param(
        [hashtable]$Expected,
        [hashtable]$Header,
        [hashtable]$Checkpoint
    )

    $normalized = ConvertTo-DeepHashtableClone -Value $Expected
    if ($Header.ContainsKey('start_mode') -and $Header.start_mode -eq 'save_checkpoint' -and
        $normalized.ContainsKey('gt') -and $Checkpoint -and $Checkpoint.ContainsKey('start_gt')) {
        $normalized.gt = [int64]$normalized.gt - [int64]$Checkpoint.start_gt
    }
    return $normalized
}

function Normalize-D1InD2ExpectedResult {
    param(
        [hashtable]$Expected,
        [hashtable]$Actual
    )

    $normalized = ConvertTo-DeepHashtableClone -Value $Expected
    foreach ($key in @('game', 'mission')) {
        if ($normalized.ContainsKey($key) -and $Actual.ContainsKey($key)) {
            $normalized[$key] = $Actual[$key]
        }
    }
    return $normalized
}

function Get-TerminalExitExpectedSubset {
    param(
        [hashtable]$Expected,
        [hashtable]$Actual
    )

    $subset = [ordered]@{}
    foreach ($key in @('version', 'game', 'mission', 'level', 'difficulty', 'frame_count', 'game_time64')) {
        if ($Expected.ContainsKey($key)) {
            $subset[$key] = $Expected[$key]
        }
    }
    if ($Expected.ContainsKey('terminal_exit')) {
        $subset.terminal_exit = $Expected.terminal_exit
    } elseif ($Actual.ContainsKey('terminal_exit')) {
        $subset.terminal_exit = $Actual.terminal_exit
    }
    foreach ($key in @('player0', 'position')) {
        if ($Actual.ContainsKey($key)) {
            $subset[$key] = ConvertTo-DeepHashtableClone -Value $Actual[$key]
        }
    }
    if ($Expected.ContainsKey('level_summary')) {
        $subset.level_summary = ConvertTo-DeepHashtableClone -Value $Expected.level_summary
        if ($Actual.ContainsKey('level_summary') -and $Actual.level_summary -is [System.Collections.IDictionary] -and
            $Actual.level_summary.Contains('endlevel_completed')) {
            $subset.level_summary.endlevel_completed = $Actual.level_summary.endlevel_completed
        }
    }
    return $subset
}

function Format-CompareValue {
    param([object]$Value)

    if ($null -eq $Value) {
        return '<null>'
    }
    if ($Value -is [System.Collections.IDictionary] -or ($Value -is [System.Collections.IList] -and -not ($Value -is [string]))) {
        return ($Value | ConvertTo-Json -Compress -Depth 10)
    }
    return [string]$Value
}

function Compare-JsonDiff {
    param(
        [object]$Expected,
        [object]$Actual,
        [string]$Path = 'result'
    )

    $diffs = New-Object System.Collections.Generic.List[string]

    if ($Expected -is [System.Collections.IDictionary]) {
        if (-not ($Actual -is [System.Collections.IDictionary])) {
            $diffs.Add("${Path}: expected object, actual $(Format-CompareValue -Value $Actual)")
            return $diffs
        }
        foreach ($key in $Expected.Keys) {
            if (-not $Actual.Contains($key)) {
                $diffs.Add("${Path}.${key}: missing from actual result")
            }
        }
        foreach ($key in $Actual.Keys) {
            if (-not $Expected.Contains($key)) {
                $diffs.Add("${Path}.${key}: extra in actual result = $(Format-CompareValue -Value $Actual[$key])")
            }
        }
        foreach ($key in $Expected.Keys) {
            if (-not $Actual.Contains($key)) {
                continue
            }
            $nested = Compare-JsonDiff -Expected $Expected[$key] -Actual $Actual[$key] -Path "${Path}.${key}"
            foreach ($line in $nested) {
                $diffs.Add($line)
            }
        }
        return $diffs
    }

    if ($Expected -is [System.Collections.IList] -and -not ($Expected -is [string])) {
        if (-not ($Actual -is [System.Collections.IList])) {
            $diffs.Add("${Path}: expected array, actual $(Format-CompareValue -Value $Actual)")
            return $diffs
        }
        if ($Expected.Count -ne $Actual.Count) {
            $diffs.Add("${Path}: expected array length $($Expected.Count), actual $($Actual.Count)")
        }
        $maxShared = [Math]::Min($Expected.Count, $Actual.Count)
        for ($i = 0; $i -lt $maxShared; $i++) {
            $nested = Compare-JsonDiff -Expected $Expected[$i] -Actual $Actual[$i] -Path "${Path}[$i]"
            foreach ($line in $nested) {
                $diffs.Add($line)
            }
        }
        return $diffs
    }

    if ($Expected -ne $Actual) {
        $diffs.Add("${Path}: expected $(Format-CompareValue -Value $Expected), actual $(Format-CompareValue -Value $Actual)")
    }

    return $diffs
}

function Read-JsonFileAsHashtable {
    param([string]$Path)

    return ([System.IO.File]::ReadAllText($Path) | ConvertFrom-Json -AsHashtable)
}

function Get-TextFileLinesWithRetry {
    param(
        [string]$Path,
        [int]$RetryCount = 20,
        [int]$RetryDelayMs = 100
    )

    for ($attempt = 0; $attempt -lt $RetryCount; $attempt++) {
        try {
            $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            try {
                $reader = [System.IO.StreamReader]::new($stream)
                try {
                    $lines = New-Object System.Collections.Generic.List[string]
                    while ($null -ne ($line = $reader.ReadLine())) {
                        $lines.Add($line)
                    }
                    return $lines.ToArray()
                } finally {
                    $reader.Dispose()
                }
            } finally {
                $stream.Dispose()
            }
        } catch [System.IO.IOException] {
            if ($attempt -ge ($RetryCount - 1)) {
                throw
            }
            [System.Threading.Thread]::Sleep($RetryDelayMs)
        }
    }
}

function Test-ReplayUsedTerminalExitSubset {
    param(
        [string]$SandboxDirectory,
        [hashtable]$Expected,
        [hashtable]$Actual
    )

    $gamelogPath = Join-Path $SandboxDirectory 'gamelog.txt'
    if (Test-Path -LiteralPath $gamelogPath) {
        foreach ($line in Get-TextFileLinesWithRetry -Path $gamelogPath) {
            if ($line.Contains('Input demo replay level-exit:') -or $line.Contains('Input demo replay mine-exit:')) {
                return $true
            }
        }
    }

    if (-not $Expected -or -not $Actual) {
        return $false
    }

    $expectedTerminalExit = if ($Expected.ContainsKey('terminal_exit')) { [string]$Expected.terminal_exit } else { $null }
    $actualTerminalExit = if ($Actual.ContainsKey('terminal_exit')) { [string]$Actual.terminal_exit } else { $null }
    foreach ($terminalExit in @($expectedTerminalExit, $actualTerminalExit)) {
        if ($terminalExit -ne 'level_exit' -and $terminalExit -ne 'mine_exit') {
            continue
        }
        $terminalExitExpected = Get-TerminalExitExpectedSubset -Expected $Expected -Actual $Actual
        $terminalExitDiffs = Compare-JsonDiff -Expected $terminalExitExpected -Actual $Actual
        return ($terminalExitDiffs.Count -eq 0)
    }

    if (-not ($Expected.ContainsKey('level_summary') -and $Expected.level_summary -is [System.Collections.IDictionary])) {
        return $false
    }
    if (-not ($Actual.ContainsKey('level_summary') -and $Actual.level_summary -is [System.Collections.IDictionary])) {
        return $false
    }
    if (-not ($Expected.level_summary.Contains('endlevel_completed') -and $Actual.level_summary.Contains('endlevel_completed'))) {
        return $false
    }
    if ([bool]$Expected.level_summary.endlevel_completed -or -not [bool]$Actual.level_summary.endlevel_completed) {
        return $false
    }

    $rawDiffs = Compare-JsonDiff -Expected $Expected -Actual $Actual
    if ($rawDiffs.Count -eq 0) {
        return $false
    }
    foreach ($diff in $rawDiffs) {
        $separator = $diff.IndexOf(':')
        if ($separator -lt 0) {
            return $false
        }
        $path = $diff.Substring(0, $separator)
        if ($path -eq 'result.level_summary.endlevel_completed') {
            continue
        }
        if ($path -eq 'result.player0' -or $path.StartsWith('result.player0.')) {
            continue
        }
        if ($path -eq 'result.position' -or $path.StartsWith('result.position.')) {
            continue
        }
        return $false
    }

    $terminalExitExpected = Get-TerminalExitExpectedSubset -Expected $Expected -Actual $Actual
    $terminalExitDiffs = Compare-JsonDiff -Expected $terminalExitExpected -Actual $Actual
    return ($terminalExitDiffs.Count -eq 0)
}

function Wait-ForReplayResult {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ActualResultPath,
        [int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $ActualResultPath) {
            return @{ ResultReady = $true; Exited = $Process.HasExited; ExitCode = if ($Process.HasExited) { $Process.ExitCode } else { $null } }
        }
        if ($Process.HasExited) {
            return @{ ResultReady = $false; Exited = $true; ExitCode = $Process.ExitCode }
        }
        Start-Sleep -Milliseconds 100
    }

    return @{ ResultReady = $false; Exited = $Process.HasExited; ExitCode = if ($Process.HasExited) { $Process.ExitCode } else { $null } }
}

if (-not (Test-Path -LiteralPath $outRoot)) {
    New-Item -ItemType Directory -Path $outRoot -Force | Out-Null
}

$candidateList = Get-DemoCandidates -RequestedRoot $SearchRoot
if ($ListOnly) {
    Show-DemoCandidates -Candidates $candidateList
    exit 0
}

$selectedDemo = $null
if ($DemoPath) {
    if (-not (Test-Path -LiteralPath $DemoPath)) {
        throw "Demo file not found: $DemoPath"
    }
    $resolvedDemoPath = (Resolve-Path -LiteralPath $DemoPath).Path
} else {
    $selectedDemo = Select-DemoCandidate -Candidates $candidateList
    $resolvedDemoPath = $selectedDemo.Path
}

$header = Get-DemoHeader -Path $resolvedDemoPath
$recordedGame = Resolve-DemoGame -Header $header -RequestedGame 'auto'
if ($D1InD2) {
    if ($Game -eq 'd1') {
        throw '-D1InD2 runs the D2 executable; use -Game auto or -Game d2'
    }
    if ($recordedGame -ne 'd1') {
        throw "-D1InD2 requires a D1 recording, but this demo is '$recordedGame'"
    }
    if ($PreferHeadlessConsole) {
        throw '-D1InD2 cannot be combined with -PreferHeadlessConsole'
    }
    $resolvedGame = 'd2'
    $config = Get-D1InD2GameConfig
    if ($Runner -eq 'auto') {
        $Runner = 'windowed-no-present'
    }
} else {
    if ($D1InD2StartFromLevel) {
        throw '-D1InD2StartFromLevel requires -D1InD2'
    }
    $resolvedGame = Resolve-DemoGame -Header $header -RequestedGame $Game
    $config = Get-GameConfig -Name $resolvedGame
}
$resolvedDataDir = Resolve-DataDir -Config $config -RequestedDataDir $DataDir
if ($ResolveDataDirOnly) {
    if ($D1InD2) {
        Write-Host "Resolved d1-in-d2 data dir: $resolvedDataDir"
    } else {
        Write-Host "Resolved $resolvedGame data dir: $resolvedDataDir"
    }
    exit 0
}
$runnerPromptDefaults = Get-RunnerPromptDefaults -RequestedRunner $Runner -RequestedMode $Mode -RequestedProfile $RenderProfile
$renderProfileSelection = Get-RenderProfile -RequestedProfile $runnerPromptDefaults.RenderProfile -RequestedMode $runnerPromptDefaults.Mode
$launchMode = Get-LaunchMode -RequestedMode $runnerPromptDefaults.Mode -Path $resolvedDemoPath -Config $config
$shouldCompareStateTrace = $TraceState -or $CompareStateTrace
$shouldCompareRngTrace = $TraceRng -or $CompareRngTrace
$resolvedStateLogPath = $null
$resolvedRngLogPath = $null
if ($StateLogPath -or $TraceState -or $CompareStateTrace) {
    if ($StateLogPath) {
        $resolvedStateLogPath = Resolve-AbsolutePath -Path $StateLogPath
    } else {
        $resolvedStateLogPath = Join-Path (Join-Path $repoRoot 'temp\input_demo_state_traces') ([System.IO.Path]::GetFileNameWithoutExtension($resolvedDemoPath) + '.actual_state.jsonl')
        $resolvedStateLogPath = [System.IO.Path]::GetFullPath($resolvedStateLogPath)
    }
    $stateLogDirectory = Split-Path -Path $resolvedStateLogPath -Parent
    if ($stateLogDirectory -and -not (Test-Path -LiteralPath $stateLogDirectory)) {
        New-Item -ItemType Directory -Path $stateLogDirectory -Force | Out-Null
    }
}
if ($RngLogPath -or $TraceRng -or $CompareRngTrace) {
    if ($RngLogPath) {
        $resolvedRngLogPath = Resolve-AbsolutePath -Path $RngLogPath
    } else {
        $resolvedRngLogPath = Join-Path (Join-Path $repoRoot 'temp\input_demo_state_traces') ([System.IO.Path]::GetFileNameWithoutExtension($resolvedDemoPath) + '.actual_rngtrace.jsonl')
        $resolvedRngLogPath = [System.IO.Path]::GetFullPath($resolvedRngLogPath)
    }
    $rngLogDirectory = Split-Path -Path $resolvedRngLogPath -Parent
    if ($rngLogDirectory -and -not (Test-Path -LiteralPath $rngLogDirectory)) {
        New-Item -ItemType Directory -Path $rngLogDirectory -Force | Out-Null
    }
}
$runnerSelection = Resolve-ReplayRunnerSelection -RequestedRunner $Runner -ResolvedGame $resolvedGame -Header $header -LaunchMode $launchMode -Pilot $Pilot -NoRender:$NoRender -PreferHeadlessConsole:$PreferHeadlessConsole -D1InD2:$D1InD2
$useHeadlessConsole = $runnerSelection.UseHeadlessConsole
$effectiveNoRender = $runnerSelection.UseNoRender
$headlessQuietConsole = $useHeadlessConsole -and ($HeadlessConsoleOutput -eq 1)
$interactiveReplaySelection = ($Mode -eq 'prompt') -or (-not $DemoPath)
$showReplayRobotLabels = $false
$replayRobotLabelsIgnored = $false
$canShowReplayRobotLabels = ($resolvedGame -eq 'd2') -and -not $useHeadlessConsole -and -not $effectiveNoRender

if ($canShowReplayRobotLabels) {
    switch ($ReplayRobotLabels) {
        'show' {
            $showReplayRobotLabels = $true
        }
        'hide' {
            $showReplayRobotLabels = $false
        }
        default {
            if ($interactiveReplaySelection) {
                Write-Host ''
                Write-Host 'Replay robot labels:'
                Write-Host '  [1] No'
                Write-Host '  [2] Yes'
                $showReplayRobotLabels = (Read-NumberedChoice -Prompt 'Show replay robot labels (1 or 2)' -OptionCount 2) -eq 2
            }
        }
    }
} elseif ($ReplayRobotLabels -eq 'show') {
    $replayRobotLabelsIgnored = $true
}

$headlessConsoleExe = if ($useHeadlessConsole) { Get-HeadlessConsoleExe -GameName $resolvedGame } else { $null }
if ($useHeadlessConsole) {
    Ensure-InputDemoExecutable -RepoRoot $repoRoot -GameName $config.Name -ExecutablePath $headlessConsoleExe -Description 'Headless executable' -BuildBeforeRun:$BuildBeforeRun -RequireFreshBuild:$RequireFreshBuild
} else {
    Ensure-InputDemoExecutable -RepoRoot $repoRoot -GameName $config.Name -ExecutablePath $config.Exe -BuildBeforeRun:$BuildBeforeRun -RequireFreshBuild:$RequireFreshBuild
}
$sandbox = New-LaunchSandbox -Config $config -SandboxName ([System.IO.Path]::GetFileNameWithoutExtension($resolvedDemoPath)) -ReuseSandbox:$ReuseSandbox -SkipExecutableCopy:$useHeadlessConsole
$actualResultDirectory = Join-Path $sandbox.Directory 'results'
if (-not (Test-Path -LiteralPath $actualResultDirectory)) {
    New-Item -ItemType Directory -Path $actualResultDirectory -Force | Out-Null
}
$actualResultPath = Join-Path $actualResultDirectory 'result.actual.json'
$expectedResult = Get-DemoResultRecord -Path $resolvedDemoPath
$checkpointRecord = Get-DemoCheckpointRecord -Path $resolvedDemoPath
$normalizedExpectedResult = Normalize-ExpectedResult -Expected $expectedResult -Header $header -Checkpoint $checkpointRecord
$launchArgs = if ($useHeadlessConsole) {
    Get-HeadlessConsoleLaunchArguments -ResolvedDataDir $resolvedDataDir -ResolvedDemoPath $resolvedDemoPath -ActualResultPath $actualResultPath -ResolvedStateLogPath $resolvedStateLogPath -ResolvedRngLogPath $resolvedRngLogPath -ReplayDebugLog:$ReplayDebugLog -HeadlessConsoleOutput $HeadlessConsoleOutput
} else {
    Get-LaunchArguments -Config $config -ResolvedDataDir $resolvedDataDir -ResolvedDemoPath $resolvedDemoPath -ActualResultPath $actualResultPath -LaunchMode $launchMode -RenderProfileSelection $renderProfileSelection -Pilot $Pilot -NoRender:$effectiveNoRender -ShowReplayRobotLabels:$showReplayRobotLabels -ReplayDebugLog:$ReplayDebugLog -D1InD2:$D1InD2 -D1InD2StartFromLevel:$D1InD2StartFromLevel -ResolvedStateLogPath $resolvedStateLogPath -ResolvedRngLogPath $resolvedRngLogPath
}
$launchExecutable = if ($useHeadlessConsole) { $headlessConsoleExe } else { $sandbox.Exe }
$runnerName = $runnerSelection.Name
$quotedArgs = Get-QuotedArgumentString -Arguments $launchArgs

if (-not (Test-Path -LiteralPath $launchExecutable)) {
    throw "Built executable not found: $launchExecutable"
}

if (Test-Path -LiteralPath $actualResultPath) {
    Remove-Item -LiteralPath $actualResultPath -Force
}
if ($resolvedStateLogPath -and (Test-Path -LiteralPath $resolvedStateLogPath)) {
    Remove-Item -LiteralPath $resolvedStateLogPath -Force
}
if ($resolvedRngLogPath -and (Test-Path -LiteralPath $resolvedRngLogPath)) {
    Remove-Item -LiteralPath $resolvedRngLogPath -Force
}

if (-not $headlessQuietConsole) {
    Write-Host ''
    Write-Host "Demo: $(Get-RelativeRepoPath -Path $resolvedDemoPath)"
    Write-Host "Game: $resolvedGame"
    if ($D1InD2) {
        Write-Host 'Replay compatibility: D1 recording under D2 executable'
        if ($D1InD2StartFromLevel) {
            Write-Host 'Replay compatibility mode: start recording from level start'
        }
    }
    Write-Host "Runner: $runnerName"
    Write-Host "Runner selection: $($runnerSelection.Selection)"
    Write-Host "Replay path: $($runnerSelection.Description)"
    Write-Host "Mode: $($launchMode.Name)"
    Write-Host "Render profile: $($renderProfileSelection.Name) ($($renderProfileSelection.Description))"
    if ($canShowReplayRobotLabels) {
        Write-Host "Replay labels: $(if ($showReplayRobotLabels) { 'on' } else { 'off' })"
    } elseif ($replayRobotLabelsIgnored) {
        Write-Host 'Replay labels: ignored by this runner'
    }
    Write-Host "Replay debug log: $(if ($ReplayDebugLog) { 'on' } else { 'off' })"
    if ($useHeadlessConsole -and $renderProfileSelection.ExtraArgs.Count -gt 0) {
        Write-Host 'Render profile args: ignored by headless runner'
    }
    if ($effectiveNoRender -and -not $useHeadlessConsole) {
        Write-Host 'Render: no-present'
    }
    Write-Host "Data: $(Get-RelativeRepoPath -Path $resolvedDataDir)"
    Write-Host "Sandbox: $(Get-RelativeRepoPath -Path $sandbox.Directory)"
    if ($resolvedStateLogPath) {
        Write-Host "State trace: $(Get-RelativeRepoPath -Path $resolvedStateLogPath)"
    }
    if ($resolvedRngLogPath) {
        Write-Host "Rng trace: $(Get-RelativeRepoPath -Path $resolvedRngLogPath)"
    }
    if ($ReuseSandbox) {
        Write-Host 'Sandbox mode: reuse'
    }
    Write-Host "Command: $launchExecutable $quotedArgs"
}

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $launchExecutable
$startInfo.WorkingDirectory = $sandbox.Directory
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $false
$startInfo.RedirectStandardError = $false
$startInfo.Arguments = $quotedArgs

$process = $null
$replayStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$missingActualResult = $false
try {
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if (-not $process) {
        throw 'Failed to start replay process'
    }

    $waitResult = Wait-ForReplayResult -Process $process -ActualResultPath $actualResultPath -TimeoutSeconds $TimeoutSeconds
    if (-not $waitResult.ResultReady) {
        $replayStopwatch.Stop()
        if (-not $waitResult.Exited) {
            Stop-ReplayProcess -Process $process
        }
        if ($waitResult.Exited) {
            if ($AllowMissingActualResult -and $resolvedStateLogPath -and (Test-Path -LiteralPath $resolvedStateLogPath)) {
                $missingActualResult = $true
            } else {
                throw "Replay exited before writing an actual result`nExit code: $($waitResult.ExitCode)`nRepro: $($sandbox.Exe) $quotedArgs"
            }
        }
        if (-not $missingActualResult) {
            throw "Timed out waiting for replay result after $TimeoutSeconds seconds`nRepro: $($sandbox.Exe) $quotedArgs"
        }
    }

    if (-not $process.HasExited -and -not $process.WaitForExit(2000)) {
        Stop-ReplayProcess -Process $process
    }
} finally {
    Stop-ReplayProcess -Process $process
    $replayStopwatch.Stop()
}
$actualResult = $null
if (-not $missingActualResult) {
    $actualResult = Read-JsonFileAsHashtable -Path $actualResultPath
}
$expectedForCompare = $normalizedExpectedResult
$stateTraceCompareError = $null
$stateTraceExpectedPath = $null
$rngTraceCompareError = $null
$rngTraceExpectedPath = $null
if ($actualResult -and $D1InD2) {
    $expectedForCompare = Normalize-D1InD2ExpectedResult -Expected $expectedForCompare -Actual $actualResult
}
if ($actualResult -and (Test-ReplayUsedTerminalExitSubset -SandboxDirectory $sandbox.Directory -Expected $expectedForCompare -Actual $actualResult)) {
    $expectedForCompare = Get-TerminalExitExpectedSubset -Expected $expectedForCompare -Actual $actualResult
}
if ($resolvedStateLogPath) {
    if (-not (Test-Path -LiteralPath $resolvedStateLogPath)) {
        $stateTraceCompareError = "Replay did not write state trace: $resolvedStateLogPath"
    } elseif ($shouldCompareStateTrace) {
        $stateTraceResult = Invoke-StateTraceComparison -DemoPath $resolvedDemoPath -ActualPath $resolvedStateLogPath -Silent:$headlessQuietConsole
        $stateTraceExpectedPath = $stateTraceResult.ExpectedPath
        if (-not $headlessQuietConsole) {
            Write-Host "Expected trace: $(Get-RelativeRepoPath -Path $stateTraceExpectedPath)"
        }
        if ($stateTraceResult.ExitCode -ne 0) {
            if ($stateTraceResult.MismatchLines -and $stateTraceResult.MismatchLines.Count -gt 0) {
                $stateTraceCompareError = "State trace compare failed`n" + ($stateTraceResult.MismatchLines -join "`n")
            } else {
                $stateTraceCompareError = 'State trace compare failed'
            }
        } elseif (-not $headlessQuietConsole) {
            Write-Host 'State trace compare: PASS'
        }
    }
}
if ($resolvedRngLogPath) {
    if (-not (Test-Path -LiteralPath $resolvedRngLogPath)) {
        $rngTraceCompareError = "Replay did not write rng trace: $resolvedRngLogPath"
    } elseif ($shouldCompareRngTrace) {
        $rngTraceResult = Invoke-RngTraceComparison -DemoPath $resolvedDemoPath -ActualPath $resolvedRngLogPath -Silent:$headlessQuietConsole
        $rngTraceExpectedPath = $rngTraceResult.ExpectedPath
        if (-not $headlessQuietConsole) {
            Write-Host "Expected rng trace: $(Get-RelativeRepoPath -Path $rngTraceExpectedPath)"
        }
        if ($rngTraceResult.ExitCode -ne 0) {
            $rngTraceCompareError = 'RNG trace compare failed'
        } elseif (-not $headlessQuietConsole) {
            Write-Host 'RNG trace compare: PASS'
        }
    }
}
$compareError = $null
if ($SkipExpectedChecks) {
    $compareError = $null
} elseif ($actualResult) {
    $compareDiffs = Compare-JsonDiff -Expected $expectedForCompare -Actual $actualResult
    if ($compareDiffs.Count -gt 0) {
        $compareError = "Result compare failed`n" + ($compareDiffs -join "`n")
    }
} elseif (-not $AllowMissingActualResult) {
    $compareError = "Replay did not write an actual result: $actualResultPath"
}
$elapsedSeconds = [Math]::Round($replayStopwatch.Elapsed.TotalSeconds, 3)
$replayFps = $null
if ($actualResult -and $actualResult.ContainsKey('frame_count') -and $replayStopwatch.Elapsed.TotalSeconds -gt 0) {
    $replayFps = [Math]::Round(([double]$actualResult.frame_count) / $replayStopwatch.Elapsed.TotalSeconds, 2)
}

Write-Host ''
if ($null -ne $replayFps) {
    Write-Host ("Elapsed: {0}s replay_fps={1}" -f $elapsedSeconds, $replayFps)
} else {
    Write-Host ("Elapsed: {0}s" -f $elapsedSeconds)
}
if ($missingActualResult) {
    Write-Host 'Actual: <missing> (state trace/rng trace mode)'
} else {
    Write-Host "Actual: $(Get-RelativeRepoPath -Path $actualResultPath)"
}
if ($resolvedStateLogPath) {
    Write-Host "State trace: $(Get-RelativeRepoPath -Path $resolvedStateLogPath)"
}
if ($resolvedRngLogPath) {
    Write-Host "Rng trace: $(Get-RelativeRepoPath -Path $resolvedRngLogPath)"
}
if ($stateTraceExpectedPath) {
    Write-Host "Expected trace: $(Get-RelativeRepoPath -Path $stateTraceExpectedPath)"
}
if ($rngTraceExpectedPath) {
    Write-Host "Expected rng trace: $(Get-RelativeRepoPath -Path $rngTraceExpectedPath)"
}
if ($actualResult) {
    Write-Host ($actualResult | ConvertTo-Json -Depth 10)
}
if ($compareError -or $stateTraceCompareError -or $rngTraceCompareError) {
    Write-Host ''
    Write-Host 'RESULT: FAIL' -ForegroundColor Red
    if ($compareError) {
        Write-Host $compareError
    }
    if ($stateTraceCompareError) {
        Write-Host $stateTraceCompareError
    }
    if ($rngTraceCompareError) {
        Write-Host $rngTraceCompareError
    }
    if (-not $KeepSandbox) {
        Remove-Item -LiteralPath $sandbox.Directory -Recurse -Force -ErrorAction SilentlyContinue
    }
    exit 1
}

Write-Host ''
Write-Host 'RESULT: PASS' -ForegroundColor Green

if (-not $KeepSandbox) {
    Remove-Item -LiteralPath $sandbox.Directory -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0
