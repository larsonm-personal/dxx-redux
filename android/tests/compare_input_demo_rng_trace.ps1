#!/usr/bin/env pwsh
param(
    [Parameter(Mandatory = $true)]
    [string]$ExpectedPath,
    [Parameter(Mandatory = $true)]
    [string]$ActualPath,
    [int]$ContextLines = 3
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)

function Get-RelativeRepoPath {
    param([string]$Path)

    try {
        return [System.IO.Path]::GetRelativePath($repoRoot, $Path)
    } catch {
        return $Path
    }
}

function Get-LineSummary {
    param([string]$Line)

    if ([string]::IsNullOrWhiteSpace($Line)) {
        return $null
    }
    try {
        $record = $Line | ConvertFrom-Json -AsHashtable
    } catch {
        return $null
    }

    $parts = New-Object System.Collections.Generic.List[string]
    foreach ($key in @(
            'type',
            'seq',
            'frame',
            'gt',
            'call_count',
            'stream',
            'has_context',
            'ctx_obj',
            'ctx_sig',
            'ctx_id',
            'state_before',
            'state_after',
            'seed',
            'result',
            'file',
            'func',
            'line')) {
        if ($record.ContainsKey($key)) {
            $parts.Add("${key}=$($record[$key])")
        }
    }
    if ($parts.Count -eq 0) {
        return $null
    }
    return $parts -join ' '
}

function Convert-JsonLineToRecord {
    param([string]$Line)

    if ([string]::IsNullOrWhiteSpace($Line)) {
        return $null
    }
    try {
        return $Line | ConvertFrom-Json -AsHashtable
    } catch {
        return $null
    }
}

function Test-IsFalseLike {
    param([object]$Value)

    if ($Value -is [bool]) {
        return -not $Value
    }
    if ($null -eq $Value) {
        return $false
    }
    return @('0', 'false') -contains $Value.ToString().ToLowerInvariant()
}

function Test-IsMetaLine {
    param([string]$Line)

    return (Get-LineSummary -Line $Line) -eq 'type=meta'
}

function Test-IsSupplementalEvent {
    param([string]$Line)

    $record = Convert-JsonLineToRecord -Line $Line
    if (-not $record) {
        return $false
    }
    if ($record.ContainsKey('type') -and $record.type -eq 'meta') {
        return $false
    }
    if ($record.ContainsKey('stream') -and [int]$record.stream -ne 0) {
        return $true
    }
    if ($record.ContainsKey('has_context') -and (Test-IsFalseLike -Value $record.has_context)) {
        return $true
    }
    return $false
}

function Get-ComparableTrace {
    param([string[]]$Lines)

    $eventLines = New-Object System.Collections.Generic.List[string]
    $metaLine = $null
    $skippedCount = 0

    for ($index = 0; $index -lt $Lines.Length; $index++) {
        $line = $Lines[$index]
        if ($index -eq 0 -and (Test-IsMetaLine -Line $line)) {
            $metaLine = $line
            continue
        }
        if (Test-IsSupplementalEvent -Line $line) {
            $skippedCount++
            continue
        }
        $eventLines.Add($line)
    }

    return [ordered]@{
        MetaLine = $metaLine
        EventLines = $eventLines.ToArray()
        SkippedCount = $skippedCount
    }
}

function ConvertTo-ComparableJson {
    param([hashtable]$Record)

    return ($Record | ConvertTo-Json -Compress -Depth 10)
}

function Test-MetaMismatch {
    param(
        [string]$ExpectedLine,
        [string]$ActualLine
    )

    if (-not $ExpectedLine -and -not $ActualLine) {
        return $null
    }

    $expectedRecord = Convert-JsonLineToRecord -Line $ExpectedLine
    $actualRecord = Convert-JsonLineToRecord -Line $ActualLine
    if (-not $expectedRecord -or -not $actualRecord) {
        if ($ExpectedLine -ceq $ActualLine) {
            return $null
        }
        return [ordered]@{
            Expected = $ExpectedLine
            Actual = $ActualLine
        }
    }

    if ($expectedRecord.ContainsKey('events')) {
        $null = $expectedRecord.Remove('events')
    }
    if ($actualRecord.ContainsKey('events')) {
        $null = $actualRecord.Remove('events')
    }
    if ((ConvertTo-ComparableJson -Record $expectedRecord) -eq (ConvertTo-ComparableJson -Record $actualRecord)) {
        return $null
    }
    return [ordered]@{
        Expected = $ExpectedLine
        Actual = $ActualLine
    }
}

