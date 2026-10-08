#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$compareScript = Join-Path $PSScriptRoot 'compare_input_demo_rng_trace.ps1'
$fixtureDir = Join-Path $repoRoot 'temp\input_demo_rng_trace_compare_fixture'
$expectedPath = Join-Path $fixtureDir 'expected.rngtrace.jsonl'
$actualPath = Join-Path $fixtureDir 'actual.rngtrace.jsonl'

function Write-RngTrace {
    param(
        [string]$Path,
        [int]$SecondResult
    )

    $lines = @(
        '{"type":"meta","version":1,"events":2,"truncated":false}',
        '{"type":"rand","seq":0,"frame":10,"gt":3276,"call_count":1,"stream":0,"ctx_obj":42,"ctx_sig":9001,"ctx_id":7,"state_before":100,"state_after":200,"result":123,"line":111,"file":"ai.c","func":"do_ai_frame"}',
        "{`"type`":`"rand`",`"seq`":1,`"frame`":10,`"gt`":3276,`"call_count`":2,`"stream`":0,`"ctx_obj`":42,`"ctx_sig`":9001,`"ctx_id`":7,`"state_before`":200,`"state_after`":300,`"result`":$SecondResult,`"line`":112,`"file`":`"ai.c`",`"func`":`"do_ai_frame`"}"
    )
    [IO.File]::WriteAllText($Path, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
}

function Invoke-RngCompareFixture {
    $compareOutput = & (Get-Process -Id $PID).Path -NoProfile -File $compareScript -ExpectedPath $expectedPath -ActualPath $actualPath *>&1
    return @{
        ExitCode = $LASTEXITCODE
        Output = $compareOutput -join "`n"
    }
}

function Assert-RngComparison {
    param(
        [string]$Name,
        [AllowEmptyCollection()][string[]]$ExpectedLines,
        [AllowEmptyCollection()][string[]]$ActualLines,
        [int]$ExitCode,
        [string]$Diagnostic
    )
    [IO.File]::WriteAllLines($expectedPath, $ExpectedLines, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllLines($actualPath, $ActualLines, [Text.UTF8Encoding]::new($false))
    $observed = Invoke-RngCompareFixture
    if ($observed.ExitCode -ne $ExitCode -or
        ($Diagnostic -and $observed.Output -notmatch $Diagnostic)) {
        throw "$Name expected exit $ExitCode, observed $($observed.ExitCode)`n$($observed.Output)"
    }
    Write-Host "PASS $Name"
}

try {
    if (-not (Test-Path -LiteralPath $fixtureDir)) {
        New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
    }

    Write-RngTrace -Path $expectedPath -SecondResult 456
    Write-RngTrace -Path $actualPath -SecondResult 456
    $matching = Invoke-RngCompareFixture
    if ($matching.ExitCode -ne 0) {
        throw "matching RNG trace compare failed`n$($matching.Output)"
    }

    Write-RngTrace -Path $actualPath -SecondResult 789
    $mismatch = Invoke-RngCompareFixture
    if ($mismatch.ExitCode -ne 1) {
        throw "mismatched RNG trace compare returned $($mismatch.ExitCode)`n$($mismatch.Output)"
    }
    if ($mismatch.Output -notmatch 'First differing line: 2' -or
        $mismatch.Output -notmatch 'ctx_obj=42' -or
        $mismatch.Output -notmatch 'state_before=200' -or
        $mismatch.Output -notmatch 'result=456' -or
        $mismatch.Output -notmatch 'result=789') {
        throw "mismatched RNG trace compare missed origin diagnostics`n$($mismatch.Output)"
    }

    Write-RngTrace -Path $expectedPath -SecondResult 456
    $valid = [IO.File]::ReadAllLines($expectedPath)
    Assert-RngComparison 'empty inputs' @() @() 1 'Invalid RNG trace'
    $zero = @('{"type":"meta","version":1,"events":0,"truncated":false}')
    Assert-RngComparison 'complete zero-event traces' $zero $zero 0 'RESULT: PASS'

    $invalid = [ordered]@{
        'missing header' = @($valid[1], $valid[2])
        'malformed JSON' = @('broken')
        'blank first line' = @('') + $valid
        'duplicate header' = @($valid[0]) + $valid
        'midstream header' = @($valid[0], $valid[1], $valid[0], $valid[2])
        'missing event' = @($valid[0], $valid[1])
        'extra event' = $valid + @($valid[1])
        'duplicate member' = @($valid[0].Replace('"version":1', '"version":1,"version":1')) + $valid[1..2]
        'case alias member' = @($valid[0].Replace('"version":1', '"version":1,"Version":1')) + $valid[1..2]
        'comment JSON' = @($valid[0] + ' // comment') + $valid[1..2]
        'trailing comma' = @($valid[0].Replace('false}', 'false,}')) + $valid[1..2]
        'array record' = @($valid[0], '[]', $valid[2])
    }
    foreach ($replacement in @(
            @('fractional version', '"version":1', '"version":1.0'),
            @('unsupported version', '"version":1', '"version":2'),
            @('string count', '"events":2', '"events":"2"'),
            @('negative count', '"events":2', '"events":-1'),
            @('fractional count', '"events":2', '"events":2.0'),
            @('truncated evidence', '"truncated":false', '"truncated":true'),
            @('string truncated', '"truncated":false', '"truncated":"false"'),
            @('missing count', '"events":2,', ''),
            @('unknown meta member', '"version":1', '"version":1,"future":1'))) {
        $invalid[$replacement[0]] = @($valid[0].Replace($replacement[1], $replacement[2])) + $valid[1..2]
    }
    foreach ($replacement in @(
            @('unknown event kind', '"type":"rand"', '"type":"other"'),
            @('missing frame', '"frame":10,', ''),
            @('negative sequence', '"seq":0', '"seq":-1'),
            @('overflow frame', '"frame":10', '"frame":4294967296'),
            @('overflow game time', '"gt":3276', '"gt":9223372036854775808'),
            @('negative call count', '"call_count":1', '"call_count":-1'),
            @('fractional result', '"result":123', '"result":123.5'),
            @('overflow result', '"result":123', '"result":2147483648'),
            @('string source line', '"line":111', '"line":"111"'),
            @('numeric file', '"file":"ai.c"', '"file":1'),
            @('null function', '"func":"do_ai_frame"', '"func":null'),
            @('partial object context', '"ctx_id":7,', ''),
            @('overflow object signature', '"ctx_sig":9001', '"ctx_sig":2147483648'),
            @('negative state', '"state_before":100', '"state_before":-1'),
            @('stream overflow', '"stream":0', '"stream":256'),
            @('string context flag', '"stream":0', '"stream":0,"has_context":"false"'),
            @('seed in rand', '"result":123', '"result":123,"seed":1'),
            @('srand without seed', '"type":"rand"', '"type":"srand"'),
            @('invalid supplemental stream', '"stream":0', '"stream":1,"future":1'),
            @('invalid supplemental context', '"file":"ai.c"', '"file":1,"has_context":false'))) {
        $invalid[$replacement[0]] = @($valid[0], $valid[1].Replace($replacement[1], $replacement[2]), $valid[2])
    }
    foreach ($case in $invalid.GetEnumerator()) {
        Assert-RngComparison "identical invalid $($case.Key)" $case.Value $case.Value 1 'Invalid RNG trace'
        Assert-RngComparison "invalid actual $($case.Key)" $valid $case.Value 1 'Invalid RNG trace'
        Assert-RngComparison "invalid expected $($case.Key)" $case.Value $valid 1 'Invalid RNG trace'
    }

    $shifted = @($valid[0], $valid[1].Replace('"seq":0', '"seq":10').Replace('"line":111', '"line":211'), $valid[2])
    Assert-RngComparison 'line and sequence diagnostics' $valid $shifted 0 'ignored source-line-only differences'
    $caseChanged = @($valid[0], $shifted[1].Replace('ai.c', 'AI.c'), $valid[2])
    Assert-RngComparison 'case-sensitive semantic mismatch' $valid $caseChanged 1 'First differing line: 1'
    foreach ($supplemental in @(
            $valid[1].Replace('"stream":0', '"stream":1'),
            $valid[1].Replace('"stream":0', '"has_context":false'))) {
        $withSupplemental = @($valid[0].Replace('"events":2', '"events":3'), $supplemental, $valid[1], $valid[2])
        Assert-RngComparison 'validated supplemental exclusion' $valid $withSupplemental 0 'skipped=1'
    }
    $seeded = @($valid[0], $valid[1].Replace('"type":"rand"', '"type":"srand"').Replace('"result":123', '"seed":4294967295'), $valid[2])
    Assert-RngComparison 'valid srand' $seeded $seeded 0 'RESULT: PASS'
    $reordered = @('{"truncated":false,"events":2,"version":1,"type":"meta"}', $valid[1].Replace('{"type":"rand",', '{').Replace('"func":"do_ai_frame"}', '"func":"do_ai_frame","type":"rand"}'), $valid[2])
    Assert-RngComparison 'semantic member order' $valid $reordered 0 'RESULT: PASS'
    $reorderedThenShifted = @($reordered[0], $reordered[1], $valid[2].Replace('"line":112', '"line":212'))
    Assert-RngComparison 'formatting preserves later diagnostic note' $valid $reorderedThenShifted 0 'ignored source-line-only differences'
    $onlySupplemental = @($valid[0].Replace('"events":2', '"events":1'), $valid[1].Replace('"stream":0', '"stream":1'))
    Assert-RngComparison 'complete supplemental-only trace' $zero $onlySupplemental 0 'skipped=1'
    $bounds = @($valid[0], $valid[1].Replace('"seq":0', '"seq":18446744073709551615').Replace('"frame":10', '"frame":4294967295').Replace('"gt":3276', '"gt":-9223372036854775808').Replace('"ctx_obj":42', '"ctx_obj":-2147483648').Replace('"state_before":100', '"state_before":4294967295'), $valid[2])
    Assert-RngComparison 'supported native integer bounds' $bounds $bounds 0 'RESULT: PASS'

    Write-Host 'PASS'
} finally {
    if (Test-Path -LiteralPath $fixtureDir) {
        Remove-Item -LiteralPath $fixtureDir -Recurse -Force
    }
}
