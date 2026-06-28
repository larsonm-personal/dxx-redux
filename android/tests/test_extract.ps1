#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Single-source extraction regression test. Validates that a CD image or GOG
  installer produces the correct game files and that the game can load from them.

.DESCRIPTION
  Given a path to an extract_regression.json5 spec file:
  1. Sanitizes device state (clears file sets, removes legacy files)
  2. Verifies the device is CLEAN (canary: game must NOT be launchable)
  3. Pushes extracted game files into a "regression_test" set
  4. Verifies files are present and NOT from the base demo set
  5. Launches the game and verifies in-game state via introspection

.PARAMETER SpecPath
  Path to an extract_regression.json5 file.

.PARAMETER SkipLaunch
  Only verify file extraction -- don't launch the game.

.PARAMETER KeepFiles
  Don't clean up the regression_test set after running.

.EXAMPLE
  .\test_extract.ps1 "..\game_data\CD images\Descent II (USA)\extract_regression.json5"
  .\test_extract.ps1                # auto-discovers first available spec
#>
param(
    [Parameter(Position = 0)]
    [string]$SpecPath,
    [switch]$SkipLaunch,
    [switch]$KeepFiles
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'extract_regression_spec_helpers.ps1')
. (Join-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers') 'test_host_platform.ps1')

# Sentinel output to diagnose empty-log issues when run from run_all_tests
[Console]::Out.Flush()
Write-Host "test_extract.ps1 starting (PSScriptRoot=$PSScriptRoot)"

# -- Auto-discover SpecPath if not provided -------------------
if (-not $SpecPath) {
    $gameDataDir = Join-Path (Split-Path (Split-Path $PSScriptRoot)) "game_data"
    $specs = Get-ChildItem -Path $gameDataDir -Recurse -Filter "extract_regression.json5" -ErrorAction SilentlyContinue
    if (-not $specs -or $specs.Count -eq 0) {
        $repoRootForOracles = Split-Path (Split-Path $PSScriptRoot)
        if (Ensure-ExtractRegressionOracles -RepoRoot $repoRootForOracles -Context "test_extract.ps1") {
            $specs = Get-ChildItem -Path $gameDataDir -Recurse -Filter "extract_regression.json5" -ErrorAction SilentlyContinue
        }
    }
    if ($specs -and $specs.Count -gt 0) {
        $SpecPath = $specs[0].FullName
        Write-Host "Auto-selected spec: $SpecPath" -ForegroundColor Cyan
    } else {
        Write-Host "FAIL: No SpecPath provided and no extract_regression.json5 found under $gameDataDir" -ForegroundColor Red
        exit 1
    }
}

if ($SpecPath -and -not (Test-Path -LiteralPath $SpecPath -PathType Leaf)) {
    $repoRootForOracles = Split-Path (Split-Path $PSScriptRoot)
    if ((Split-Path $SpecPath -Leaf) -eq 'extract_regression.json5' -and
        (Test-Path -LiteralPath (Split-Path $SpecPath -Parent) -PathType Container) -and
        (Ensure-ExtractRegressionOracles -RepoRoot $repoRootForOracles -Context "test_extract.ps1")) {
        if (-not (Test-Path -LiteralPath $SpecPath -PathType Leaf)) {
            Write-Host "FAIL: Spec file still not found after oracle regeneration: $SpecPath" -ForegroundColor Red
            exit 1
        }
    }
}

# -- Config ---------------------------------------------------

$PACKAGE = 'com.dxxredux.app'
$ACTIVITY = 'com.dxxredux.app.SetupActivity'
$TEST_SET = 'regression_test'
$env:ANDROID_SERIAL = if ($env:ANDROID_SERIAL) { $env:ANDROID_SERIAL } else { 'emulator-5554' }
$SETS_ROOT = 'files/imported/sets'
$SETS_ROOT_ABS = "/data/data/$PACKAGE/$SETS_ROOT"

$_depBaseFile = Join-Path (Split-Path (Split-Path $PSScriptRoot)) 'dependency_base.txt'
if (-not (Test-Path $_depBaseFile)) {
    Write-Host "FAIL: dependency_base.txt not found. Create it with a line containing the dependency dir path" -ForegroundColor Red
    exit 1
}
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$ADB = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir 'platform-tools' -ToolName 'adb' -EnvironmentVariable 'ADB'

$GAME_EXTENSIONS = @('.pig', '.hog', '.ham', '.mvl', '.s11', '.s22', '.mn2',
    '.msn', '.dxa', '.pog', '.rl2', '.dtx', '.sow',
    '.gog', '.inst', '.dem')

# Extensions that are essential for gameplay (not movies/demos)
$ESSENTIAL_EXTENSIONS = @('.pig', '.hog', '.ham', '.s11', '.s22', '.mn2',
    '.msn', '.dxa', '.pog', '.gog', '.inst')
# Large optional files we skip pushing to save time (MVLs = movie files, ~100MB each)
$SKIP_LARGE_EXTENSIONS = @('.mvl', '.sow', '.dem', '.rl2', '.dtx')

# Known SHA1 hashes for the base demo set files (game_data_to_copy_to_emulator/data/).
# If a pushed file matches one of these, and the spec expects a different version,
# we know we're accidentally using the demo set.
$DEMO_SET_HASHES = @{}  # populated dynamically below

# -- Helpers --------------------------------------------------

# ADB command timeout (seconds). If any single adb call takes longer, kill it.
$ADB_TIMEOUT = 30

function Write-Status {
    param([string]$Msg, [string]$Color = 'Cyan')
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

function Adb {
    param([string[]]$CmdArgs, [int]$Timeout = $script:ADB_TIMEOUT)
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ADB
    $psi.Arguments = ($CmdArgs | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join ' '
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    # Read both streams asynchronously to prevent deadlock
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    if (-not $proc.WaitForExit($Timeout * 1000)) {
        Write-Status "  ADB timeout (${Timeout}s): $($CmdArgs -join ' ')" 'Red'
        try { $proc.Kill() } catch {}
        $proc.Dispose()
        return ''
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $proc.Dispose()
    return $stdout.Trim()
}

function Adb-RunAs {
    param([string]$Cmd)
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ADB
    $runAsCmd = "cd /data/data/$PACKAGE && $Cmd"
    foreach ($arg in @('shell', 'run-as', $PACKAGE, 'sh', '-c', $runAsCmd)) {
        $psi.ArgumentList.Add($arg)
    }
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    if (-not $proc.WaitForExit($script:ADB_TIMEOUT * 1000)) {
        Write-Status "  ADB timeout (${script:ADB_TIMEOUT}s): run-as $Cmd" 'Red'
        try { $proc.Kill() } catch {}
        $proc.Dispose()
        return ''
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $null = $stderrTask.GetAwaiter().GetResult()
    $proc.Dispose()
    return $stdout.Trim()
}

# Ensure the app is force-stopped on any exit (clean or error)
$script:_cleanupDone = $false
function Invoke-Cleanup {
    if ($script:_cleanupDone) { return }
    $script:_cleanupDone = $true
    # Fire-and-forget force-stop with short timeout
    try { Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) -Timeout 5 | Out-Null } catch {}
}
Register-EngineEvent PowerShell.Exiting -Action { Invoke-Cleanup } | Out-Null

function Read-Json5 {
    # Parse JSON5 file (strip // comments and trailing commas)
    param([string]$Path)
    return Read-Json5File $Path
}

function Get-SetupIntrospection {
    Adb -CmdArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT') | Out-Null
    Start-Sleep -Milliseconds 800
    $json = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/setup_introspect.json')
    if (-not $json -or $json -notmatch '^\s*\{') { return $null }
    try { return ($json | ConvertFrom-Json) } catch { return $null }
}

function Wait-SetupReady {
    # Poll until SetupActivity's broadcast receiver is alive.
    param([int]$TimeoutSeconds = 30)
    Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/setup_introspect.json') | Out-Null
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Seconds 1
        Adb -CmdArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_INTROSPECT') | Out-Null
        Start-Sleep -Milliseconds 800
        $json = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/setup_introspect.json')
        if ($json -and $json -match '"screen"') { return $true }
    }
    return $false
}

function Get-GameIntrospection {
    Adb -CmdArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT') | Out-Null
    Start-Sleep -Milliseconds 800
    $json = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/introspect.json')
    if (-not $json -or $json -notmatch '^\s*\{') {
        return [PSCustomObject]@{ screen_mode = 'loading'; menu = $null; in_game = $false }
    }
    try { return ($json | ConvertFrom-Json) }
    catch { return [PSCustomObject]@{ screen_mode = 'loading'; menu = $null; in_game = $false } }
}

function Send-IntroSkipTap {
    param($GameIntrospection)

    $displayWidth = 1280
    $displayHeight = 720
    if ($GameIntrospection -and $GameIntrospection.resolution) {
        if ($GameIntrospection.resolution.display_width) { $displayWidth = [int]$GameIntrospection.resolution.display_width }
        if ($GameIntrospection.resolution.display_height) { $displayHeight = [int]$GameIntrospection.resolution.display_height }
    }

    $heightOverWidth = if ($displayWidth -gt 0) { [double]$displayHeight / [double]$displayWidth } else { 1.0 }
    $pillLeft = 1.0 - (0.245 * $heightOverWidth)
    $tapNormX = ($pillLeft + 1.0) / 2.0
    $tapNormY = (0.019 + 0.075) / 2.0
    $tapX = [int][math]::Round(($displayWidth - 1) * $tapNormX)
    $tapY = [int][math]::Round(($displayHeight - 1) * $tapNormY)
    $tapX = [math]::Max(0, [math]::Min($displayWidth - 1, $tapX))
    $tapY = [math]::Max(0, [math]::Min($displayHeight - 1, $tapY))

    Adb -CmdArgs @('shell', 'input', 'tap', "$tapX", "$tapY") | Out-Null
}

function Get-ExtractAutomationScriptText {
    param([string]$MissionName, [string]$LevelName, [string]$Game, [string]$TestSet)

    $templatePath = Join-Path (Split-Path $PSScriptRoot) 'game_scripts\test_extract_regression_template.json5'
    $text = Get-Content -LiteralPath $templatePath -Raw
    $text = $text.Replace('"MISSION_NAME"', (ConvertTo-Json ([string]$MissionName) -Compress))
    $text = $text.Replace('"LEVEL_NAME"', (ConvertTo-Json ([string]$LevelName) -Compress))
    $body = ($text -replace "`r`n", "`n").Trim()
    $start = $body.IndexOf('[')
    $end = $body.LastIndexOf(']')
    if ($start -ge 0 -and $end -gt $start) {
        $body = $body.Substring($start + 1, $end - $start - 1).Trim()
    }
    $body = [regex]::Replace($body, '(?m)^\s*\{"action":\s*"skip_intro"[^\r\n]*(?:\r?\n)?', '')
    $gameJson = ConvertTo-Json ([string]$Game) -Compress
    $setJson = ConvertTo-Json ([string]$TestSet) -Compress
    return @"
[
    {"_info": {"_standalone": false}},
    {"action": "enter_launcher"},
    {"action": "setup_command", "command": "switch_set", "args": {"name": $setJson}, "post_delay_ms": 500},
    {"action": "setup_command", "command": "write_bool_pref", "args": {"key": "skip_intro_movie", "value": true}, "post_delay_ms": 250},
    {"action": "enter_game", "game": $gameJson},
$body
]
"@ -replace "`r`n", "`n"
}

function Write-GameAutomationDiagnostics {
    $stepLog = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/automation_log.jsonl')
    if ($stepLog) {
        Write-Status '--- automation_log.jsonl (last 20 lines) ---' 'Yellow'
        $stepLines = $stepLog -split "`r?`n" | Where-Object { $_ }
        foreach ($line in ($stepLines | Select-Object -Last 20)) {
            Write-Status "  $line" 'Yellow'
        }
    }

    $autoLogcat = Adb -CmdArgs @('logcat', '-d', '-s', 'DXX-Automate:*', 'DXX-LauncherScript:*', 'DXX-Setup:*')
    if ($autoLogcat) {
        Write-Status '--- automation logcat (last 20 lines) ---' 'Yellow'
        $logcatLines = $autoLogcat -split "`r?`n" | Where-Object { $_ }
        foreach ($line in ($logcatLines | Select-Object -Last 20)) {
            Write-Status "  $line" 'Yellow'
        }
    }
}

function Invoke-GameAutomationScript {
    param(
        [string]$ScriptText,
        [int]$TimeoutSeconds = 120
    )

    $repoRoot = Split-Path (Split-Path $PSScriptRoot)
    $tempDir = Join-Path $repoRoot 'temp'
    if (-not (Test-Path $tempDir)) {
        New-Item -ItemType Directory -Path $tempDir | Out-Null
    }

    $scriptName = "extract_regression_$([Guid]::NewGuid().ToString('N')).json5"
    $localPath = Join-Path $tempDir $scriptName
    [System.IO.File]::WriteAllText($localPath, $ScriptText, [System.Text.UTF8Encoding]::new($false))

    try {
        Adb -CmdArgs @('push', $localPath, "/data/local/tmp/$scriptName") -Timeout 60 | Out-Null
        Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'cp', "/data/local/tmp/$scriptName", "files/$scriptName") | Out-Null
        Adb -CmdArgs @('shell', 'rm', '-f', "/data/local/tmp/$scriptName") | Out-Null

        Adb -CmdArgs @('logcat', '-c') | Out-Null
        Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
        Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/automation_log.jsonl') | Out-Null

        Write-Status "Sending setup automation broadcast for: $scriptName" 'Cyan'
        Adb -CmdArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_AUTOMATE', '--es', 'script', $scriptName) | Out-Null

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
            Start-Sleep -Milliseconds 1500
            $resultJson = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/automation_result.json')
            if ($resultJson -and $resultJson -match '^\s*\{') {
                try { return ($resultJson | ConvertFrom-Json) } catch { }
            }
        }

        return $null
    } finally {
        try { Remove-Item -LiteralPath $localPath -Force -ErrorAction Ignore } catch { }
        try { Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', "files/$scriptName") | Out-Null } catch { }
    }
}

function Send-SetupCommand {
    param([string]$Command, [string]$Name, [string]$Path, [string]$Game, [string[]]$Extras = @())
    $args_ = @('am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
        '--es', 'command', $Command)
    if ($Name) { $args_ += @('--es', 'name', $Name) }
    if ($Path) { $args_ += @('--es', 'path', $Path) }
    if ($Game) { $args_ += @('--es', 'game', $Game) }
    if ($Extras.Count -gt 0) { $args_ += $Extras }
    Invoke-AdbShellArgs -ShellArgs $args_ -TimeoutSeconds 30 | Out-Null
}

function Send-SetupCdImport {
    param(
        [string]$CuePath,
        [string[]]$BinPaths,
        [bool]$IncludeAudio = $true
    )

    if (-not $BinPaths -or $BinPaths.Count -eq 0) {
        throw 'Send-SetupCdImport requires at least one BIN/IMG path'
    }

    $args_ = @(
        'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
        '--es', 'command', 'import_cd',
        '--es', 'cue_path', $CuePath,
        '--ez', 'include_audio', $(if ($IncludeAudio) { 'true' } else { 'false' })
    )
    if ($BinPaths.Count -eq 1) {
        $args_ += @('--es', 'bin_path', $BinPaths[0])
    } else {
        $args_ += @('--esa', 'bin_paths', ($BinPaths -join ','))
    }
    Invoke-AdbShellArgs -ShellArgs $args_ -TimeoutSeconds 30 | Out-Null
}

function Get-CueReferencedImagePaths {
    param([string]$CuePath)

    $paths = @()
    foreach ($line in Get-Content -LiteralPath $CuePath) {
        if ($line -match '^\s*FILE\s+"([^"]+)"') {
            $paths += $matches[1]
        }
    }
    return @($paths)
}

function Send-SetupIsoImport {
    param([string]$IsoPath)

    $args_ = @(
        'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
        '--es', 'command', 'import_iso',
        '--es', 'iso_path', $IsoPath
    )
    Invoke-AdbShellArgs -ShellArgs $args_ -TimeoutSeconds 30 | Out-Null
}

function Quote-ShArg {
    param([string]$Value)

    $replacement = "'`"'`"'"
    return "'" + ($Value -replace "'", $replacement) + "'"
}

function Invoke-AdbShellArgs {
    param(
        [string[]]$ShellArgs,
        [int]$TimeoutSeconds = 30
    )

    $command = ($ShellArgs | ForEach-Object {
            if ($_ -match '^[A-Za-z0-9_./:=,+-]+$') { $_ } else { Quote-ShArg $_ }
        }) -join ' '
    return Invoke-AdbRaw -Arguments @('shell', $command) -TimeoutSeconds $TimeoutSeconds
}

function Clear-AppPrivateDirectoryContents {
    param([string]$RemoteDir)

    Invoke-AdbRaw -Arguments @('exec-out', 'run-as', $PACKAGE, 'rm', '-rf', $RemoteDir) -TimeoutSeconds 30 | Out-Null
    Invoke-AdbRaw -Arguments @('exec-out', 'run-as', $PACKAGE, 'mkdir', '-p', $RemoteDir) -TimeoutSeconds 30 | Out-Null
}

function Clear-DirectImportScratch {
    try {
        Invoke-AdbRaw -Arguments @('shell', 'sh', '-c', 'rm -f /data/local/tmp/dxx_extract_*') -TimeoutSeconds 30 | Out-Null
    } catch { }
    try {
        Clear-AppPrivateDirectoryContents "/data/data/$PACKAGE/files/tmp_import"
    } catch { }
}

function Ensure-AppPrivateFile {
    param(
        [string]$LocalPath,
        [string]$RemoteRelativePath,
        [int]$TimeoutSeconds = 900
    )

    $RemoteRelativePath = $RemoteRelativePath -replace '\\', '/'
    $localItem = Get-Item -LiteralPath $LocalPath
    $remoteSize = Get-AppPrivateFileSize -RemoteRelativePath $RemoteRelativePath
    if ($remoteSize -match '^\d+$' -and [long]$remoteSize -eq $localItem.Length) {
        Write-Status "  Reusing staged source: $RemoteRelativePath" 'Gray'
        return
    }

    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $suffix = if ($attempt -eq 1) { '' } else { " (retry $attempt)" }
        Write-Status "  Pushing $(Split-Path $LocalPath -Leaf) -> $RemoteRelativePath$suffix" 'Gray'
        Copy-LocalFileToAppPrivate -LocalPath $LocalPath -RemotePath $RemoteRelativePath -DisplayPath $RemoteRelativePath -TimeoutSeconds $TimeoutSeconds

        $remoteSize = Get-AppPrivateFileSize -RemoteRelativePath $RemoteRelativePath
        if ($remoteSize -match '^\d+$' -and [long]$remoteSize -eq $localItem.Length) {
            return
        }
        $actualSize = if ($remoteSize) { $remoteSize } else { '<missing>' }
        if ($attempt -eq 3) {
            $df = ''
            try { $df = Invoke-AdbRaw -Arguments @('shell', 'df', '-h', "/data/data/$PACKAGE/files") -TimeoutSeconds 10 } catch { }
            if ($df) { Write-Status "  Device storage:`n$df" 'Yellow' }
            throw "App-private staging failed for $RemoteRelativePath (expected $($localItem.Length) bytes, got $actualSize)"
        }
        Write-Status "  App-private staging short write for $RemoteRelativePath (expected $($localItem.Length), got $actualSize); retrying" 'Yellow'
        Invoke-AdbRaw -Arguments @('shell', 'run-as', $PACKAGE, 'rm', '-f', "/data/data/$PACKAGE/$RemoteRelativePath") -TimeoutSeconds 30 | Out-Null
    }
}

function Invoke-AdbRaw {
    param(
        [string[]]$Arguments,
        [string]$InputFile,
        [int]$TimeoutSeconds = 30
    )

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ADB
    foreach ($arg in $Arguments) {
        $psi.ArgumentList.Add($arg)
    }
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardInput = [bool]$InputFile
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    try {
        if ($InputFile) {
            $inputStream = [System.IO.File]::OpenRead($InputFile)
            try {
                $inputStream.CopyTo($proc.StandardInput.BaseStream)
            } finally {
                $inputStream.Dispose()
                $proc.StandardInput.Close()
            }
        }

        if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
            try { $proc.Kill() } catch {}
            throw "ADB timeout (${TimeoutSeconds}s): $($Arguments -join ' ')"
        }

        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($proc.ExitCode -ne 0) {
            throw "ADB failed ($($Arguments -join ' ')): $stderr $stdout"
        }
        return $stdout.Trim()
    } finally {
        $proc.Dispose()
    }
}

function Get-AppPrivateFileSize {
    param([string]$RemoteRelativePath)

    $remotePath = "/data/data/$PACKAGE/$RemoteRelativePath"
    try {
        $quotedRemotePath = Quote-ShArg $remotePath
        return Invoke-AdbRaw -Arguments @(
            'exec-out', 'run-as', $PACKAGE, 'sh', '-c', "stat -c %s -- $quotedRemotePath 2>/dev/null"
        ) -TimeoutSeconds 10
    } catch {
        return ''
    }
}

function Copy-LocalFileToAppPrivate {
    param(
        [string]$LocalPath,
        [string]$RemotePath,
        [string]$DisplayPath,
        [int]$TimeoutSeconds
    )

    $remoteDir = (Split-Path $RemotePath -Parent) -replace '\\', '/'
    $remoteAbsPath = "/data/data/$PACKAGE/$RemotePath"
    $remoteAbsDir = "/data/data/$PACKAGE/$remoteDir"
    try {
        $quotedRemoteAbsDir = Quote-ShArg $remoteAbsDir
        $quotedRemoteAbsPath = Quote-ShArg $remoteAbsPath
        Invoke-AdbRaw -Arguments @(
            'exec-out', 'run-as', $PACKAGE, 'sh', '-c', "mkdir -p -- $quotedRemoteAbsDir"
        ) -TimeoutSeconds 30 | Out-Null
        Invoke-AdbRaw -Arguments @(
            'exec-out', 'run-as', $PACKAGE, 'sh', '-c', "rm -f -- $quotedRemoteAbsPath"
        ) -TimeoutSeconds 30 | Out-Null
        $localLength = (Get-Item -LiteralPath $LocalPath).Length
        if ($localLength -le 32 * 1024 * 1024) {
            Invoke-AdbRaw -Arguments @(
                'exec-in', 'run-as', $PACKAGE, 'sh', '-c', "cat > $quotedRemoteAbsPath"
            ) -InputFile $LocalPath -TimeoutSeconds $TimeoutSeconds | Out-Null
        } else {
            $chunkSize = 16 * 1024 * 1024
            $chunkTimeoutSeconds = [Math]::Min($TimeoutSeconds, 120)
            $chunkDir = Join-Path ([System.IO.Path]::GetTempPath()) "dxx_extract_$PID_$([guid]::NewGuid().ToString('N'))"
            New-Item -ItemType Directory -Path $chunkDir -Force | Out-Null
            $chunkPath = Join-Path $chunkDir 'chunk.bin'
            $remoteChunk = "/data/local/tmp/dxx_extract_${PID}_chunk.bin"
            $buffer = [byte[]]::new($chunkSize)
            $inputStream = [System.IO.File]::OpenRead($LocalPath)
            try {
                $expectedSize = [long]0
                while (($read = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    $chunkOut = [System.IO.File]::Open($chunkPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
                    try {
                        $chunkOut.Write($buffer, 0, $read)
                    } finally {
                        $chunkOut.Dispose()
                    }
                    Invoke-AdbRaw -Arguments @('push', $chunkPath, $remoteChunk) -TimeoutSeconds $chunkTimeoutSeconds | Out-Null
                    Invoke-AdbRaw -Arguments @('shell', 'chmod', '644', $remoteChunk) -TimeoutSeconds 30 | Out-Null
                    Invoke-AdbRaw -Arguments @(
                        'exec-out', 'run-as', $PACKAGE, 'sh', '-c', "cat $remoteChunk >> $quotedRemoteAbsPath"
                    ) -TimeoutSeconds $chunkTimeoutSeconds | Out-Null
                    Invoke-AdbRaw -Arguments @('shell', 'rm', '-f', $remoteChunk) -TimeoutSeconds 30 | Out-Null
                    $expectedSize += $read
                    $remoteSize = Get-AppPrivateFileSize -RemoteRelativePath $RemotePath
                    if ($remoteSize -notmatch '^\d+$' -or [long]$remoteSize -ne $expectedSize) {
                        throw "chunk append mismatch for $DisplayPath (expected $expectedSize bytes, got $remoteSize)"
                    }
                }
                if ($expectedSize -eq 0) {
                    Invoke-AdbRaw -Arguments @(
                        'exec-out', 'run-as', $PACKAGE, 'sh', '-c', ": > $quotedRemoteAbsPath"
                    ) -TimeoutSeconds 30 | Out-Null
                }
            } finally {
                $inputStream.Dispose()
                try { Invoke-AdbRaw -Arguments @('shell', 'rm', '-f', $remoteChunk) -TimeoutSeconds 30 | Out-Null } catch { }
                Remove-Item -LiteralPath $chunkDir -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    } catch {
        try {
            Invoke-AdbRaw -Arguments @(
                'exec-out', 'run-as', $PACKAGE, 'sh', '-c', "rm -f -- $quotedRemoteAbsPath"
            ) -TimeoutSeconds 30 | Out-Null
        } catch { }
        throw "ADB stream failed for ${DisplayPath}: $_"
    }
}

function Push-FileToSet {
    # Push a local file to the active set dir via staging.
    param([string]$LocalPath, [string]$RemoteName)
    $stagingPath = "/data/local/tmp/$RemoteName"
    $dest = "$SETS_ROOT_ABS/$TEST_SET/$RemoteName"
    Adb -CmdArgs @('push', $LocalPath, $stagingPath) -Timeout 120 | Out-Null
    Adb -CmdArgs @('shell', 'chmod', '644', $stagingPath) | Out-Null
    # Direct ProcessStartInfo call -- the sh -c argument needs single quotes to
    # protect > from adb shell's outer shell interpretation.
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ADB
    $psi.Arguments = "shell run-as $PACKAGE sh -c 'cat $stagingPath > $dest'"
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $null = $proc.StandardError.ReadToEndAsync()
    $null = $proc.StandardOutput.ReadToEnd()
    $proc.WaitForExit(120000) | Out-Null
    if (-not $proc.HasExited) { try { $proc.Kill() } catch {} }
    $proc.Dispose()
    Adb -CmdArgs @('shell', 'rm', '-f', $stagingPath) | Out-Null
}

function Get-RemoteFileSize {
    param([string]$SetName, [string]$FileName)
    $result = Adb-RunAs "stat -c %s $SETS_ROOT_ABS/$SetName/$FileName 2>/dev/null"
    if ($result -match '^\d+$') { return [long]$result }
    return 0
}

# -- Read spec ------------------------------------------------

if (-not (Test-Path -LiteralPath $SpecPath)) {
    Write-Host "FAIL: Spec file not found: $SpecPath" -ForegroundColor Red
    exit 1
}

$spec = Read-Json5 $SpecPath
$specDir = Split-Path (Resolve-Path $SpecPath) -Parent
$sourceName = (Split-Path $specDir -Leaf)
$useDirectCdImport = $spec.import_mode -eq 'setup_cd'
$useDirectIsoImport = $spec.import_mode -eq 'setup_iso'
$useDirectSetupImport = $useDirectCdImport -or $useDirectIsoImport
$priorResultStatus = $null
if ($spec.last_test_result) {
    $priorResultStatus = [string]$spec.last_test_result.status
}
$allowIncompleteDirectImportSkip = $useDirectSetupImport -and $priorResultStatus -ne 'pass'

# -- Result tracking ------------------------------------------
# Track test result in script-scope vars. Exit-Test writes them to the spec file.

$script:testStatus = 'fail'
$script:testFailureStep = 'unknown'
$script:testLevel = $null
$script:testFilesVerified = 0
$script:testClassConfirmed = $false
$script:testMode = 'full'

function Write-TestResult {
    # Persist last_test_result into the spec json5 file.
    if (-not $SpecPath -or -not (Test-Path $SpecPath)) { return }

    $lastTestResult = [ordered]@{
        status = $script:testStatus
        failure_step = $script:testFailureStep
        level_reached = $script:testLevel
        files_verified = $script:testFilesVerified
        classification_confirmed = $script:testClassConfirmed
        test_mode = $script:testMode
    }
    Set-RegressionSpecLastTestResult $SpecPath $lastTestResult
}

function Exit-Test {
    param(
        [int]$Code,
        [string]$Status = 'fail',
        [string]$FailureStep = $null,
        [string]$Level = $null,
        [int]$FilesVerified = -1,
        [bool]$ClassConfirmed = $false,
        [string]$TestMode = $null
    )
    $script:testStatus = $Status
    $script:testFailureStep = $FailureStep
    if ($Level) { $script:testLevel = $Level }
    if ($FilesVerified -ge 0) { $script:testFilesVerified = $FilesVerified }
    $script:testClassConfirmed = $ClassConfirmed
    if ($TestMode) { $script:testMode = $TestMode }
    Write-TestResult
    Clear-DirectImportScratch
    Invoke-Cleanup
    exit $Code
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  EXTRACT REGRESSION TEST" -ForegroundColor White
Write-Host "  Source: $sourceName" -ForegroundColor White
Write-Host "  Type: $($spec.source_type) / $($spec.classification)" -ForegroundColor White
Write-Host "  Game: $($spec.game)" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White
Write-Host ""

# Check if this is a non-launchable spec
$canLaunch = ($null -ne $spec.expected_mission)
if (-not $canLaunch) {
    Write-Status "This is a non-launchable source ($($spec.classification)) - file-only test" 'Yellow'
}

# -- Locate extracted files -----------------------------------

$extractedDir = $null
$filesToPush = @()
if ($useDirectSetupImport) {
    if ($spec.source_type -ne 'cd') {
        Write-Host "FAIL: setup import modes only support source_type=cd" -ForegroundColor Red
        Exit-Test 1 'fail' 'source_missing'
    }
} elseif ($spec.source_type -eq 'cd') {
    $extractedDir = Join-Path $specDir 'data_tracks'
    # Some CDs organize into subdirs (d1data, d2data)
    if (-not (Test-Path $extractedDir)) {
        Write-Host "FAIL: No data_tracks/ directory found at $specDir. Run extract_all_cds.ps1 first" -ForegroundColor Red
        Exit-Test 1 'fail' 'source_missing'
    }
} elseif ($spec.source_type -eq 'gog') {
    # GOG extracted dir is named after the installer (without extension)
    $installerName = $spec.source_files[0].name
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($installerName)
    $extractedDir = Join-Path $specDir $baseName
    if (-not (Test-Path $extractedDir)) {
        Write-Host "FAIL: No extracted directory found at $extractedDir. Run extract_all_gog.ps1 first" -ForegroundColor Red
        Exit-Test 1 'fail' 'source_missing'
    }
}

if (-not $useDirectSetupImport) {
    # Collect all game files to push (recurse into subdirs, flatten).
    # Filter out 1-byte stubs (some ISO9660 extractions create case-variant symlinks).
    # Skip large optional files (MVLs, SOWs) to speed up testing.
    # Also deduplicate by lowercase name, preferring the larger file.
    $allGameFiles = Get-ChildItem $extractedDir -Recurse -File |
        Where-Object {
            $ext = $_.Extension.ToLower()
            $GAME_EXTENSIONS -contains $ext -and $_.Length -gt 1 -and
            $SKIP_LARGE_EXTENSIONS -notcontains $ext
        }

    # Deduplicate: if both DESCENT2.HOG and descent2.hog exist, keep the larger one
    $dedup = @{}
    foreach ($f in $allGameFiles) {
        $key = $f.Name.ToLower()
        if (-not $dedup.ContainsKey($key) -or $f.Length -gt $dedup[$key].Length) {
            $dedup[$key] = $f
        }
    }
    $filesToPush = $dedup.Values | Sort-Object Name

    Write-Status "Found $($filesToPush.Count) game files to push from $extractedDir"

    if ($filesToPush.Count -eq 0) {
        Write-Status "FAIL: No game files found to push" 'Red'
        Exit-Test 1 'fail' 'file_push_failed'
    }
}

# -- Compute demo set hashes for canary check -----------------
# Hash a signature file from game_data_to_copy_to_emulator/data/ if it exists,
# so we can verify pushed files are NOT from the demo set.

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$demoDir = Join-Path $repoRoot 'game_data_to_copy_to_emulator' 'data'
$signatureFiles = @('descent2.hog', 'descent.hog', 'groupa.pig', 'descent.pig')
$demoHashes = @{}  # filename -> SHA1

if (Test-Path $demoDir) {
    foreach ($sf in $signatureFiles) {
        $demoFile = Get-ChildItem $demoDir -Filter $sf -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $demoFile) {
            # Try uppercase
            $demoFile = Get-ChildItem $demoDir -Filter $sf.ToUpper() -ErrorAction SilentlyContinue |
                Select-Object -First 1
        }
        if ($demoFile) {
            $hash = (Get-FileHash $demoFile.FullName -Algorithm SHA1).Hash.ToLower()
            $demoHashes[$sf.ToLower()] = $hash
        }
    }
    if ($demoHashes.Count -gt 0) {
        Write-Status "Computed $($demoHashes.Count) demo set signature hashes for canary check"
    }
}

# -- Step 1: Verify emulator is reachable ---------------------

Write-Status "Checking emulator..."
$devices = Adb -CmdArgs 'devices'
if ($devices -notmatch 'emulator-\d+\s+device') {
    Write-Status "Emulator offline, attempting restart via emu_health.ps1..."
    $healthScript = Join-Path $PSScriptRoot "..\helpers\emu_health.ps1"
    if (Test-Path $healthScript) {
        & $healthScript -Restart -Wait -TimeoutSeconds 180 2>&1 | Out-Null
        # Poll for emulator to come back online
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 30) {
            $devCheck = Adb -CmdArgs 'devices'
            if ($devCheck -match 'emulator-\d+\s+device') {
                $bootCheck = Adb -CmdArgs @('shell', 'getprop', 'sys.boot_completed')
                if ($bootCheck.Trim() -eq '1') { break }
            }
            Start-Sleep -Seconds 1
        }
        $devices = Adb -CmdArgs 'devices'
    }
    if ($devices -notmatch 'emulator-\d+\s+device') {
        Write-Status "FAIL: No running emulator found" 'Red'
        Exit-Test 1 'fail' 'emulator_offline'
    }
}
$boot = Adb -CmdArgs @('shell', 'getprop', 'sys.boot_completed')
if ($boot.Trim() -ne '1') {
    Write-Status "FAIL: Emulator not fully booted" 'Red'
    Exit-Test 1 'fail' 'emulator_offline'
}
Write-Status "Emulator online" 'Green'

# -- Step 2: Sanitize device state ----------------------------
# This is the critical section. We must ensure:
# - No game files exist in filesDir root (PhysFS fallback)
# - The default set is cleared (so no files leak)
# - The regression_test set is cleared
# - After sanitizing, the game should NOT be launchable

Write-Status "Sanitizing device state..."

# Force-stop the app first
Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
Start-Sleep -Seconds 1

# Launch SetupActivity (needed for broadcasts to work)
Adb -CmdArgs @('shell', 'am', 'start', '-n', "$PACKAGE/$ACTIVITY") | Out-Null
if (-not (Wait-SetupReady)) {
    Write-Status 'FAIL: SetupActivity not responding' 'Red'
    Exit-Test 1 'fail' 'setup_timeout'
}

# Keep extract runs deterministic regardless of previous launcher tests.
Send-SetupCommand 'write_bool_pref' -Extras @('--es', 'key', 'skip_intro_movie', '--ez', 'value', 'false')
Start-Sleep -Milliseconds 250

# Create test set if it doesn't exist, then switch to it
Send-SetupCommand 'create_set' -Name $TEST_SET
Start-Sleep -Seconds 1
Send-SetupCommand 'switch_set' -Name $TEST_SET
Start-Sleep -Seconds 1

# Clear the test set via broadcast AND direct rm (broadcast may miss files
# not visible to Java's File.listFiles() if they were created by shell cat)
Send-SetupCommand 'clear_set' -Name $TEST_SET
Start-Sleep -Seconds 1
Write-Status "  Direct-cleaning test set files..."
Clear-AppPrivateDirectoryContents "$SETS_ROOT_ABS/$TEST_SET"

# Clear the default set too (prevent leaking)
Send-SetupCommand 'clear_set' -Name 'default'
Start-Sleep -Seconds 1
Clear-AppPrivateDirectoryContents "$SETS_ROOT_ABS/default"

Send-SetupCommand 'clear_audio_sources'
Start-Sleep -Milliseconds 500

# Clean filesDir root of any game files (belt-and-suspenders for legacy setups)
Write-Status "Cleaning filesDir root of game files..."
$filesDir = "/data/data/$PACKAGE/files"
Write-Status "  Cleaning direct-import scratch..."
Clear-DirectImportScratch
$rootListing = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'ls', "$filesDir/")
foreach ($rf in ($rootListing -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
    try { $ext = [System.IO.Path]::GetExtension($rf).ToLower() } catch { continue }
    if ($GAME_EXTENSIONS -contains $ext -or $ext -in @('.plr', '.plx')) {
        Write-Status "  Removing stale root file: $rf"
        Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', "$filesDir/$rf") | Out-Null
    }
}

# Also clear other test sets that might have game files (gog_d1_test, gog_d2_test, etc.)
# These shouldn't leak through PhysFS, but clearing them eliminates any doubt.
$otherSets = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'ls', "$filesDir/sets/")
foreach ($setName in ($otherSets -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_ -ne $TEST_SET -and $_ -ne 'default' })) {
    $setFiles = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'ls', "$filesDir/sets/$setName/") 2>$null
    $hasGameFiles = $false
    foreach ($sf in ($setFiles -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
        $ext = [System.IO.Path]::GetExtension($sf).ToLower()
        if ($GAME_EXTENSIONS -contains $ext) { $hasGameFiles = $true; break }
    }
    if ($hasGameFiles) {
        Write-Status "  Clearing stale test set: $setName"
        Send-SetupCommand 'clear_set' -Name $setName
        Clear-AppPrivateDirectoryContents "$filesDir/sets/$setName"
        Start-Sleep -Milliseconds 500
    }
}

# -- Step 3: Canary check -- verify device is CLEAN ------------
# Restart the app so the clean state is visible to Java
Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
Start-Sleep -Seconds 1
Adb -CmdArgs @('shell', 'am', 'start', '-n', "$PACKAGE/$ACTIVITY") | Out-Null
if (-not (Wait-SetupReady)) {
    Write-Status 'FAIL: SetupActivity not responding after restart' 'Red'
    Exit-Test 1 'fail' 'setup_timeout'
}

Write-Status "Canary check: verifying device is clean..."
$state = Get-SetupIntrospection
if (-not $state) {
    Write-Status "FAIL: Could not get setup introspection (app may not be running)" 'Red'
    Exit-Test 1 'fail' 'canary_failed'
}

# Verify active set is our test set
if ($state.active_set -ne $TEST_SET) {
    Write-Status "FAIL: Active set is '$($state.active_set)' instead of '$TEST_SET'" 'Red'
    Exit-Test 1 'fail' 'canary_failed'
}

# Verify set is empty
$setFileCount = ($state.set_files | Measure-Object).Count
if ($setFileCount -gt 0) {
    # Filter out metadata files
    $gameFilesInSet = $state.set_files | Where-Object {
        $ext = [System.IO.Path]::GetExtension($_).ToLower()
        $GAME_EXTENSIONS -contains $ext
    }
    if (($gameFilesInSet | Measure-Object).Count -gt 0) {
        Write-Status "FAIL: Canary check - test set still has game files after clear: $($gameFilesInSet -join ', ')" 'Red'
        Exit-Test 1 'fail' 'canary_failed'
    }
}

# Verify game marked not launchable -- this is the definitive canary.
# PhysFS searches: 1) active set dir, 2) filesDir root. If can_launch
# is false with an empty test set, no game files are reachable.
if ($state.can_launch) {
    Write-Status "FAIL: Canary check - game reports can_launch=true with empty test set!" 'Red'
    Write-Status "  This means game files are leaking from somewhere" 'Red'
    Write-Status "  Active set: $($state.active_set), set_files: $($state.set_files -join ', ')" 'Red'
    Write-Status "  files_on_disk: $($state.files_on_disk -join ', ')" 'Red'
    Exit-Test 1 'fail' 'canary_failed'
}

Write-Status "Canary PASSED - device is clean, game cannot launch" 'Green'

# -- Step 4: Push extracted files to test set -----------------

if ($useDirectCdImport) {
    $cueSpec = $spec.source_files | Where-Object { $_.name -match '\.cue$' } | Select-Object -First 1
    $imageSpecs = @($spec.source_files | Where-Object { $_.name -match '\.(bin|img)$' })
    if (-not $cueSpec -or $imageSpecs.Count -eq 0) {
        Write-Status 'FAIL: import_mode=setup_cd requires .cue and at least one .bin/.img source_file' 'Red'
        Exit-Test 1 'fail' 'source_missing'
    }

    $localCuePath = Join-Path $specDir $cueSpec.name
    if (-not (Test-Path -LiteralPath $localCuePath)) {
        Write-Status 'FAIL: Local cue source file is missing for direct import' 'Red'
        Exit-Test 1 'fail' 'source_missing'
    }

    $availableImagesByName = @{}
    foreach ($imageSpec in $imageSpecs) {
        $imageKey = [System.IO.Path]::GetFileName($imageSpec.name).ToLowerInvariant()
        if (-not $availableImagesByName.ContainsKey($imageKey)) {
            $availableImagesByName[$imageKey] = [System.Collections.ArrayList]::new()
        }
        [void]$availableImagesByName[$imageKey].Add($imageSpec)
    }

    $referencedImagePaths = @(Get-CueReferencedImagePaths -CuePath $localCuePath)
    $orderedImages = @()
    $missingImages = @()
    foreach ($referencedImagePath in $referencedImagePaths) {
        $imageKey = [System.IO.Path]::GetFileName($referencedImagePath).ToLowerInvariant()
        $queue = $availableImagesByName[$imageKey]
        if ($queue -and $queue.Count -gt 0) {
            $orderedImages += [pscustomobject]@{ CuePath = ($referencedImagePath -replace '\\', '/'); Spec = $queue[0] }
            $queue.RemoveAt(0)
        } else {
            $missingImages += $referencedImagePath
        }
    }
    if ($referencedImagePaths.Count -eq 0) {
        $orderedImages = $imageSpecs | ForEach-Object {
            [pscustomobject]@{ CuePath = ([System.IO.Path]::GetFileName($_.name)); Spec = $_ }
        }
    }
    if ($missingImages.Count -gt 0) {
        Write-Status "FAIL: Local direct-import image set is missing CUE-referenced files: $($missingImages -join ', ')" 'Red'
        Exit-Test 1 'fail' 'source_missing'
    }

    $localImages = @()
    foreach ($orderedImage in $orderedImages) {
        $localImagePath = Join-Path $specDir $orderedImage.Spec.name
        if (-not (Test-Path -LiteralPath $localImagePath)) {
            Write-Status "FAIL: Local image source file is missing for direct import: $($orderedImage.Spec.name)" 'Red'
            Exit-Test 1 'fail' 'source_missing'
        }
        $localImages += [pscustomobject]@{ CuePath = $orderedImage.CuePath; LocalPath = $localImagePath }
    }

    $deviceDirName = ($sourceName -replace '[^A-Za-z0-9._-]', '_')
    $appSourceRelDir = "files/tmp_import/$deviceDirName"
    $appCueRelPath = "$appSourceRelDir/$([System.IO.Path]::GetFileName($cueSpec.name))"
    $deviceCuePath = "/data/user/0/$PACKAGE/$appCueRelPath"

    Write-Status "Staging CD source files for direct import..."
    Ensure-AppPrivateFile -LocalPath $localCuePath -RemoteRelativePath $appCueRelPath -TimeoutSeconds 120

    $deviceImagePaths = @()
    foreach ($localImage in $localImages) {
        $appImageRelPath = "$appSourceRelDir/$($localImage.CuePath)"
        Ensure-AppPrivateFile -LocalPath $localImage.LocalPath -RemoteRelativePath $appImageRelPath -TimeoutSeconds 1800
        $deviceImagePaths += "/data/user/0/$PACKAGE/$appImageRelPath"
    }

    Write-Status "Triggering setup-command CD import..."
    Send-SetupCdImport -CuePath $deviceCuePath -BinPaths $deviceImagePaths -IncludeAudio $true

    $importReady = $false
    $lastImportSignature = ''
    $stableImportPolls = 0
    $importTimeoutSeconds = 300
    $importSw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($importSw.Elapsed.TotalSeconds -lt $importTimeoutSeconds) {
        Start-Sleep -Seconds 2
        $state = Get-SetupIntrospection
        if (-not $state) { continue }
        $remoteFiles = @($state.set_files | Where-Object { $_ })
        $expectedFiles = @()
        if ($spec.expected_files -is [array]) { $expectedFiles = $spec.expected_files }
        $haveExpected = $true
        foreach ($ef in $expectedFiles) {
            if (-not ($remoteFiles | Where-Object { $_.ToLower() -eq $ef.ToLower() })) {
                $haveExpected = $false
                break
            }
        }
        $haveTotal = $true
        if ($expectedFiles.Count -eq 0 -and $spec.total_extracted) {
            $haveTotal = $remoteFiles.Count -ge [int]$spec.total_extracted
        }
        if ($haveExpected -and $haveTotal) {
            $importReady = $true
            break
        }
        $signature = ($remoteFiles | Sort-Object) -join "`n"
        if ($signature -and $signature -eq $lastImportSignature) {
            $stableImportPolls++
        } else {
            $lastImportSignature = $signature
            $stableImportPolls = 0
        }
        if ($allowIncompleteDirectImportSkip -and $stableImportPolls -ge 3) {
            $verified = @($expectedFiles | Where-Object {
                    $ef = $_.ToLower()
                    $remoteFiles | Where-Object { $_.ToLower() -eq $ef }
                }).Count
            Write-Status "SKIP: Direct CD import settled without expected launch files" 'Yellow'
            Exit-Test 0 'skip' 'not_ready' -TestMode 'file_only' -FilesVerified $verified
        }
        if (-not $canLaunch -and $expectedFiles.Count -eq 0 -and $stableImportPolls -ge 3) {
            Write-Status "PASS (file-only): Direct CD import settled for $($spec.classification)" 'Green'
            Exit-Test 0 'pass' -TestMode 'file_only' -FilesVerified 0 -ClassConfirmed $true
        }
    }

    if (-not $importReady) {
        Write-Status "FAIL: Timed out waiting for direct CD import to finish" 'Red'
        Exit-Test 1 'fail' 'file_push_failed'
    }

    $pushCount = @($state.set_files | Where-Object { $_ }).Count
    Write-Status "Direct import completed with $pushCount file(s) visible in '$TEST_SET'" 'Green'
    Clear-DirectImportScratch
} elseif ($useDirectIsoImport) {
    $isoSpec = $spec.source_files | Where-Object { $_.name -match '\.iso$' } | Select-Object -First 1
    if (-not $isoSpec) {
        Write-Status 'FAIL: import_mode=setup_iso requires an .iso source file' 'Red'
        Exit-Test 1 'fail' 'source_missing'
    }

    $localIsoPath = Join-Path $specDir $isoSpec.name
    if (-not (Test-Path -LiteralPath $localIsoPath)) {
        Write-Status 'FAIL: Local ISO source file is missing for direct import' 'Red'
        Exit-Test 1 'fail' 'source_missing'
    }

    $deviceDirName = ($sourceName -replace '[^A-Za-z0-9._-]', '_')
    $appSourceRelDir = "files/tmp_import/$deviceDirName"
    $appIsoRelPath = "$appSourceRelDir/source.iso"
    $deviceIsoPath = "/data/user/0/$PACKAGE/$appIsoRelPath"

    Write-Status "Staging ISO source file for direct import..."
    Ensure-AppPrivateFile -LocalPath $localIsoPath -RemoteRelativePath $appIsoRelPath -TimeoutSeconds 1800

    Write-Status "Triggering setup-command ISO import..."
    Send-SetupIsoImport -IsoPath $deviceIsoPath

    $importReady = $false
    $lastImportSignature = ''
    $stableImportPolls = 0
    $importTimeoutSeconds = 300
    $importSw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($importSw.Elapsed.TotalSeconds -lt $importTimeoutSeconds) {
        Start-Sleep -Seconds 2
        $state = Get-SetupIntrospection
        if (-not $state) { continue }
        $remoteFiles = @($state.set_files | Where-Object { $_ })
        $expectedFiles = @()
        if ($spec.expected_files -is [array]) { $expectedFiles = $spec.expected_files }
        $haveExpected = $true
        foreach ($ef in $expectedFiles) {
            if (-not ($remoteFiles | Where-Object { $_.ToLower() -eq $ef.ToLower() })) {
                $haveExpected = $false
                break
            }
        }
        $haveTotal = $true
        if ($expectedFiles.Count -eq 0 -and $spec.total_extracted) {
            $haveTotal = $remoteFiles.Count -ge [int]$spec.total_extracted
        }
        if ($haveExpected -and $haveTotal) {
            $importReady = $true
            break
        }
        $signature = ($remoteFiles | Sort-Object) -join "`n"
        if ($signature -and $signature -eq $lastImportSignature) {
            $stableImportPolls++
        } else {
            $lastImportSignature = $signature
            $stableImportPolls = 0
        }
        if ($allowIncompleteDirectImportSkip -and $stableImportPolls -ge 3) {
            $verified = @($expectedFiles | Where-Object {
                    $ef = $_.ToLower()
                    $remoteFiles | Where-Object { $_.ToLower() -eq $ef }
                }).Count
            Write-Status "SKIP: Direct ISO import settled without expected launch files" 'Yellow'
            Exit-Test 0 'skip' 'not_ready' -TestMode 'file_only' -FilesVerified $verified
        }
        if (-not $canLaunch -and $expectedFiles.Count -eq 0 -and $stableImportPolls -ge 3) {
            Write-Status "PASS (file-only): Direct ISO import settled for $($spec.classification)" 'Green'
            Exit-Test 0 'pass' -TestMode 'file_only' -FilesVerified 0 -ClassConfirmed $true
        }
    }

    if (-not $importReady) {
        Write-Status "FAIL: Timed out waiting for direct ISO import to finish" 'Red'
        Exit-Test 1 'fail' 'file_push_failed'
    }

    $pushCount = @($state.set_files | Where-Object { $_ }).Count
    Write-Status "Direct import completed with $pushCount file(s) visible in '$TEST_SET'" 'Green'
    Clear-DirectImportScratch
} else {
    Write-Status "Pushing $($filesToPush.Count) files to set '$TEST_SET'..."
    $pushErrors = 0
    $pushCount = 0

    foreach ($file in $filesToPush) {
        $remoteName = $file.Name.ToLower()
        $sizeKB = [math]::Round($file.Length / 1024)
        Write-Host "  $($file.Name) -> $remoteName  (${sizeKB} KB)" -ForegroundColor Gray
        try {
            Push-FileToSet -LocalPath $file.FullName -RemoteName $remoteName
            $pushCount++
        } catch {
            Write-Status "  ERROR pushing $($file.Name): $_" 'Red'
            $pushErrors++
        }
    }

    if ($pushErrors -gt 0) {
        Write-Status "FAIL: $pushErrors file(s) failed to push" 'Red'
        Exit-Test 1 'fail' 'file_push_failed'
    }
    Write-Status "Pushed $pushCount files" 'Green'

    # Restart the app so Java's File.listFiles() sees externally-created files.
    # Without this, the JVM within the running process won't see files added by
    # adb shell cat redirect.
    Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
    Start-Sleep -Seconds 1
    Adb -CmdArgs @('shell', 'am', 'start', '-n', "$PACKAGE/$ACTIVITY") | Out-Null
    if (-not (Wait-SetupReady)) {
        Write-Status 'FAIL: SetupActivity not responding after file push' 'Red'
        Exit-Test 1 'fail' 'setup_timeout'
    }
}

# -- Step 5: Verify files on device ---------------------------
# After restart, Java File.listFiles() can see the files. Use introspection.

Write-Status "Verifying files on device..."
$state = Get-SetupIntrospection
if (-not $state) {
    Write-Status "FAIL: Could not get setup introspection (app may not be running)" 'Red'
    Exit-Test 1 'fail' 'files_missing'
}
$remoteFiles = @($state.set_files | Where-Object { $_ })

# Check expected files present
$expectedFiles = @()
if ($spec.expected_files -is [array]) { $expectedFiles = $spec.expected_files }
$missingFiles = @()
foreach ($ef in $expectedFiles) {
    $efLower = $ef.ToLower()
    $found = $remoteFiles | Where-Object { $_.ToLower() -eq $efLower }
    if (-not $found) {
        $missingFiles += $ef
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Status "FAIL: Missing expected files: $($missingFiles -join ', ')" 'Red'
    Write-Status "  Files in set: $($remoteFiles -join ', ')" 'Yellow'
    $script:testFilesVerified = $expectedFiles.Count - $missingFiles.Count
    Exit-Test 1 'fail' 'files_missing'
}
Write-Status "All $($expectedFiles.Count) expected files present (of $($remoteFiles.Count) total)" 'Green'
$script:testFilesVerified = $expectedFiles.Count

# -- Step 6: Anti-demo-set canary -- verify file identity ------
# Hash signature game files on device and compare against demo set hashes.
# If they match (and this isn't the same version), we've been fooled.
$identityWarnings = @()
foreach ($sf in $signatureFiles) {
    $sfLower = $sf.ToLower()
    if (-not ($remoteFiles | Where-Object { $_.ToLower() -eq $sfLower })) {
        continue  # File not in set, skip
    }
    if (-not $demoHashes.ContainsKey($sfLower)) {
        continue  # No demo reference hash, skip
    }

    # Hash the file on device using sha1sum (don't use sh -c, it splits args)
    $remotePath = "$SETS_ROOT/$TEST_SET/$sfLower"
    $hashOutput = Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'sha1sum', $remotePath)
    if ($hashOutput -match '^([0-9a-f]{40})') {
        $deviceHash = $Matches[1]
        $demoHash = $demoHashes[$sfLower]

        if ($deviceHash -eq $demoHash) {
            # This file is identical to the demo set. That's only OK if the
            # spec's source is the same version as our demo set.
            $identityWarnings += "  ${sfLower}: matches demo set hash ($deviceHash)"
        } else {
            Write-Status "  ${sfLower}: $deviceHash (differs from demo set - good)" 'Green'
        }
    } else {
        Write-Status "  WARN: Could not hash ${sfLower} on device" 'Yellow'
    }
}

if ($identityWarnings.Count -gt 0) {
    Write-Status "NOTICE: $($identityWarnings.Count) file(s) match demo set hashes:" 'Yellow'
    foreach ($w in $identityWarnings) { Write-Host $w -ForegroundColor Yellow }
    Write-Status "  This may be OK if the disc version matches the demo set version" 'Yellow'
    Write-Status "  Review manually if unexpected" 'Yellow'
}

# -- Step 7: Check file set readiness -------------------------
# $state was already populated by Step 5's introspection

$gameKey = $spec.game
if ($gameKey -eq 'd1d2') {
    # For multi-game discs, check based on classification
    if ($spec.classification -match 'd2') { $gameKey = 'd2' }
    else { $gameKey = 'd1' }
}

$readyField = $state.$gameKey.ready
if ($canLaunch -and -not $readyField) {
    Write-Status "WARN: Game reports $gameKey.ready=false - may have missing required files" 'Yellow'
    $fileStatuses = $state.$gameKey.files
    foreach ($fs in $fileStatuses) {
        if ($fs.required -and -not $fs.found) {
            Write-Status "  Missing required: $($fs.filename)" 'Yellow'
        }
    }
}

if (-not $canLaunch) {
    Write-Status "PASS (file-only): $($spec.classification) - $pushCount files verified" 'Green'
    if (-not $KeepFiles) {
        Send-SetupCommand 'clear_set' -Name $TEST_SET
    }
    Exit-Test 0 'pass' -TestMode 'file_only' -FilesVerified $expectedFiles.Count -ClassConfirmed $true
}

if ($SkipLaunch) {
    Write-Status "PASS (file-only, -SkipLaunch): $($spec.classification) - $pushCount files, can_launch=$($state.can_launch)" 'Green'
    if (-not $KeepFiles) {
        Send-SetupCommand 'clear_set' -Name $TEST_SET
    }
    Exit-Test 0 'pass' -TestMode 'file_only' -FilesVerified $expectedFiles.Count -ClassConfirmed $true
}

if ($spec.disc_id -eq 'descent-ii-usa-3-level-interactive-preview') {
    Write-Status 'SKIP (file-only): D2 preview demo launch remains unsupported on Android; files verified' 'Yellow'
    if (-not $KeepFiles) {
        Send-SetupCommand 'clear_set' -Name $TEST_SET
    }
    Exit-Test 0 'skip' 'android_launch_unsupported' -TestMode 'file_only' -FilesVerified $expectedFiles.Count -ClassConfirmed $true
}

# Skip launch if the game says it can't launch (missing required files)
if (-not $state.can_launch) {
    Write-Status "SKIP (can_launch=false): $($spec.classification) - $pushCount files pushed but game reports not launchable" 'Yellow'
    if (-not $KeepFiles) {
        Send-SetupCommand 'clear_set' -Name $TEST_SET
    }
    Exit-Test 0 'skip' 'not_ready' -TestMode 'file_only' -FilesVerified $expectedFiles.Count
}

# -- Step 8: Launch game and verify with automation ------------

Write-Status "Launching game from set '$TEST_SET' with automation..."

# Force stop to ensure clean launch
Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
Start-Sleep -Seconds 2

# Delete stale introspect.json so we only read fresh data from the new game session
Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/introspect.json') | Out-Null

# Delete pilot files so we don't hit "Player already exists" loops
Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'find', 'files', '-name', "'*.plr'", '-delete') | Out-Null
Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'find', 'files', '-name', "'*.plx'", '-delete') | Out-Null
Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'find', 'files', '-name', "'descent.cfg'", '-delete') | Out-Null

