#!/usr/bin/env pwsh
param(
    [Parameter(Mandatory = $true)]
    [string]$ExpectedPath,
    [Parameter(Mandatory = $true)]
    [string]$ActualPath,
    [int]$StartFrame = -1,
    [int]$EndFrame = -1,
    [int]$MaxMismatches = 20,
    [switch]$CompareFrameMetadata
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)

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

    return $Line.Trim() | ConvertFrom-Json -AsHashtable
}

function Get-RelativeRepoPath {
    param([string]$Path)

    try {
        return [System.IO.Path]::GetRelativePath($repoRoot, $Path)
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

function Format-CompareValue {
    param([object]$Value)

    if ($null -eq $Value) {
        return '<null>'
    }
    if ($Value -is [System.Collections.IDictionary] -or ($Value -is [System.Collections.IList] -and -not ($Value -is [string]))) {
        return ($Value | ConvertTo-Json -Compress -Depth 32)
    }
    return [string]$Value
}

function Compare-JsonDiff {
    param(
        [object]$Expected,
        [object]$Actual,
        [string]$Path = 'state'
    )

    $diffs = New-Object System.Collections.Generic.List[string]

    if ($Expected -is [System.Collections.IDictionary]) {
        if (-not ($Actual -is [System.Collections.IDictionary])) {
            $diffs.Add("${Path}: expected object, actual $(Format-CompareValue -Value $Actual)")
            return $diffs
        }

        foreach ($key in $Expected.Keys) {
            if (-not $Actual.Contains($key)) {
                $diffs.Add("${Path}.${key}: missing from actual trace")
            }
        }
        foreach ($key in $Actual.Keys) {
            if (-not $Expected.Contains($key)) {
                $diffs.Add("${Path}.${key}: extra in actual trace = $(Format-CompareValue -Value $Actual[$key])")
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
        for ($index = 0; $index -lt $maxShared; $index++) {
            $nested = Compare-JsonDiff -Expected $Expected[$index] -Actual $Actual[$index] -Path "${Path}[$index]"
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

function Get-MismatchStageLabel {
    param(
        [string]$StateError,
        [hashtable]$ActualFrame,
        [hashtable]$PreviousActualFrame
    )

    $hasDiag = $ActualFrame -and $ActualFrame.Contains('diag') -and ($ActualFrame.diag -is [System.Collections.IDictionary])
    $hasPrevDiag = $PreviousActualFrame -and $PreviousActualFrame.Contains('diag') -and ($PreviousActualFrame.diag -is [System.Collections.IDictionary])

    if ($hasDiag -and $hasPrevDiag) {
        $diag = $ActualFrame.diag
        $prevDiag = $PreviousActualFrame.diag
        $awarenessChanged = ($diag.Contains('awareness_events') -and $prevDiag.Contains('awareness_events') -and [int]$diag.awareness_events -ne [int]$prevDiag.awareness_events)
        $cameraAwakeChanged = ($diag.Contains('camera_awake_robots') -and $prevDiag.Contains('camera_awake_robots') -and [int]$diag.camera_awake_robots -ne [int]$prevDiag.camera_awake_robots)
        $dangerChanged = ($diag.Contains('danger_laser_robots') -and $prevDiag.Contains('danger_laser_robots') -and [int]$diag.danger_laser_robots -ne [int]$prevDiag.danger_laser_robots)

        if ($awarenessChanged) {
            return 'awareness_transition'
        }
        if ($cameraAwakeChanged -or $dangerChanged) {
            return 'robot_wake_transition'
        }
    }

    if ($StateError -match '^state\.level_summary\.(robots_alive|robots_killed)') {
        return 'robot_lifecycle'
    }
    if ($StateError -match '^state\.position\.') {
        return 'motion'
    }
    if ($StateError -match '^state\.player0\.(score|energy|shields|lives)') {
        return 'combat_or_damage'
    }
    if ($StateError -match '^state\.game_time64') {
        return 'frame_phase'
    }
    return 'unknown'
}

function Read-StateTraceFrames {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "State trace not found: $Path"
    }

    $frames = [ordered]@{}
    $lastFrameTime = $null
    foreach ($line in [System.IO.File]::ReadLines((Resolve-Path -LiteralPath $Path).Path)) {
        if (-not (Test-JsonRecordLine -Line $line)) {
            continue
        }
        $record = ConvertFrom-JsonLine -Line $line
        if ($record.type -eq 'frame') {
            if ($record.ContainsKey('ft')) {
                $lastFrameTime = [int]$record.ft
            }
            if (-not $record.ContainsKey('state')) {
                continue
            }
            $frame = [int]$record.f
            if (-not (Test-FrameInRange -Frame $frame)) {
                continue
            }
            $item = [ordered]@{
                f = $frame
                state = $record.state
            }
            if ($null -ne $lastFrameTime) {
                $item.ft = $lastFrameTime
            }
            if ($record.ContainsKey('rng')) {
                $item.rng = $record.rng
            }
            if ($record.ContainsKey('diag')) {
                $item.diag = $record.diag
            }
            $frames[[string]$frame] = $item
            continue
        }
        if ($record.type -ne 'frame_state' -and $record.type -ne 'replay_state') {
            continue
        }
        if (-not $record.ContainsKey('state')) {
            continue
        }
        $traceFrame = [int]$record.f
        if (-not (Test-FrameInRange -Frame $traceFrame)) {
            continue
        }
        $frames[[string]$traceFrame] = $record
    }
    return $frames
}

$expectedFrames = Read-StateTraceFrames -Path $ExpectedPath
$actualFrames = Read-StateTraceFrames -Path $ActualPath
$mismatches = New-Object System.Collections.Generic.List[string]
$compared = 0

foreach ($frameKey in (@($expectedFrames.Keys) | Sort-Object { [int]$_ })) {
    $frame = [int]$frameKey
    $expected = $expectedFrames[$frameKey]
    $frameLookupKey = [string]$frame
    if (-not $actualFrames.Contains($frameLookupKey)) {
        $mismatches.Add("frame=$frame missing from actual trace")
        if ($mismatches.Count -ge $MaxMismatches) {
            break
        }
        continue
    }
    $actual = $actualFrames[$frameLookupKey]
    $previousActual = $null
    $previousLookupKey = [string]($frame - 1)
    if ($actualFrames.Contains($previousLookupKey)) {
        $previousActual = $actualFrames[$previousLookupKey]
    }
    $compared++

    if ($CompareFrameMetadata) {
        foreach ($key in @('ft', 'rng')) {
            if ($expected.Contains($key)) {
                $metadataDiffs = Compare-JsonDiff -Expected $expected[$key] -Actual $actual[$key] -Path $key
                if ($metadataDiffs.Count -gt 0) {
                    foreach ($metadataDiff in $metadataDiffs) {
                        $mismatches.Add("frame=$frame $metadataDiff")
                        if ($mismatches.Count -ge $MaxMismatches) {
                            break
                        }
                    }
                    break
                }
            }
        }
    }
    if ($mismatches.Count -ge $MaxMismatches) {
        break
    }

    $stateDiffs = Compare-JsonDiff -Expected $expected.state -Actual $actual.state -Path 'state'
    if ($stateDiffs.Count -gt 0) {
        $stageLabel = Get-MismatchStageLabel -StateError $stateDiffs[0] -ActualFrame $actual -PreviousActualFrame $previousActual
        foreach ($stateDiff in $stateDiffs) {
            $mismatches.Add("frame=$frame stage=$stageLabel $stateDiff")
            if ($mismatches.Count -ge $MaxMismatches) {
                break
            }
        }
        if ($mismatches.Count -ge $MaxMismatches) {
            break
        }
    }
}

Write-Host "Expected: $(Get-RelativeRepoPath -Path ((Resolve-Path -LiteralPath $ExpectedPath).Path))"
Write-Host "Actual: $(Get-RelativeRepoPath -Path ((Resolve-Path -LiteralPath $ActualPath).Path))"
Write-Host "Compared $compared frame state records"

if ($mismatches.Count -eq 0) {
    Write-Host 'RESULT: PASS'
    exit 0
}

Write-Host 'RESULT: FAIL'
foreach ($mismatch in $mismatches) {
    Write-Host $mismatch
}
exit 1
