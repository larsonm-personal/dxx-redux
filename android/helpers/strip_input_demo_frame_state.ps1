#!/usr/bin/env pwsh
[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Path
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'powershell_compat.ps1')

if (-not $Path) {
    $Path = Join-Path (Split-Path $PSScriptRoot) 'temp_game_logs'
}

function Resolve-AbsolutePath {
    param([string]$Value)

    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Value))
}

function Get-RelativeDisplayPath {
    param([string]$Value)

    $repoRoot = Split-Path (Split-Path $PSScriptRoot)
    $absoluteValue = [System.IO.Path]::GetFullPath($Value)
    $absoluteRepoRoot = [System.IO.Path]::GetFullPath($repoRoot)

    if ($absoluteValue.StartsWith($absoluteRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return Get-CompatibleRelativePath -BasePath $absoluteRepoRoot -TargetPath $absoluteValue
    }

    return $absoluteValue
}

function Read-TextFileInfo {
    param([string]$FilePath)

    $text = [System.IO.File]::ReadAllText($FilePath)
    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $hasTrailingNewline = $text.EndsWith("`n")

    if ($text.Length -eq 0) {
        $lines = @()
    } else {
        $lines = [regex]::Split($text, "`r?`n")
        if ($hasTrailingNewline -and $lines.Count -gt 0 -and $lines[-1] -eq '') {
            $lines = $lines[0..($lines.Count - 2)]
        }
    }

    return [pscustomobject]@{
        Text = $text
        Lines = $lines
        Newline = $newline
        HasTrailingNewline = $hasTrailingNewline
    }
}

function ConvertFrom-JsonRecordLine {
    param(
        [string]$Line,
        [string]$FilePath,
        [int]$LineNumber
    )

    try {
        return ($Line | ConvertFrom-Json)
    } catch {
        throw "Could not parse JSON in $FilePath line ${LineNumber}: $($_.Exception.Message)"
    }
}

function ConvertTo-JsonRecordLineWithoutState {
    param([object]$Record)

    $ordered = [ordered]@{}

    foreach ($property in $Record.PSObject.Properties) {
        if ($property.Name -eq 'state') {
            continue
        }

        $ordered[$property.Name] = $property.Value
    }

    return ($ordered | ConvertTo-Json -Compress -Depth 100)
}

function Strip-FrameStateFromDemo {
    param([string]$FilePath)

    $fileInfo = Read-TextFileInfo -FilePath $FilePath
    $outputLines = New-Object System.Collections.Generic.List[string]
    $removedCount = 0

    for ($lineIndex = 0; $lineIndex -lt $fileInfo.Lines.Count; $lineIndex++) {
        $line = $fileInfo.Lines[$lineIndex]
        $trimmed = $line.Trim()

        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('//')) {
            $outputLines.Add($line)
            continue
        }

        $record = ConvertFrom-JsonRecordLine -Line $line -FilePath $FilePath -LineNumber ($lineIndex + 1)
        $typeProperty = $record.PSObject.Properties['type']
        $stateProperty = $record.PSObject.Properties['state']

        if ($null -eq $typeProperty) {
            throw "Missing record type in $FilePath line $($lineIndex + 1)"
        }

        if ($typeProperty.Value -eq 'frame' -and $null -ne $stateProperty) {
            $outputLines.Add((ConvertTo-JsonRecordLineWithoutState -Record $record))
            $removedCount++
            continue
        }

        $outputLines.Add($line)
    }

    if ($removedCount -eq 0) {
        return [pscustomobject]@{
            Changed = $false
            FrameStatesRemoved = 0
            Content = $null
        }
    }

    $content = [string]::Join($fileInfo.Newline, $outputLines)
    if ($fileInfo.HasTrailingNewline) {
        $content += $fileInfo.Newline
    }

    return [pscustomobject]@{
        Changed = $content -ne $fileInfo.Text
        FrameStatesRemoved = $removedCount
        Content = $content
    }
}

$targetPath = Resolve-AbsolutePath -Value $Path

if (-not (Test-Path -LiteralPath $targetPath -PathType Container)) {
    throw "Directory not found: $targetPath"
}

$demoFiles = @(Get-ChildItem -LiteralPath $targetPath -File -Filter '*.dximdemo' -ErrorAction Stop | Sort-Object -Property Name)

if ($demoFiles.Count -eq 0) {
    Write-Host ("No .dximdemo files found in {0}" -f (Get-RelativeDisplayPath -Value $targetPath))
    return
}

$changedFiles = 0
$removedStates = 0

foreach ($demoFile in $demoFiles) {
    $result = Strip-FrameStateFromDemo -FilePath $demoFile.FullName

    if (-not $result.Changed) {
        continue
    }

    $changedFiles++
    $removedStates += $result.FrameStatesRemoved

    if ($PSCmdlet.ShouldProcess($demoFile.FullName, 'strip per-frame state')) {
        [System.IO.File]::WriteAllText(
            $demoFile.FullName,
            $result.Content,
            [System.Text.UTF8Encoding]::new($false))
        Write-Host (
            'Stripped {0} frame states from {1}' -f
            $result.FrameStatesRemoved,
            (Get-RelativeDisplayPath -Value $demoFile.FullName))
    }
}

Write-Host ('Scanned {0} .dximdemo files in {1}' -f $demoFiles.Count, (Get-RelativeDisplayPath -Value $targetPath))

if ($changedFiles -eq 0) {
    Write-Host 'No per-frame state fields needed stripping'
    return
}

Write-Host ('Updated {0} files and removed {1} frame state records' -f $changedFiles, $removedStates)
