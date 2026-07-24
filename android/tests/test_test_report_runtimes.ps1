#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$reader = Join-Path $repoRoot 'android\helpers\get-test-report-runtimes.ps1'
$tempRoot = Join-Path $repoRoot "temp\test_report_runtimes_$([Guid]::NewGuid().ToString('N'))"

try {
    New-Item -Path $tempRoot -ItemType Directory -Force | Out-Null
    $olderReport = Join-Path $tempRoot 'report_20260722_010000.md'
    $newerReport = Join-Path $tempRoot 'report_20260723_010000.md'
    @'
| Status | Time | Test | Type |
|--------|------|------|------|
| PASS | 09:59 | stale_test | ps1 |
'@ | Set-Content -LiteralPath $olderReport -Encoding utf8
    @'
| Status | Time | Test | Type |
|--------|------|------|------|
| PASS | 00:09 | short_test | ps1 |
| FAIL | 01:09 | minute_test | json5 |
| TIMEOUT | 01:02:03 | hour_test | ps1 |
| SKIP | -- | skipped_test | ps1 |
'@ | Set-Content -LiteralPath $newerReport -Encoding utf8
    (Get-Item -LiteralPath $olderReport).LastWriteTimeUtc = (Get-Date).AddMinutes(-2).ToUniversalTime()
    (Get-Item -LiteralPath $newerReport).LastWriteTimeUtc = (Get-Date).AddMinutes(-1).ToUniversalTime()

    $records = @(& $reader -ReportDir $tempRoot)
    $byName = @{}
    foreach ($record in $records) {
        $byName[$record.Name] = $record
    }

    foreach ($expected in @{
            short_test = 9
            minute_test = 69
            hour_test = 3723
        }.GetEnumerator()) {
        if (-not $byName.ContainsKey($expected.Key)) {
            throw "Missing runtime for $($expected.Key)"
        }
        if ($byName[$expected.Key].Seconds -ne $expected.Value) {
            throw "Runtime for $($expected.Key) was $($byName[$expected.Key].Seconds), expected $($expected.Value)"
        }
    }
    if ($byName.ContainsKey('stale_test') -or $byName.ContainsKey('skipped_test')) {
        throw 'Reader included a stale or untimed result'
    }
    if ($records[0].ReportPath -ne $newerReport) {
        throw "Reader selected $($records[0].ReportPath), expected $newerReport"
    }
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Host 'PASS'