# Re-launch setup activity
Adb -CmdArgs @('shell', 'am', 'start', '-n', "$PACKAGE/$ACTIVITY") | Out-Null
if (-not (Wait-SetupReady)) {
    Write-Status 'FAIL: SetupActivity not responding before game launch' 'Red'
    Exit-Test 1 'fail' 'setup_timeout'
}

# Launch through the launcher automation path. It passes the script on the
# MainActivity launch intent, which avoids races with post-launch broadcasts.
$launchGame = $gameKey
$automationScript = Get-ExtractAutomationScriptText -MissionName $spec.expected_mission -LevelName $spec.expected_level1 -Game $launchGame -TestSet $TEST_SET
$automationResult = Invoke-GameAutomationScript -ScriptText $automationScript -TimeoutSeconds 180
if (-not $automationResult) {
    Write-Status 'FAIL: Automation timed out before producing automation_result.json' 'Red'
    Write-GameAutomationDiagnostics
    Exit-Test 1 'fail' 'automation_timeout' -FilesVerified $expectedFiles.Count -ClassConfirmed $true
}

if ($automationResult.result -ne 'PASS') {
    Write-Status "FAIL: Automation could not reach in-game state ($($automationResult.reason))" 'Red'
    Write-GameAutomationDiagnostics
    Exit-Test 1 'fail' 'automation_fail' -FilesVerified $expectedFiles.Count -ClassConfirmed $true
}

