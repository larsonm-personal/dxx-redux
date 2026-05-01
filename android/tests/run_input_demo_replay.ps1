#!/usr/bin/env pwsh
param(
    [string]$DemoPath,
    [string]$SearchRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('prompt', 'realtime', 'accelerated')]
    [string]$Mode = 'prompt',
    [int]$TimeoutSeconds = 300,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$Pilot,
    [switch]$NoRender,
    [switch]$PreferHeadlessConsole,
    [switch]$ReuseSandbox,
    [switch]$KeepSandbox,
    [switch]$ListOnly
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
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
                Exe = Join-Path $repoRoot 'buildd1\main\dxx-redux-d1.exe'
                TitleArg = '-notitles'
                RequiredFiles = @('DESCENT.HOG', 'DESCENT.PIG')
                DefaultDataDirs = @(
                    (Join-Path $repoRoot 'game_data\extracted\d1 mac extracted'),
                    (Join-Path $repoRoot 'game_data_to_copy_to_emulator\temp')
                )
                MaxFps = 200
            }
        }
        'd2' {
            return @{
                Name = 'd2'
                Exe = Join-Path $repoRoot 'buildd2\main\dxx-redux-d2.exe'
                TitleArg = '-nomovies'
                RequiredFiles = @('DESCENT2.HOG', 'DESCENT2.HAM', 'GROUPA.PIG')
                DefaultDataDirs = @(
                    (Join-Path $repoRoot 'game_data_to_copy_to_emulator\temp'),
                    (Join-Path $repoRoot 'game_data\extracted\descent 2 demo 1-0_extracted')
                )
                MaxFps = 1000
            }
        }
    }

    throw "Unsupported game: $Name"
}