function Test-IgnorableMismatch {
    param(
        [string]$ExpectedLine,
        [string]$ActualLine
    )

    $expectedRecord = Convert-JsonLineToRecord -Line $ExpectedLine
    $actualRecord = Convert-JsonLineToRecord -Line $ActualLine
    if (-not $expectedRecord -or -not $actualRecord) {
        return $null
    }

    $lineChanged = $false
    $seqChanged = $false
    $expectedLineNumber = $null
    $actualLineNumber = $null
    $expectedSeq = $null
    $actualSeq = $null

    if ($expectedRecord.ContainsKey('line') -and $actualRecord.ContainsKey('line')) {
        $expectedLineNumber = $expectedRecord.line
        $actualLineNumber = $actualRecord.line
        $lineChanged = $expectedLineNumber -ne $actualLineNumber
        $null = $expectedRecord.Remove('line')
        $null = $actualRecord.Remove('line')
    }

    if ($expectedRecord.ContainsKey('seq') -and $actualRecord.ContainsKey('seq')) {
        $expectedSeq = $expectedRecord.seq
        $actualSeq = $actualRecord.seq
        $seqChanged = $expectedSeq -ne $actualSeq
        $null = $expectedRecord.Remove('seq')
        $null = $actualRecord.Remove('seq')
    }

    if (-not $lineChanged -and -not $seqChanged) {
        return $null
    }
    if ((ConvertTo-ComparableJson -Record $expectedRecord) -ne (ConvertTo-ComparableJson -Record $actualRecord)) {
        return $null
    }

    return [ordered]@{
        LineChanged = $lineChanged
        SeqChanged = $seqChanged
        ExpectedLineNumber = $expectedLineNumber
        ActualLineNumber = $actualLineNumber
        ExpectedSeq = $expectedSeq
        ActualSeq = $actualSeq
    }
}

function Write-ContextLines {
    param(
        [string[]]$Lines,
        [int]$MismatchIndex,
        [int]$Count,
        [string]$Label
    )

    if (-not $Lines -or $MismatchIndex -le 0 -or $Count -le 0) {
        return
    }

    $start = [Math]::Max(0, $MismatchIndex - $Count)
    Write-Host "$Label context:"
    for ($index = $start; $index -lt $MismatchIndex; $index++) {
        Write-Host ('  [{0}] {1}' -f ($index + 1), $Lines[$index])
    }
}

$resolvedExpectedPath = (Resolve-Path -LiteralPath $ExpectedPath).Path
$resolvedActualPath = (Resolve-Path -LiteralPath $ActualPath).Path
$expectedRawLines = [System.IO.File]::ReadAllLines($resolvedExpectedPath)
$actualRawLines = [System.IO.File]::ReadAllLines($resolvedActualPath)
$expectedTrace = Get-ComparableTrace -Lines $expectedRawLines
$actualTrace = Get-ComparableTrace -Lines $actualRawLines
$expectedLines = $expectedTrace.EventLines
$actualLines = $actualTrace.EventLines
$metaMismatch = Test-MetaMismatch -ExpectedLine $expectedTrace.MetaLine -ActualLine $actualTrace.MetaLine

$sharedCount = [Math]::Min($expectedLines.Length, $actualLines.Length)
$mismatchIndex = -1
$ignorableMismatch = $null

