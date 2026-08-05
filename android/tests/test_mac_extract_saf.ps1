#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Verifies Mac CD extraction through seekable and pipe-backed content URIs

.DESCRIPTION
    Stages the known MacPlay D1 image behind the debug SAF provider, injects
    those URIs through the same callback used by the system picker, checks the
    extracted launcher set, and proves the pipe descriptor staging fallback ran
#>
param(
    [string]$CuePath = "",
    [string]$BinPath = "",
    [switch]$SkipBuild,
    [int]$TimeoutSeconds = 150
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$defaultMediaDir = Join-Path $repoRoot "game_data\CD images\Descent - Mac macplay"
if (-not $CuePath) { $CuePath = Join-Path $defaultMediaDir "Descent - Mac macplay.cue" }
if (-not $BinPath) { $BinPath = Join-Path $defaultMediaDir "Descent - Mac macplay.bin" }

$expectedCueHash = "b8127c0ce27a4573b596c5cfb78af0f0e7ce58e1dbe130e1424f3ede09d4446d"
$expectedBinHash = "38393a12630bbdfdd2e2efac138da463c242586a42c889b91054fabf6ba7b925"
$providerDir = "files/saf_provider_source"
$testSet = "mac-saf-extract-test"
$deviceCue = "$providerDir/macplay.cue"
$deviceBin = "$providerDir/macplay.bin"
$tmpCue = Join-Path $repoRoot "android\temp\mac_extract_saf.cue"
$expectedFiles = @("CHAOS.HOG", "CHAOS.MSN", "demo1.dem", "descent.hog", "descent.pig", "watchme.dem", "yep9.dem")

function Invoke-SetupCommand {
    param([Parameter(Mandatory)][string]$Command, [string]$Name = "")
    $args_ = @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", $Command)
    if ($Name) { $args_ += @("--es", "name", $Name) }
    Adb -AdbArgs $args_ | Out-Null
}

function Get-UiDump {
    Adb-Timeout -AdbArgs @("shell", "uiautomator", "dump", "/sdcard/mac_extract_saf.xml") -Seconds 10 | Out-Null
    return Adb-Timeout -AdbArgs @("shell", "cat", "/sdcard/mac_extract_saf.xml") -Seconds 5
}

function Wait-UiText {
    param([Parameter(Mandatory)][string]$Text, [int]$Seconds = 20)
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        $xml = Get-UiDump
        if ($xml -and $xml.Contains("text=`"$Text`"")) { return $true }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

function Tap-UiText {
    param([Parameter(Mandatory)][string]$Text)
    $xml = Get-UiDump
    $escaped = [regex]::Escape($Text)
    $match = [regex]::Match($xml, "text=`"$escaped`"[^>]*bounds=`"\[(\d+),(\d+)\]\[(\d+),(\d+)\]`"")
    if (-not $match.Success) { throw "Could not find enabled UI text: $Text" }
    $x = ([int]$match.Groups[1].Value + [int]$match.Groups[3].Value) / 2
    $y = ([int]$match.Groups[2].Value + [int]$match.Groups[4].Value) / 2
    Adb -AdbArgs @("shell", "input", "tap", [int]$x, [int]$y) | Out-Null
}

function Test-ExpectedSetFiles {
    param($State)
    $actual = @($State.set_files)
    foreach ($name in $expectedFiles) {
        if ($actual -notcontains $name) { return $false }
    }
    return $true
}

function Invoke-SafMode {
    param([Parameter(Mandatory)][ValidateSet("seekable", "pipe")][string]$Mode)
    Write-Status "Testing Mac extraction through $Mode content URIs"
    Invoke-SetupCommand -Command "clear_set" -Name $testSet
    if (-not (Wait-SetupCondition -TimeoutSeconds 15 -PollMs 300 -Predicate { param($state) @($state.set_files).Count -eq 0 })) {
        throw "Test set did not become empty before the $Mode import"
    }
    Adb -AdbArgs @("logcat", "-c") | Out-Null
    $uris = "content://com.dxxredux.app.saf-test/$Mode/macplay.cue,content://com.dxxredux.app.saf-test/$Mode/macplay.bin"
    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
        "--es", "command", "import_picked_uris", "--esa", "uris", $uris
    ) | Out-Null
    if (-not (Wait-UiText -Text "Extract Game Files")) { throw "$Mode import dialog did not become ready" }
    Tap-UiText -Text "Extract Game Files"
    if (-not (Wait-SetupCondition -TimeoutSeconds $TimeoutSeconds -PollMs 1000 -Predicate { param($state) Test-ExpectedSetFiles $state })) {
        throw "$Mode content URI extraction did not publish the expected Mac files"
    }

    $stageLine = Adb -AdbArgs @("logcat", "-d", "-v", "brief", "DXX-SAF-Stage:I", "*:S")
    if ($Mode -eq "pipe" -and $stageLine -notmatch 'Staged nonseekable SAF source macplay.bin \(718912320 bytes\)') {
        throw "Pipe extraction succeeded without the expected descriptor staging evidence"
    }
    if ($Mode -eq "seekable" -and $stageLine -match 'Staged nonseekable SAF source') {
        throw "Seekable provider unexpectedly used descriptor staging"
    }
    Write-Status "PASS: $Mode content URI Mac extraction" "Green"
    if (Wait-UiText -Text "Done" -Seconds 10) { Tap-UiText -Text "Done" }
}

foreach ($path in @($CuePath, $BinPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required MacPlay media not found: $path" }
}
if ((Get-FileHash -LiteralPath $CuePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedCueHash) {
    throw "Unexpected MacPlay CUE hash: $CuePath"
}
if ((Get-FileHash -LiteralPath $BinPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedBinHash) {
    throw "Unexpected MacPlay BIN hash: $BinPath"
}

Ensure-EmulatorHealthy
$exitCode = 1
try {
    if (-not $SkipBuild) {
        $env:JAVA_HOME = "C:\local\jdk-21"
        $env:Path = "$env:JAVA_HOME\bin;$env:Path"
        Push-Location (Join-Path $repoRoot "android")
        try { & .\gradlew.bat :app:assembleDebug } finally { Pop-Location }
        if ($LASTEXITCODE -ne 0) { throw "Debug APK build failed" }
    }
    $apk = Join-Path $repoRoot "android\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path -LiteralPath $apk -PathType Leaf)) { throw "Debug APK not found: $apk" }
    Adb -AdbArgs @("install", "-r", $apk) -Seconds 120 | Out-Null

    # Earlier mission tests can leave hundreds of MiB of disposable staged music
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-rf", "cache/mission_zip_music") | Out-Null

    New-Item -ItemType Directory -Force (Split-Path $tmpCue) | Out-Null
    $cueText = [IO.File]::ReadAllText($CuePath) -replace '(?m)^FILE\s+"[^"]+"\s+BINARY\s*$', 'FILE "macplay.bin" BINARY'
    [IO.File]::WriteAllText($tmpCue, $cueText, [Text.Encoding]::ASCII)
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-rf", $providerDir) | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $providerDir) | Out-Null
    Adb -AdbArgs @("push", $tmpCue, "/data/local/tmp/macplay.cue") -Seconds 30 | Out-Null
    Adb -AdbArgs @("push", $BinPath, "/data/local/tmp/macplay.bin") -Seconds 120 | Out-Null
    $sourceBinSize = (Get-Item -LiteralPath $BinPath).Length
    $stagedBinSize = Adb-Timeout -AdbArgs @("shell", "stat", "-c", "%s", "/data/local/tmp/macplay.bin") -Seconds 5
    if (-not ($stagedBinSize -match '^\d+$') -or [long]$stagedBinSize -ne $sourceBinSize) {
        throw "MacPlay BIN push was incomplete: expected $sourceBinSize bytes, got $stagedBinSize"
    }
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/macplay.cue", $deviceCue) | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/macplay.bin", $deviceBin) -Seconds 120 | Out-Null
    $providerBinSize = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "stat", "-c", "%s", $deviceBin) -Seconds 5
    if (-not ($providerBinSize -match '^\d+$') -or [long]$providerBinSize -ne $sourceBinSize) {
        throw "MacPlay BIN app staging was incomplete: expected $sourceBinSize bytes, got $providerBinSize"
    }
    Adb -AdbArgs @("shell", "rm", "-f", "/data/local/tmp/macplay.cue", "/data/local/tmp/macplay.bin") | Out-Null

    Stop-AppAndWait
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json") | Out-Null
    Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
    if (-not (Wait-SetupCondition -TimeoutSeconds 30 -Predicate { param($state) $state.screen -eq "setup" })) {
        throw "SetupActivity did not become ready"
    }
    Invoke-SetupCommand -Command "create_set" -Name $testSet
    if (-not (Wait-SetupCondition -TimeoutSeconds 15 -PollMs 300 -Predicate { param($state) @($state.sets.name) -contains $testSet })) {
        throw "Test set was not created"
    }
    Invoke-SetupCommand -Command "switch_set" -Name $testSet
    if (-not (Wait-SetupCondition -TimeoutSeconds 15 -PollMs 300 -Predicate { param($state) $state.active_set -eq $testSet })) {
        throw "Test set did not become active"
    }
    Stop-AppAndWait
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json") | Out-Null
    Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
    if (-not (Wait-SetupCondition -TimeoutSeconds 30 -Predicate { param($state) $state.active_set -eq $testSet })) {
        throw "SetupActivity did not restart on the test set"
    }
    Invoke-SafMode -Mode "seekable"
    Invoke-SafMode -Mode "pipe"
    $exitCode = 0
} finally {
    try { Invoke-SetupCommand -Command "switch_set" -Name "default" } catch {}
    try { Invoke-SetupCommand -Command "clear_set" -Name $testSet } catch {}
    try { Stop-AppAndWait } catch {}
    try { Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-rf", $providerDir) | Out-Null } catch {}
    try { Adb -AdbArgs @("shell", "rm", "-f", "/sdcard/mac_extract_saf.xml", "/data/local/tmp/macplay.cue", "/data/local/tmp/macplay.bin") | Out-Null } catch {}
    Remove-Item -LiteralPath $tmpCue -Force -ErrorAction SilentlyContinue
}

exit $exitCode