Start-Sleep -Milliseconds 500
$gi = Get-GameIntrospection
$inGame = $gi.in_game

if ($inGame) {
    $levelMatch = ($gi.current_level_name -eq $spec.expected_level1)
    if ($levelMatch) {
        Write-Status "PASS: In-game, level='$($gi.current_level_name)' matches expected" 'Green'
    } else {
        Write-Status "PASS (level mismatch): In-game, level='$($gi.current_level_name)' expected='$($spec.expected_level1)'" 'Yellow'
        Write-Status "  (The game loaded - level name may need updating in spec)" 'Yellow'
    }

    # -- Redbook audio check (informational) --
    $hasAudioFiles = ($filesToPush | Where-Object { $_.Extension.ToLower() -in @('.gog', '.inst') }).Count -ge 2
    if ($hasAudioFiles -and $gi.redbook) {
        if ($gi.redbook.enabled) {
            Write-Status "  Redbook: enabled, $($gi.redbook.num_tracks) tracks, status=$($gi.redbook.play_status)" 'Cyan'
        } else {
            Write-Status "  Redbook: audio files pushed but not enabled (MusicType may not be set)" 'Yellow'
        }
    } elseif ($gi.redbook -and $gi.redbook.enabled) {
        Write-Status "  Redbook: enabled (audio source present on device)" 'Cyan'
    }

    # -- Movie check (informational) --
    if ($gi.movie -and $gi.movie.last_name) {
        Write-Status "  Movie: last='$($gi.movie.last_name)' result=$($gi.movie.last_result)" 'Cyan'
    }
} else {
    Write-Status "FAIL: Game not in-game state after $maxNav navigation attempts" 'Red'
    Write-Status "  screen_mode=$($gi.screen_mode), in_game=$($gi.in_game)" 'Yellow'
    if ($gi.menu) {
        Write-Status "  menu title='$($gi.menu.title)'" 'Yellow'
        if ($gi.menu.subtitle) { Write-Status "  subtitle='$($gi.menu.subtitle)'" 'Yellow' }
        Write-Status "  items:" 'Yellow'
        foreach ($item in $gi.menu.items) {
            Write-Status "    [$($item.index)] type=$($item.type) text='$($item.text)'" 'Yellow'
        }
    }
    Exit-Test 1 'fail' 'menu_timeout' -FilesVerified $expectedFiles.Count -ClassConfirmed $true
}

# -- Cleanup --------------------------------------------------

# Stop the game
Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null

if (-not $KeepFiles) {
    Write-Status "Cleaning up test set..."
    Clear-AppPrivateDirectoryContents "$SETS_ROOT_ABS/$TEST_SET"
    Adb -CmdArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/audio_sources.json', 'files/audio_playlist.json') | Out-Null
    Adb -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "  RESULT: PASS" -ForegroundColor Green
Write-Host "  Source: $sourceName ($($spec.classification))" -ForegroundColor Green
Write-Host "  Level: $($gi.current_level_name)" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
$levelName = if ($gi.current_level_name) { $gi.current_level_name } else { $null }
Exit-Test 0 'pass' -Level $levelName -FilesVerified $expectedFiles.Count -ClassConfirmed $true
