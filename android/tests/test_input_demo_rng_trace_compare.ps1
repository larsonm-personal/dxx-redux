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

    @(
        '{"type":"meta","version":1,"events":2,"truncated":false}',
        '{"type":"rand","seq":0,"frame":10,"gt":3276,"call_count":1,"stream":0,"ctx_obj":42,"ctx_sig":9001,"ctx_id":7,"state_before":100,"state_after":200,"result":123,"line":111,"file":"ai.c","func":"do_ai_frame"}',
        "{`"type`":`"rand`",`"seq`":1,`"frame`":10,`"gt`":3276,`"call_count`":2,`"stream`":0,`"ctx_obj`":42,`"ctx_sig`":9001,`"ctx_id`":7,`"state_before`":200,`"state_after`":300,`"result`":$SecondResult,`"line`":112,`"file`":`"ai.c`",`"func`":`"do_ai_frame`"}"
    ) | Set-Content -LiteralPath $Path -Encoding utf8NoBOM
}

function Invoke-RngCompareFixture {
    $compareOutput = & $compareScript -ExpectedPath $expectedPath -ActualPath $actualPath *>&1
    return @{
        ExitCode = $LASTEXITCODE
        Output = $compareOutput -join "`n"
    }
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

    Write-Host 'PASS'
} finally {
    if (Test-Path -LiteralPath $fixtureDir) {
        Remove-Item -LiteralPath $fixtureDir -Recurse -Force
    }
}