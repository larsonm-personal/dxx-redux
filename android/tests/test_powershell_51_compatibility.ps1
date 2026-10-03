#!/usr/bin/env pwsh
param(
    [switch]$WindowsPowerShellChild,
    [string]$CanonicalSpecOutput
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/powershell_compat.ps1')

function Assert-Equal {
    param([object]$Expected, [object]$Actual, [string]$Case)

    if ($Expected -cne $Actual) {
        throw "$Case expected '$Expected', got '$Actual'"
    }
}

function Test-CompatibilityHelpers {
    $base = [IO.Path]::GetFullPath((Join-Path $repoRoot 'android/tests'))
    $cases = @(
        @{ Target = $base; Expected = '.'; Name = 'same path' },
        @{ Target = Join-Path $base 'fixture/file.json'; Expected = Join-Path 'fixture' 'file.json'; Name = 'child path' },
        @{ Target = Join-Path $base '../helpers/jsonc.ps1'; Expected = Join-Path (Join-Path '..' 'helpers') 'jsonc.ps1'; Name = 'sibling path' }
    )
    foreach ($case in $cases) {
        $actual = Get-CompatibleRelativePath -BasePath $base -TargetPath $case.Target
        Assert-Equal -Expected $case.Expected -Actual $actual -Case $case.Name
    }

    $value = ConvertFrom-CompatibleJsonHashtable -Json '{"name":1,"Name":2,"nested":[{"x":3}],"empty":[],"single":[4]}'
    Assert-Equal -Expected 5 -Actual $value.Count -Case 'case-sensitive top-level keys'
    Assert-Equal -Expected 1 -Actual $value['name'] -Case 'lowercase JSON key'
    Assert-Equal -Expected 2 -Actual $value['Name'] -Case 'uppercase JSON key'
    Assert-Equal -Expected 1 -Actual $value['nested'].Count -Case 'nested array shape'
    Assert-Equal -Expected 3 -Actual $value['nested'][0]['x'] -Case 'nested hashtable value'
    Assert-Equal -Expected 0 -Actual $value['empty'].Count -Case 'empty array shape'
    Assert-Equal -Expected 1 -Actual $value['single'].Count -Case 'single-item array shape'

    $items = @(ConvertFrom-CompatibleJsonItems -Json '[{"id":1},{"id":2}]')
    Assert-Equal -Expected 2 -Actual $items.Count -Case 'root JSON array enumeration'
    Assert-Equal -Expected 2 -Actual $items[1].id -Case 'root JSON array item'

    foreach ($case in @(
            @{ Json = '[]'; Count = 0 }, @{ Json = '[1]'; Count = 1 },
            @{ Json = '[1,2]'; Count = 2 }, @{ Json = '[[1,2],[3]]'; Count = 2 }
        )) {
        $value = $case.Json | ConvertFrom-CompatibleJsonValue
        if ($value -isnot [array]) { throw "Root array shape was lost: $($case.Json)" }
        Assert-Equal -Expected $case.Count -Actual $value.Count -Case 'non-enumerating JSON array count'
        $emitted = @($case.Json | ConvertFrom-CompatibleJsonValue)
        Assert-Equal -Expected 1 -Actual $emitted.Count -Case 'single emitted JSON array value'
    }
    $nested = '[[1,2],[3]]' | ConvertFrom-CompatibleJsonValue
    Assert-Equal -Expected 2 -Actual $nested[0].Count -Case 'nested root array shape'
    Assert-Equal -Expected 3 -Actual $nested[1][0] -Case 'nested root array value'
    $value = '{"levels":[]}' | ConvertFrom-CompatibleJsonValue
    Assert-Equal -Expected 0 -Actual $value.levels.Count -Case 'non-enumerating JSON object value'

    . (Join-Path $repoRoot 'android/helpers/test_execution_evidence.ps1')
    $context = New-TestExecutionEvidenceContext -RepositoryRoot $repoRoot -ReportDir (Join-Path $repoRoot 'temp') -ReportPath (Join-Path $repoRoot 'temp/report.md')
    if (-not $context.HostKey -or $context.Runtime -notmatch 'PowerShell') { throw 'Missing execution evidence host identity' }
    $test = @{ Name = 'fixture'; Type = 'ps1'; Requires = 'none'; Path = Join-Path $repoRoot 'android/tests/fixture.ps1'; Arguments = @('-Fixture') }
    $observation = New-TestExecutionObservation -Context $context -Test $test -Result @{ Status = 'PASS'; ExitCode = 0 } -StartedUtc ([DateTime]::UtcNow.ToString('o')) -SourceSha256 ('0' * 64)
    Assert-Equal -Expected 'android/tests/fixture.ps1' -Actual $observation.source -Case 'execution evidence source path'
    Assert-Equal -Expected 'PASS' -Actual $observation.status -Case 'execution evidence status'
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        $savedNativeArchitecture = $env:PROCESSOR_ARCHITEW6432
        $savedProcessArchitecture = $env:PROCESSOR_ARCHITECTURE
        try {
            $env:PROCESSOR_ARCHITECTURE = 'x86'
            foreach ($native in @('AMD64', 'ARM64')) {
                $env:PROCESSOR_ARCHITEW6432 = $native
                $context = New-TestExecutionEvidenceContext -RepositoryRoot $repoRoot -ReportDir (Join-Path $repoRoot 'temp') -ReportPath (Join-Path $repoRoot 'temp/report.md')
                $expected = if ($native -eq 'AMD64') { 'windows_x64' } else { 'windows_arm64' }
                Assert-Equal -Expected $expected -Actual $context.HostKey -Case 'native OS architecture from 32-bit shell'
            }
        } finally {
            $env:PROCESSOR_ARCHITEW6432 = $savedNativeArchitecture
            $env:PROCESSOR_ARCHITECTURE = $savedProcessArchitecture
        }
    }
}

