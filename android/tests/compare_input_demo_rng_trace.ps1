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
    foreach ($key in @('type', 'seq', 'frame', 'call_count', 'file', 'func', 'line')) {
        if ($record.ContainsKey($key)) {
            $parts.Add("${key}=$($record[$key])")
        }
    }
    if ($parts.Count -eq 0) {
        return $null
    }
    return $parts -join ' '
}

function Test-IsMetaLine {
    param([string]$Line)

    return (Get-LineSummary -Line $Line) -eq 'type=meta'
}

function ConvertTo-ComparableJson {
    param([hashtable]$Record)

    return ($Record | ConvertTo-Json -Compress -Depth 10)
}

function Test-LineOnlyMismatch {
    param(
        [string]$ExpectedLine,
        [string]$ActualLine
    )

    try {
        $expectedRecord = $ExpectedLine | ConvertFrom-Json -AsHashtable
        $actualRecord = $ActualLine | ConvertFrom-Json -AsHashtable
    } catch {
        return $null
    }

    if (-not $expectedRecord.ContainsKey('line') -or -not $actualRecord.ContainsKey('line')) {
        return $null
    }

    $expectedLineNumber = $expectedRecord.line
    $actualLineNumber = $actualRecord.line
    $null = $expectedRecord.Remove('line')
    $null = $actualRecord.Remove('line')
    if ($expectedLineNumber -eq $actualLineNumber) {
        return $null
    }
    if ((ConvertTo-ComparableJson -Record $expectedRecord) -ne (ConvertTo-ComparableJson -Record $actualRecord)) {
        return $null
    }

    return [ordered]@{
        ExpectedLineNumber = $expectedLineNumber
        ActualLineNumber = $actualLineNumber
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
$expectedLines = [System.IO.File]::ReadAllLines($resolvedExpectedPath)
$actualLines = [System.IO.File]::ReadAllLines($resolvedActualPath)
$compareStartIndex = 0
$metaMismatch = $null

if ($expectedLines.Length -gt 0 -and $actualLines.Length -gt 0 -and
    (Test-IsMetaLine -Line $expectedLines[0]) -and (Test-IsMetaLine -Line $actualLines[0])) {
    $compareStartIndex = 1
    if ($expectedLines[0] -cne $actualLines[0]) {
        $metaMismatch = [ordered]@{
            Expected = $expectedLines[0]
            Actual = $actualLines[0]
        }
    }
}

$sharedCount = [Math]::Min($expectedLines.Length, $actualLines.Length)
$mismatchIndex = -1
$lineOnlyMismatch = $null

for ($index = $compareStartIndex; $index -lt $sharedCount; $index++) {
    if ($expectedLines[$index] -cne $actualLines[$index]) {
        $lineOnlyDetail = Test-LineOnlyMismatch -ExpectedLine $expectedLines[$index] -ActualLine $actualLines[$index]
        if ($lineOnlyDetail) {
            if (-not $lineOnlyMismatch) {
                $lineOnlyMismatch = [ordered]@{
                    LineNumber = $index + 1
                    ExpectedLineNumber = $lineOnlyDetail.ExpectedLineNumber
                    ActualLineNumber = $lineOnlyDetail.ActualLineNumber
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
Write-Host "Expected lines: $($expectedLines.Length)"
Write-Host "Actual lines: $($actualLines.Length)"

if ($mismatchIndex -lt 0) {
    if ($metaMismatch) {
        Write-Host 'RESULT: FAIL'
        Write-Host 'Meta line differs even though all event lines matched'
        Write-Host "Expected meta: $($metaMismatch.Expected)"
        Write-Host "Actual meta: $($metaMismatch.Actual)"
        exit 1
    }
    if ($lineOnlyMismatch) {
        Write-Host 'RESULT: FAIL'
        Write-Host 'Only source-line differences were found'
        Write-Host "First differing line: $($lineOnlyMismatch.LineNumber)"
        Write-Host "Expected source line: $($lineOnlyMismatch.ExpectedLineNumber)"
        Write-Host "Actual source line: $($lineOnlyMismatch.ActualLineNumber)"
        Write-Host "Expected line: $($lineOnlyMismatch.ExpectedLine)"
        Write-Host "Actual line: $($lineOnlyMismatch.ActualLine)"
        exit 1
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
if ($lineOnlyMismatch) {
    Write-Host "First source-line-only mismatch: line $($lineOnlyMismatch.LineNumber) expected_line=$($lineOnlyMismatch.ExpectedLineNumber) actual_line=$($lineOnlyMismatch.ActualLineNumber)"
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