#!/usr/bin/env pwsh
param(
    [string]$MissionZip = "game_data\mission_files\plutonia.zip",
    [string]$MissionName = "plutonia",
    [string]$DataDir = "game_data_to_copy_to_emulator\temp",
    [string]$Exe = "buildd2\main\dxx-redux-d2-headless-metadata.exe",
    [string]$OutputPath = "temp\coop_start_fanout_mapset.json",
    [int]$AssertSlots = 8
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)

function Resolve-RepoPath {
    param([Parameter(Mandatory)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $repoRoot $Path
}

function Stage-MissionZip {
    param(
        [Parameter(Mandatory)][string]$ZipPath,
        [Parameter(Mandatory)][string]$StageDir
    )

    Remove-Item -Recurse -Force $StageDir -ErrorAction SilentlyContinue
    $missionsDir = Join-Path $StageDir "missions"
    New-Item -ItemType Directory -Force -Path $missionsDir | Out-Null

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        foreach ($entry in $zip.Entries) {
            if (-not $entry.Name) {
                continue
            }
            if ($entry.FullName -match "[/\\]") {
                continue
            }
            [IO.Compression.ZipFileExtensions]::ExtractToFile(
                $entry,
                (Join-Path $missionsDir $entry.Name),
                $true)
        }
    } finally {
        $zip.Dispose()
    }
}

function Get-PositionKey {
    param([Parameter(Mandatory)]$Start)

    return (@($Start.pos) -join ",")
}

$zipPath = Resolve-RepoPath $MissionZip
$dataPath = Resolve-RepoPath $DataDir
$exePath = Resolve-RepoPath $Exe
$outPath = Resolve-RepoPath $OutputPath
$stageDir = Resolve-RepoPath "temp\coop_start_fanout_mapset_stage"
$logPath = Resolve-RepoPath "temp\coop_start_fanout_mapset.log"

if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
    throw "Mission ZIP not found: $zipPath"
}
if (-not (Test-Path -LiteralPath $dataPath -PathType Container)) {
    throw "D2 data directory not found: $dataPath"
}
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "D2 coop start dump executable not found: $exePath"
}

Stage-MissionZip -ZipPath $zipPath -StageDir $stageDir
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outPath) | Out-Null

$logLines = & $exePath -hogdir $dataPath -extra-dir $stageDir -mission $MissionName -coop-starts-json-out $outPath 2>&1
$dumpExitCode = $LASTEXITCODE
[IO.File]::WriteAllText($logPath, ($logLines -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
if ($dumpExitCode -ne 0) {
    Get-Content -LiteralPath $logPath -ErrorAction SilentlyContinue | Select-Object -Last 80 | ForEach-Object {
        Write-Host $_
    }
    throw "Coop start dump failed for $MissionName"
}

$dump = Get-Content -LiteralPath $outPath -Raw | ConvertFrom-Json
$failures = @()

foreach ($level in @($dump.levels)) {
    $starts = @($level.starts)
    if ([int]$level.num_net_player_positions -ne $AssertSlots) {
        $failures += "level $($level.level_num) $($level.level_name): num_net_player_positions=$($level.num_net_player_positions)"
        continue
    }
    if ([int]$level.too_close_pairs -ne 0) {
        $failures += "level $($level.level_num) $($level.level_name): $($level.too_close_pairs) start pairs closer than $($level.minimum_allowed_distance)"
    }
    $seen = @{}
    $limit = [Math]::Min($AssertSlots, $starts.Count)
    for ($index = 0; $index -lt $limit; ++$index) {
        $key = Get-PositionKey -Start $starts[$index]
        if ($seen.ContainsKey($key)) {
            $failures += "level $($level.level_num) $($level.level_name): slot $index overlaps an earlier start at $key"
        } else {
            $seen[$key] = $true
        }
    }
}

if ($failures.Count) {
    $failures | ForEach-Object { Write-Host "FAIL: $_" }
    throw "Coop start fanout assertions failed"
}

$levelCount = @($dump.levels).Count
Write-Host "PASS: $MissionName coop start fanout generated $AssertSlots distinct starts on $levelCount levels"