function Test-WindowsPowerShellParser {
    $relativeFiles = @(& git -C $repoRoot ls-files --cached --others --exclude-standard -- '*.ps1' '*.psm1' '*.psd1' |
            Where-Object { Test-Path -LiteralPath (Join-Path $repoRoot $_) -PathType Leaf })
    $failures = New-Object System.Collections.Generic.List[string]
    foreach ($relativePath in $relativeFiles) {
        $tokens = $null
        $parseErrors = $null
        $path = Join-Path $repoRoot $relativePath
        [void][Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$parseErrors)
        foreach ($parseError in $parseErrors) {
            $failures.Add("${relativePath}:$($parseError.Extent.StartLineNumber): $($parseError.Message)")
        }
    }
    if ($failures.Count -gt 0) {
        throw "PowerShell 5.1 parse failures:`n$($failures -join "`n")"
    }
    Write-Host "PASS: $($relativeFiles.Count) PowerShell files parse under Windows PowerShell $($PSVersionTable.PSVersion)"
}

function Write-TestCanonicalRegressionSpec {
    param([Parameter(Mandatory = $true)][string]$Path)

    . (Join-Path $repoRoot 'android/tests/extract_regression_spec_helpers.ps1')
    $spec = [ordered]@{
        source_type = 'cd'
        expected_files = @('tartarus.msn', 't-zone.msn', 'deepfrst.hog', 'deep-pit.msn')
        total_extracted = 4
    }
    Write-CanonicalRegressionSpec -path $Path -spec $spec -sourceName 'runtime parity' `
        -generated '2000-01-01 00:00:00'
}

if ($WindowsPowerShellChild) {
    Test-CompatibilityHelpers
    Test-WindowsPowerShellParser
    if ($CanonicalSpecOutput) {
        Write-TestCanonicalRegressionSpec -Path $CanonicalSpecOutput
    }
    Write-Host 'PASS: PowerShell 5.1 compatibility helpers'
    exit 0
}

Test-CompatibilityHelpers
Write-Host 'PASS: compatibility helpers on current host runtime'

$windowsPowerShell = Get-Command powershell.exe -ErrorAction SilentlyContinue
if (-not $windowsPowerShell) {
    Write-Host 'RESULT: SKIP (portable compatibility helpers passed; Windows PowerShell 5.1 is unavailable on this host)'
    exit 2
}

$tempRoot = Join-Path $repoRoot "android/temp/powershell-compat-$PID"
$currentOutput = Join-Path $tempRoot 'current.jsonc'
$legacyOutput = Join-Path $tempRoot 'windows-powershell.jsonc'
try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
    Write-TestCanonicalRegressionSpec -Path $currentOutput
    & $windowsPowerShell.Source -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $PSCommandPath `
        -WindowsPowerShellChild -CanonicalSpecOutput $legacyOutput
    if ($LASTEXITCODE -ne 0) {
        throw "Windows PowerShell 5.1 compatibility test failed with exit code $LASTEXITCODE"
    }
    if ([IO.File]::ReadAllText($currentOutput) -cne [IO.File]::ReadAllText($legacyOutput)) {
        throw 'PowerShell 5.1 and PowerShell 7 produced different canonical regression specs'
    }
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
