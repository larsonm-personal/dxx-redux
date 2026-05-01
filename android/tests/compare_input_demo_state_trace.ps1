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

function Compare-JsonSubset {
    param(
        [object]$Expected,
        [object]$Actual,
        [string]$Path = 'state'
    )

    if ($Expected -is [System.Collections.IDictionary]) {
        if (-not ($Actual -is [System.Collections.IDictionary])) {
            return "${Path}: expected object, actual $(Format-CompareValue -Value $Actual)"
        }
        foreach ($key in $Expected.Keys) {
            if (-not $Actual.Contains($key)) {
                return "${Path}.${key}: missing from actual trace"
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
        for ($index = 0; $index -lt $Expected.Count; $index++) {
            $nested = Compare-JsonSubset -Expected $Expected[$index] -Actual $Actual[$index] -Path "${Path}[$index]"
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
    $compared++

    if ($CompareFrameMetadata) {
        foreach ($key in @('ft', 'rng')) {
            if ($expected.Contains($key)) {
                $metadataError = Compare-JsonSubset -Expected $expected[$key] -Actual $actual[$key] -Path $key
                if ($metadataError) {
                    $mismatches.Add("frame=$frame $metadataError")
                    break
                }
            }
        }
    }
    if ($mismatches.Count -ge $MaxMismatches) {
        break
    }

    $stateError = Compare-JsonSubset -Expected $expected.state -Actual $actual.state -Path 'state'
    if ($stateError) {
        $mismatches.Add("frame=$frame $stateError")
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
