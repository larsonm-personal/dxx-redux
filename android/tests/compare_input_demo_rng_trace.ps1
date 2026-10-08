#!/usr/bin/env pwsh
#requires -Version 7.0
param(
    [Parameter(Mandatory = $true)]
    [string]$ExpectedPath,
    [Parameter(Mandatory = $true)]
    [string]$ActualPath,
    [int]$ContextLines = 3
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'helpers\powershell_compat.ps1')

function Get-RelativeRepoPath {
    param([string]$Path)

    try {
        return Get-CompatibleRelativePath -BasePath $repoRoot -TargetPath $Path
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
        $record = ConvertFrom-CompatibleJsonHashtable -Json $Line
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

function Assert-TraceInteger {
    param([hashtable]$Record, [string]$Key, [decimal]$Minimum, [decimal]$Maximum)

    $value = $Record[$Key]
    if (($value -isnot [long] -and $value -isnot [ulong]) -or
        [decimal]$value -lt $Minimum -or [decimal]$value -gt $Maximum) {
        throw "Invalid integer field '$Key'"
    }
}

function Convert-JsonLineToRecord {
    param([string]$Line)

    # Strict native JSON, including member identity, before any comparison or filtering
    $document = [System.Text.Json.JsonDocument]::Parse($Line)
    try {
        if ($document.RootElement.ValueKind -ne [System.Text.Json.JsonValueKind]::Object) {
            throw 'Record must be a JSON object'
        }
        $record = @{}
        foreach ($property in $document.RootElement.EnumerateObject()) {
            if ($record.ContainsKey($property.Name)) { throw "Duplicate field '$($property.Name)'" }
            $value = $property.Value
            switch ($value.ValueKind.ToString()) {
                String { $record[$property.Name] = $value.GetString() }
                True { $record[$property.Name] = $true }
                False { $record[$property.Name] = $false }
                Number {
                    $signed = [long]0
                    $unsigned = [ulong]0
                    if ($value.TryGetInt64([ref]$signed)) { $record[$property.Name] = $signed }
                    elseif ($value.TryGetUInt64([ref]$unsigned)) { $record[$property.Name] = $unsigned }
                    else { throw "Invalid integer field '$($property.Name)'" }
                }
                default { throw "Invalid scalar field '$($property.Name)'" }
            }
        }
        if ($record.type -isnot [string] -or $record.type -cnotin @('meta', 'rand', 'srand')) {
            throw 'Unsupported record type'
        }
        if ($record.type -ceq 'meta') {
            $allowed = @('type', 'version', 'events', 'truncated')
            Assert-TraceInteger $record 'version' 1 1
            Assert-TraceInteger $record 'events' 0 '18446744073709551615'
            if ($record.truncated -isnot [bool] -or $record.truncated) { throw 'Trace is truncated or lacks a boolean completeness flag' }
        } else {
            $allowed = @('type', 'seq', 'frame', 'gt', 'call_count', 'stream', 'has_context',
                'ctx_obj', 'ctx_sig', 'ctx_id', 'state_before', 'state_after', 'line', 'file', 'func')
            Assert-TraceInteger $record 'seq' 0 '18446744073709551615'
            Assert-TraceInteger $record 'frame' 0 4294967295
            Assert-TraceInteger $record 'gt' '-9223372036854775808' '9223372036854775807'
            Assert-TraceInteger $record 'call_count' 0 4294967295
            Assert-TraceInteger $record 'line' -2147483648 2147483647
            foreach ($key in @('file', 'func')) {
                if ($record[$key] -isnot [string]) { throw "Invalid string field '$key'" }
            }
            foreach ($key in @('state_before', 'state_after')) {
                if ($record.ContainsKey($key)) { Assert-TraceInteger $record $key 0 4294967295 }
            }
            if ($record.ContainsKey('stream')) { Assert-TraceInteger $record 'stream' 0 255 }
            if ($record.ContainsKey('has_context') -and $record.has_context -isnot [bool]) {
                throw 'Invalid boolean context flag'
            }
            $objectFields = @('ctx_obj', 'ctx_sig', 'ctx_id')
            $present = @($objectFields | Where-Object { $record.ContainsKey($_) })
            if ($present.Count -ne 0 -and $present.Count -ne 3) { throw 'Incomplete object context' }
            foreach ($key in $present) { Assert-TraceInteger $record $key -2147483648 2147483647 }
            if ($record.type -ceq 'rand') {
                $allowed += 'result'
                Assert-TraceInteger $record 'result' -2147483648 2147483647
            } else {
                $allowed += 'seed'
                Assert-TraceInteger $record 'seed' 0 4294967295
            }
        }
        foreach ($key in $record.Keys) {
            if ($allowed -cnotcontains $key) { throw "Unsupported field '$key'" }
        }
        return $record
    } finally { $document.Dispose() }
}

function Test-IsSupplementalEvent {
    param([hashtable]$Record)

    return ($Record.ContainsKey('stream') -and $Record.stream -ne 0) -or
    ($Record.ContainsKey('has_context') -and -not $Record.has_context)
}

function Get-ComparableTrace {
    param([AllowEmptyCollection()][string[]]$Lines, [string]$Label)

    if ($Lines.Count -eq 0) { throw "Invalid RNG trace ${Label}: missing meta header" }
    $eventLines = New-Object System.Collections.Generic.List[string]
    $metaLine = $null
    $skippedCount = 0
    for ($index = 0; $index -lt $Lines.Length; $index++) {
        $line = $Lines[$index]
        try {
            $record = Convert-JsonLineToRecord -Line $line
            if ($index -eq 0) {
                if ($record.type -cne 'meta') { throw 'First record must be the meta header' }
                $metaLine = $line
                $eventCount = $record.events
                continue
            }
            if ($record.type -ceq 'meta') { throw 'Duplicate or midstream meta header' }
            if (Test-IsSupplementalEvent -Record $record) { $skippedCount++ }
            else { $eventLines.Add($line) }
        } catch {
            throw "Invalid RNG trace $Label at line $($index + 1): $_"
        }
    }
    # Count every raw event before documented stream/context exclusions, including zero-event traces
    if ($eventCount -ne ($Lines.Length - 1)) {
        throw "Invalid RNG trace ${Label}: declared $eventCount events, found $($Lines.Length - 1)"
    }
    return [ordered]@{
        MetaLine = $metaLine
        EventLines = $eventLines.ToArray()
        SkippedCount = $skippedCount
    }
}

function ConvertTo-ComparableJson {
    param([hashtable]$Record)

    $ordered = [ordered]@{}
    [string[]]$keys = @($Record.Keys)
    [Array]::Sort($keys, [StringComparer]::Ordinal)
    foreach ($key in $keys) { $ordered[$key] = $Record[$key] }
    return ConvertTo-Json -InputObject $ordered -Compress -Depth 10
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
    if ((ConvertTo-ComparableJson -Record $expectedRecord) -ceq (ConvertTo-ComparableJson -Record $actualRecord)) {
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

    if ((ConvertTo-ComparableJson -Record $expectedRecord) -cne (ConvertTo-ComparableJson -Record $actualRecord)) {
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
try {
    $expectedTrace = Get-ComparableTrace -Lines $expectedRawLines -Label $resolvedExpectedPath
    $actualTrace = Get-ComparableTrace -Lines $actualRawLines -Label $resolvedActualPath
} catch {
    Write-Host 'RESULT: FAIL'
    Write-Host $_
    exit 1
}
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
            if (-not $ignorableMismatch -and ($ignorableDetail.LineChanged -or $ignorableDetail.SeqChanged)) {
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
