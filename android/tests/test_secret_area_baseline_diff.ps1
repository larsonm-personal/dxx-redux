#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'secret_area_baseline_helpers.ps1')

$expected = '{"same":1,"changed":1,"removed":true,"array":[1],"empty":{}}' | ConvertFrom-Json
$actual = '{"same":1,"changed":2,"added":"yes","array":[1,2],"empty":{}}' | ConvertFrom-Json
$diff = Compare-JsonStructure -Expected $expected -Actual $actual

if ($diff.Added -ne 2 -or $diff.Removed -ne 1 -or $diff.Changed -ne 1 -or $diff.Total -ne 4) {
    throw "Unexpected structural diff counts: $($diff | ConvertTo-Json -Compress)"
}
$formatted = Format-JsonStructureDiff -Diff $diff
foreach ($expectedText in @('$/added', '$/array/1', '$/removed', '$/changed')) {
    if ($formatted -notmatch [regex]::Escape($expectedText)) {
        throw "Structural diff omitted $expectedText"
    }
}
$balanced = Format-JsonStructureDiff -Diff $diff -MaxDetails 3
foreach ($prefix in @('+ ', '- ', '~ ')) {
    if ($balanced -notmatch "(?m)^$([regex]::Escape($prefix))") {
        throw "Balanced structural diff omitted category '$prefix'"
    }
}
$single = Format-JsonStructureDiff -Diff $diff -MaxDetails 1
if (@($single -split "`n").Count -ne 3 -or $single -notmatch 'Showing 1 of 4 differences') {
    throw "MaxDetails did not limit formatted output: $single"
}
$matched = Compare-JsonStructure -Expected $expected -Actual $expected
if ($matched.Total -ne 0) {
    throw 'Identical JSON produced structural differences'
}

Write-Host 'PASS'