for ($index = 0; $index -lt $sharedCount; $index++) {
    if ($expectedLines[$index] -cne $actualLines[$index]) {
        $ignorableDetail = Test-IgnorableMismatch -ExpectedLine $expectedLines[$index] -ActualLine $actualLines[$index]
        if ($ignorableDetail) {
            if (-not $ignorableMismatch) {
                $ignorableMismatch = [ordered]@{
                    LineNumber = $index + 1
                    LineChanged = $ignorableDetail.LineChanged
                    SeqChanged = $ignorableDetail.SeqChanged
                    ExpectedLineNumber = $ignorableDetail.ExpectedLineNumber
                    ActualLineNumber = $ignorableDetail.ActualLineNumber
                    ExpectedSeq = $ignorableDetail.ExpectedSeq
                    ActualSeq = $ignorableDetail.ActualSeq
                    ExpectedLine = $expectedLines[$index]
                    ActualLine = $actualLines[$index]
                }
            }
            continue
        }
        $mismatchIndex = $index
        break
    }
}

if ($mismatchIndex -lt 0 -and $expectedLines.Length -ne $actualLines.Length) {
    $mismatchIndex = $sharedCount
}

Write-Host "Expected: $(Get-RelativeRepoPath -Path $resolvedExpectedPath)"
Write-Host "Actual: $(Get-RelativeRepoPath -Path $resolvedActualPath)"
Write-Host "Expected lines: comparable=$($expectedLines.Length) skipped=$($expectedTrace.SkippedCount) raw=$($expectedRawLines.Length)"
Write-Host "Actual lines: comparable=$($actualLines.Length) skipped=$($actualTrace.SkippedCount) raw=$($actualRawLines.Length)"

if ($mismatchIndex -lt 0) {
    if ($metaMismatch) {
        Write-Host 'RESULT: FAIL'
        Write-Host 'Meta line differs even though all event lines matched'
        Write-Host "Expected meta: $($metaMismatch.Expected)"
        Write-Host "Actual meta: $($metaMismatch.Actual)"
        exit 1
    }
    if ($ignorableMismatch -and $ignorableMismatch.LineChanged) {
        Write-Host 'Note: ignored source-line-only differences'
        Write-Host "First differing line: $($ignorableMismatch.LineNumber)"
        Write-Host "Expected source line: $($ignorableMismatch.ExpectedLineNumber)"
        Write-Host "Actual source line: $($ignorableMismatch.ActualLineNumber)"
    }
    if ($ignorableMismatch -and $ignorableMismatch.SeqChanged) {
        Write-Host "Note: ignored sequence-only differences starting at comparable line $($ignorableMismatch.LineNumber)"
    }
    Write-Host 'RESULT: PASS'
    exit 0
}

$lineNumber = $mismatchIndex + 1
$expectedLine = if ($mismatchIndex -lt $expectedLines.Length) { $expectedLines[$mismatchIndex] } else { '<missing>' }
$actualLine = if ($mismatchIndex -lt $actualLines.Length) { $actualLines[$mismatchIndex] } else { '<missing>' }
$expectedSummary = Get-LineSummary -Line $expectedLine
$actualSummary = Get-LineSummary -Line $actualLine

Write-Host 'RESULT: FAIL'
if ($metaMismatch) {
    Write-Host 'Meta line differs:'
    Write-Host "Expected meta: $($metaMismatch.Expected)"
    Write-Host "Actual meta: $($metaMismatch.Actual)"
}
if ($ignorableMismatch) {
    if ($ignorableMismatch.LineChanged) {
        Write-Host "First source-line-only mismatch: line $($ignorableMismatch.LineNumber) expected_line=$($ignorableMismatch.ExpectedLineNumber) actual_line=$($ignorableMismatch.ActualLineNumber)"
    }
    if ($ignorableMismatch.SeqChanged) {
        Write-Host "First sequence-only mismatch: line $($ignorableMismatch.LineNumber) expected_seq=$($ignorableMismatch.ExpectedSeq) actual_seq=$($ignorableMismatch.ActualSeq)"
    }
}
Write-Host "First differing line: $lineNumber"
if ($expectedSummary) {
    Write-Host "Expected summary: $expectedSummary"
}
if ($actualSummary) {
    Write-Host "Actual summary: $actualSummary"
}
Write-ContextLines -Lines $expectedLines -MismatchIndex $mismatchIndex -Count $ContextLines -Label 'Expected'
Write-ContextLines -Lines $actualLines -MismatchIndex $mismatchIndex -Count $ContextLines -Label 'Actual'
Write-Host "Expected line: $expectedLine"
Write-Host "Actual line: $actualLine"
exit 1
