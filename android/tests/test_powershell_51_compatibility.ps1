#!/usr/bin/env pwsh
param([switch]$WindowsPowerShellChild)

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
}

function Test-WindowsPowerShellParser {
    $relativeFiles = @(& git -C $repoRoot ls-files --cached --others --exclude-standard -- '*.ps1' '*.psm1' '*.psd1')
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

if ($WindowsPowerShellChild) {
    Test-CompatibilityHelpers
    Test-WindowsPowerShellParser
    Write-Host 'PASS: PowerShell 5.1 compatibility helpers'
    exit 0
}

$windowsPowerShell = Get-Command powershell.exe -ErrorAction SilentlyContinue
if (-not $windowsPowerShell) {
    Write-Host 'SKIP: Windows PowerShell 5.1 is unavailable on this host'
    exit 0
}

& $windowsPowerShell.Source -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $PSCommandPath -WindowsPowerShellChild
if ($LASTEXITCODE -ne 0) {
    throw "Windows PowerShell 5.1 compatibility test failed with exit code $LASTEXITCODE"
}
