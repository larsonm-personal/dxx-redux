#!/usr/bin/env pwsh
param(
    [Parameter(Mandatory = $true)]
    [string]$DemoPath,
    [string]$OutputPath,
    [int]$StartFrame = -1,
    [int]$EndFrame = -1
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers\powershell_compat.ps1')

function Test-JsonRecordLine {
    param([string]$Line)

    if ($null -eq $Line) {
        return $false
    }
    $trimmed = $Line.Trim()
    return $trimmed.Length -gt 0 -and -not $trimmed.StartsWith('//')
}

function ConvertFrom-JsonLine {
    param([string]$Line)

    return ConvertFrom-CompatibleJsonHashtable -Json $Line.Trim()
}

function Get-RelativeRepoPath {
    param([string]$Path)

    try {
        return Get-CompatibleRelativePath -BasePath $repoRoot -TargetPath $Path
    } catch {
        return $Path
    }
}

function Test-FrameInRange {
    param([int]$Frame)

    if ($StartFrame -ge 0 -and $Frame -lt $StartFrame) {
        return $false
    }
    if ($EndFrame -ge 0 -and $Frame -gt $EndFrame) {
        return $false
    }
    return $true
}

if (-not (Test-Path -LiteralPath $DemoPath)) {
    throw "Demo file not found: $DemoPath"
}

$resolvedDemoPath = (Resolve-Path -LiteralPath $DemoPath).Path
if (-not $OutputPath) {
    $outputRoot = Join-Path $repoRoot 'temp\input_demo_state_traces'
    if (-not (Test-Path -LiteralPath $outputRoot)) {
        New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
    }
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($resolvedDemoPath)
    $OutputPath = Join-Path $outputRoot "$baseName.expected_state.jsonl"
}

$resolvedOutputPath = $OutputPath
$outputParent = Split-Path $resolvedOutputPath
if ($outputParent -and -not (Test-Path -LiteralPath $outputParent)) {
    New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
}

$header = $null
$lastFrameTime = $null
$frameRecordCount = 0
$recordedStateFrameCount = 0
$frameLines = New-Object System.Collections.Generic.List[string]

foreach ($line in [System.IO.File]::ReadLines($resolvedDemoPath)) {
    if (-not (Test-JsonRecordLine -Line $line)) {
        continue
    }
    $record = ConvertFrom-JsonLine -Line $line
    if ($record.type -eq 'header') {
        $header = $record
        continue
    }
    if ($record.type -ne 'frame') {
        continue
    }
    $frameRecordCount++
    if ($record.ContainsKey('ft')) {
        $lastFrameTime = [int]$record.ft
    }
    if ($record.ContainsKey('state')) {
        $recordedStateFrameCount++
    } else {
        continue
    }

    $frame = [int]$record.f
    if (-not (Test-FrameInRange -Frame $frame)) {
        continue
    }

    $outRecord = [ordered]@{
        type = 'frame_state'
        source = 'recorded'
        f = $frame
    }
    if ($null -ne $lastFrameTime) {
        $outRecord.ft = $lastFrameTime
    }
    if ($record.ContainsKey('rng')) {
        $outRecord.rng = $record.rng
    }
    if ($record.ContainsKey('diag')) {
        $outRecord.diag = $record.diag
    }
    $outRecord.state = $record.state
    $frameLines.Add(($outRecord | ConvertTo-Json -Compress -Depth 32))
}

if ($frameRecordCount -eq 0) {
    throw "Demo contains no frame records: $(Get-RelativeRepoPath -Path $resolvedDemoPath)"
}

if ($recordedStateFrameCount -eq 0) {
    throw "Demo contains no recorded per-frame state: $(Get-RelativeRepoPath -Path $resolvedDemoPath)"
}

if ($frameLines.Count -eq 0) {
    throw "No recorded per-frame state records matched the requested frame range for $(Get-RelativeRepoPath -Path $resolvedDemoPath)"
}

$meta = [ordered]@{
    type = 'meta'
    version = 1
    source = 'recorded'
    demo = Get-RelativeRepoPath -Path $resolvedDemoPath
    frames = $frameLines.Count
}
if ($header) {
    foreach ($key in @('game', 'mission', 'level', 'difficulty', 'start_mode', 'frame_count')) {
        if ($header.ContainsKey($key)) {
            $meta[$key] = $header[$key]
        }
    }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add(($meta | ConvertTo-Json -Compress -Depth 10))
foreach ($frameLine in $frameLines) {
    $lines.Add($frameLine)
}

[System.IO.File]::WriteAllLines($resolvedOutputPath, $lines, [System.Text.Encoding]::ASCII)
Write-Host "Wrote $($frameLines.Count) frame state records to $(Get-RelativeRepoPath -Path $resolvedOutputPath)"
