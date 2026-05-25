#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Validates CD extract regression specs against their source images.
.DESCRIPTION
    Checks that each CD regression spec has source files that exist on disk,
    that CUE specs list every image referenced by their CUE sheets, and that
    Android direct import modes are set for CUE and ISO sources.
#>
param(
    [string]$CdRoot = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'game_data\CD images')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'extract_regression_spec_helpers.ps1')
$failures = New-Object System.Collections.Generic.List[string]

function Add-Failure($specPath, $message) {
    $relative = Resolve-Path -LiteralPath $specPath -Relative
    $script:failures.Add("${relative}: ${message}") | Out-Null
}

function Get-CueReferencedFiles($cuePath) {
    $references = New-Object System.Collections.Generic.List[string]
    try {
        foreach ($line in [System.IO.File]::ReadLines($cuePath)) {
            if ($line -match '^\s*FILE\s+"([^"]+)"') {
                $references.Add($matches[1]) | Out-Null
            } elseif ($line -match '^\s*FILE\s+(\S+)') {
                $references.Add($matches[1]) | Out-Null
            }
        }
    } catch {
        throw "cannot read CUE '$cuePath': $($_.Exception.Message)"
    }
    return $references
}

$specPaths = Get-ChildItem -LiteralPath $CdRoot -Recurse -Filter 'extract_regression.json5' -File | Sort-Object FullName
foreach ($specFile in $specPaths) {
    $spec = Read-Json5File $specFile.FullName
    if ($spec.source_type -ne 'cd') {
        continue
    }

    $specDir = Split-Path $specFile.FullName -Parent
    $sourceFiles = @($spec.source_files)
    $sourceNames = @($sourceFiles | ForEach-Object { $_.name })
    $sourceSet = @{}
    foreach ($name in $sourceNames) {
        if ($name) {
            $sourceSet[$name] = $true
        }
    }

    foreach ($name in $sourceNames) {
        $sourcePath = Join-Path $specDir $name
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            Add-Failure $specFile.FullName "source file '$name' is listed but missing"
        }
    }

    if ($spec.disc_image_type -eq 'iso') {
        if ($spec.import_mode -ne 'setup_iso') {
            Add-Failure $specFile.FullName "ISO spec import_mode should be setup_iso"
        }
        continue
    }

    if ($spec.disc_image_type -ne 'cue_bin') {
        continue
    }

    if ($spec.import_mode -ne 'setup_cd') {
        Add-Failure $specFile.FullName "CUE spec import_mode should be setup_cd"
    }

    $cueNames = @($sourceNames | Where-Object { [System.IO.Path]::GetExtension($_).ToLowerInvariant() -eq '.cue' })
    if ($cueNames.Count -eq 0) {
        Add-Failure $specFile.FullName "CUE spec has no .cue source file"
        continue
    }

    foreach ($cueName in $cueNames) {
        $cuePath = Join-Path $specDir $cueName
        if (-not (Test-Path -LiteralPath $cuePath -PathType Leaf)) {
            continue
        }

        try {
            $referencedFiles = Get-CueReferencedFiles $cuePath
        } catch {
            Add-Failure $specFile.FullName $_
            continue
        }

        foreach ($referenced in $referencedFiles) {
            $referencedName = [System.IO.Path]::GetFileName($referenced)
            $referencedPath = Join-Path $specDir $referenced
            if (-not (Test-Path -LiteralPath $referencedPath -PathType Leaf)) {
                Add-Failure $specFile.FullName "CUE '$cueName' references missing image '$referenced'"
            }
            if (-not $sourceSet.ContainsKey($referenced) -and -not $sourceSet.ContainsKey($referencedName)) {
                Add-Failure $specFile.FullName "CUE '$cueName' references '$referenced' but source_files does not list it"
            }
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host "Regression spec validation failed:" -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "  $failure" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Validated $($specPaths.Count) CD regression specs." -ForegroundColor Green