function Get-HeadlessConsoleExe {
    param([string]$GameName)

    switch ($GameName) {
        'd2' {
            return Join-Path $repoRoot 'buildd2\main\dxx-redux-d2-headless.exe'
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
        $requiredPath = Join-Path $Path $requiredFile
        if (-not (Test-Path -LiteralPath $requiredPath)) {
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
    $candidates += $Config.DefaultDataDirs

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-DataDirMatchesGame -Path $candidate -Config $Config) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $missingSummary = foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-Path -LiteralPath $candidate) {
            $missing = @()
            foreach ($requiredFile in $Config.RequiredFiles) {
                $requiredPath = Join-Path $candidate $requiredFile
                if (-not (Test-Path -LiteralPath $requiredPath)) {
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

function Test-UseHeadlessConsoleRunner {
    param(
        [string]$ResolvedGame,
        [hashtable]$Header,
        [hashtable]$LaunchMode,
        [string]$Pilot,
        [switch]$NoRender,
        [switch]$PreferHeadlessConsole
    )

    if (-not $PreferHeadlessConsole -or $NoRender -or $Pilot) {
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

function Get-HeadlessConsoleLaunchArguments {
    param(
        [string]$ResolvedDataDir,
        [string]$ResolvedDemoPath
    )

    return @(
        '-hogdir', $ResolvedDataDir,
        '-inputdemo-replay', $ResolvedDemoPath
    )
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
            $choice = Read-Host 'Choose replay mode (1 or 2)'
            switch ($choice) {
                '1' { $RequestedMode = 'realtime' }
                '2' { $RequestedMode = 'accelerated' }
                default { Write-Host 'Enter 1 or 2' }
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

function Get-LaunchArguments {
    param(
        [hashtable]$Config,
        [string]$ResolvedDataDir,
        [string]$ResolvedDemoPath,
        [hashtable]$LaunchMode,
        [string]$Pilot,
        [switch]$NoRender
    )

    $launchParameters = @(
        '-hogdir', $ResolvedDataDir,
        '-window',
        $Config.TitleArg,
        '-nomusic',
        '-nosound',
        '-maxfps', [string]$LaunchMode.MaxFps,
        '-inputdemo-replay', $ResolvedDemoPath
    )
    if ($LaunchMode.ExtraArgs.Count -gt 0) {
        $launchParameters += $LaunchMode.ExtraArgs
    }
    if ($NoRender) {
        $launchParameters += '-inputdemo-norender'
    }
    if ($Pilot) {
        $launchParameters += @('-pilot', $Pilot)
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
    while ($true) {
        $choice = Read-Host 'Choose demo number'
        $index = 0
        if ([int]::TryParse($choice, [ref]$index) -and $index -ge 1 -and $index -le $Candidates.Count) {
            return $Candidates[$index - 1]
        }
        Write-Host "Enter a number between 1 and $($Candidates.Count)"
    }
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

function Compare-JsonSubset {
    param(
        [object]$Expected,
        [object]$Actual,
        [string]$Path = 'result'
    )

    if ($Expected -is [System.Collections.IDictionary]) {
        if (-not ($Actual -is [System.Collections.IDictionary])) {
            return "${Path}: expected object, actual $(Format-CompareValue -Value $Actual)"
        }
        foreach ($key in $Expected.Keys) {
            if (-not $Actual.Contains($key)) {
                return "${Path}.${key}: missing from actual result"
            }
            $nested = Compare-JsonSubset -Expected $Expected[$key] -Actual $Actual[$key] -Path "${Path}.${key}"
            if ($nested) {
                return $nested
            }
        }
        return $null
    }

    if ($Expected -is [System.Collections.IList] -and -not ($Expected -is [string])) {
        if (-not ($Actual -is [System.Collections.IList])) {
            return "${Path}: expected array, actual $(Format-CompareValue -Value $Actual)"
        }
        if ($Expected.Count -ne $Actual.Count) {
            return "${Path}: expected array length $($Expected.Count), actual $($Actual.Count)"
        }
        for ($i = 0; $i -lt $Expected.Count; $i++) {
            $nested = Compare-JsonSubset -Expected $Expected[$i] -Actual $Actual[$i] -Path "${Path}[$i]"
            if ($nested) {
                return $nested
            }
        }
        return $null
    }

    if ($Expected -ne $Actual) {
        return "${Path}: expected $(Format-CompareValue -Value $Expected), actual $(Format-CompareValue -Value $Actual)"
    }

    return $null
}

function Read-JsonFileAsHashtable {
    param([string]$Path)

    return ([System.IO.File]::ReadAllText($Path) | ConvertFrom-Json -AsHashtable)
}

function Test-ReplayUsedTerminalExitSubset {
    param([string]$SandboxDirectory)

    $gamelogPath = Join-Path $SandboxDirectory 'gamelog.txt'
    if (-not (Test-Path -LiteralPath $gamelogPath)) {
        return $false
    }

    foreach ($line in [System.IO.File]::ReadLines($gamelogPath)) {
        if ($line.Contains('Input demo replay level-exit:') -or $line.Contains('Input demo replay mine-exit:')) {
            return $true
        }
    }

    return $false
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
$resolvedGame = Resolve-DemoGame -Header $header -RequestedGame $Game
$config = Get-GameConfig -Name $resolvedGame
$resolvedDataDir = Resolve-DataDir -Config $config -RequestedDataDir $DataDir
$launchMode = Get-LaunchMode -RequestedMode $Mode -Path $resolvedDemoPath -Config $config
$useHeadlessConsole = Test-UseHeadlessConsoleRunner -ResolvedGame $resolvedGame -Header $header -LaunchMode $launchMode -Pilot $Pilot -NoRender:$NoRender -PreferHeadlessConsole:$PreferHeadlessConsole
$headlessConsoleExe = if ($useHeadlessConsole) { Get-HeadlessConsoleExe -GameName $resolvedGame } else { $null }
$sandbox = New-LaunchSandbox -Config $config -SandboxName ([System.IO.Path]::GetFileNameWithoutExtension($resolvedDemoPath)) -ReuseSandbox:$ReuseSandbox -SkipExecutableCopy:$useHeadlessConsole
$actualResultPath = $resolvedDemoPath + '.actual.json'
$expectedResult = Get-DemoResultRecord -Path $resolvedDemoPath
$checkpointRecord = Get-DemoCheckpointRecord -Path $resolvedDemoPath
$normalizedExpectedResult = Normalize-ExpectedResult -Expected $expectedResult -Header $header -Checkpoint $checkpointRecord
$launchArgs = if ($useHeadlessConsole) {
    Get-HeadlessConsoleLaunchArguments -ResolvedDataDir $resolvedDataDir -ResolvedDemoPath $resolvedDemoPath
} else {
    Get-LaunchArguments -Config $config -ResolvedDataDir $resolvedDataDir -ResolvedDemoPath $resolvedDemoPath -LaunchMode $launchMode -Pilot $Pilot -NoRender:$NoRender
}
$launchExecutable = if ($useHeadlessConsole) { $headlessConsoleExe } else { $sandbox.Exe }
$runnerName = if ($useHeadlessConsole) { 'headless-console' } elseif ($NoRender) { 'windowed-no-present' } else { 'windowed' }
$quotedArgs = Get-QuotedArgumentString -Arguments $launchArgs

if (-not (Test-Path -LiteralPath $launchExecutable)) {
    throw "Built executable not found: $launchExecutable"
}

if (Test-Path -LiteralPath $actualResultPath) {
    Remove-Item -LiteralPath $actualResultPath -Force
}

Write-Host ''
Write-Host "Demo: $(Get-RelativeRepoPath -Path $resolvedDemoPath)"
Write-Host "Game: $resolvedGame"
Write-Host "Runner: $runnerName"
Write-Host "Mode: $($launchMode.Name)"
if ($NoRender -and -not $useHeadlessConsole) {
    Write-Host 'Render: no-present'
}
Write-Host "Data: $(Get-RelativeRepoPath -Path $resolvedDataDir)"
Write-Host "Sandbox: $(Get-RelativeRepoPath -Path $sandbox.Directory)"
if ($ReuseSandbox) {
    Write-Host 'Sandbox mode: reuse'
}
Write-Host "Command: $launchExecutable $quotedArgs"

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $launchExecutable
$startInfo.WorkingDirectory = $sandbox.Directory
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $false
$startInfo.RedirectStandardError = $false
$startInfo.Arguments = $quotedArgs

$process = [System.Diagnostics.Process]::Start($startInfo)
if (-not $process) {
    throw 'Failed to start replay process'
}

$replayStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$waitResult = Wait-ForReplayResult -Process $process -ActualResultPath $actualResultPath -TimeoutSeconds $TimeoutSeconds
if (-not $waitResult.ResultReady) {
    $replayStopwatch.Stop()
    if (-not $waitResult.Exited) {
        try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    if ($waitResult.Exited) {
        throw "Replay exited before writing an actual result`nExit code: $($waitResult.ExitCode)`nRepro: $($sandbox.Exe) $quotedArgs"
    }
    throw "Timed out waiting for replay result after $TimeoutSeconds seconds`nRepro: $($sandbox.Exe) $quotedArgs"
}

if (-not $process.HasExited) {
    try {
        if (-not $process.WaitForExit(2000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    } catch {
        try { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
}

$replayStopwatch.Stop()
$actualResult = Read-JsonFileAsHashtable -Path $actualResultPath
$expectedForCompare = $normalizedExpectedResult
if (Test-ReplayUsedTerminalExitSubset -SandboxDirectory $sandbox.Directory) {
    $expectedForCompare = Get-TerminalExitExpectedSubset -Expected $normalizedExpectedResult -Actual $actualResult
}
$compareError = Compare-JsonSubset -Expected $expectedForCompare -Actual $actualResult
$elapsedSeconds = [Math]::Round($replayStopwatch.Elapsed.TotalSeconds, 3)
$replayFps = $null
if ($actualResult.ContainsKey('frame_count') -and $replayStopwatch.Elapsed.TotalSeconds -gt 0) {
    $replayFps = [Math]::Round(([double]$actualResult.frame_count) / $replayStopwatch.Elapsed.TotalSeconds, 2)
}

Write-Host ''
if ($null -ne $replayFps) {
    Write-Host ("Elapsed: {0}s replay_fps={1}" -f $elapsedSeconds, $replayFps)
} else {
    Write-Host ("Elapsed: {0}s" -f $elapsedSeconds)
}
if ($compareError) {
    Write-Host 'RESULT: FAIL' -ForegroundColor Red
    Write-Host $compareError
    Write-Host "Actual: $(Get-RelativeRepoPath -Path $actualResultPath)"
    Write-Host ($actualResult | ConvertTo-Json -Depth 10)
    if (-not $KeepSandbox) {
        Remove-Item -LiteralPath $sandbox.Directory -Recurse -Force -ErrorAction SilentlyContinue
    }
    exit 1
}

Write-Host 'RESULT: PASS' -ForegroundColor Green
Write-Host "Actual: $(Get-RelativeRepoPath -Path $actualResultPath)"
Write-Host ($actualResult | ConvertTo-Json -Depth 10)

if (-not $KeepSandbox) {
    Remove-Item -LiteralPath $sandbox.Directory -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0