#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/gradle_test_evidence.ps1"
$fixtureRoot = Join-Path $PSScriptRoot ('../temp/gradle_test_evidence/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
$file = Join-Path $fixtureRoot 'TEST-control.xml'
try {
    $controls = @(
        @{ Name = 'missing'; Xml = $null; Pass = $false },
        @{ Name = 'empty'; Xml = '<testsuite tests="0" failures="0" errors="0" skipped="0"/>'; Pass = $false },
        @{ Name = 'skipped'; Xml = '<testsuite tests="1" failures="0" errors="0" skipped="1"><testcase><skipped/></testcase></testsuite>'; Pass = $false },
        @{ Name = 'failure'; Xml = '<testsuite tests="1" failures="1" errors="0" skipped="0"><testcase><failure/></testcase></testsuite>'; Pass = $false },
        @{ Name = 'hidden-error'; Xml = '<testsuite tests="1" failures="0" errors="0" skipped="0"><testcase><error/></testcase></testsuite>'; Pass = $false },
        @{ Name = 'count-mismatch'; Xml = '<testsuite tests="2" failures="0" errors="0" skipped="0"><testcase/></testsuite>'; Pass = $false },
        @{ Name = 'missing-count'; Xml = '<testsuite tests="1"><testcase/></testsuite>'; Pass = $false },
        @{ Name = 'malformed'; Xml = '<testsuite'; Pass = $false },
        @{ Name = 'pass'; Xml = '<testsuite name="example.RealTest" tests="1" failures="0" errors="0" skipped="0"><testcase name="realAssertion"/></testsuite>'; Pass = $true },
        @{ Name = 'pass-with-skip'; Xml = '<testsuite tests="2" failures="0" errors="0" skipped="1"><testcase/><testcase><skipped/></testcase></testsuite>'; Pass = $true }
    )
    foreach ($control in $controls) {
        if ($null -ne $control.Xml) { [IO.File]::WriteAllText($file, $control.Xml) }
        $passed = $false
        try {
            Assert-GradleUnitTestEvidence -ResultsDirectory $fixtureRoot
            $passed = $true
        } catch {
            if ($control.Pass) { throw }
        }
        if ($passed -ne $control.Pass) { throw "Unexpected evidence outcome: $($control.Name)" }
    }
    # Each module must independently provide evidence, even if another passed
    try {
        Assert-GradleUnitTestEvidence -ResultsDirectory (Join-Path $fixtureRoot 'absent-module')
        throw 'Missing module was accepted'
    } catch {
        if ($_.Exception.Message -eq 'Missing module was accepted') { throw }
    }
} finally {
    Remove-Item -LiteralPath $file -Force -ErrorAction SilentlyContinue
}
Write-Host 'Gradle test evidence controls passed'
