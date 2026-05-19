<#
.SYNOPSIS
    Shared progress and extraction helpers for d2x-xl conversion scripts.
#>

function Format-ElapsedText {
    param([TimeSpan]$Elapsed)

    $hours = [int]$Elapsed.TotalHours
    return "{0:D2}:{1:D2}:{2:D2}" -f $hours, $Elapsed.Minutes, $Elapsed.Seconds
}

function Format-EtaText {
    param([double]$Seconds)

    if ($Seconds -lt 0 -or [double]::IsNaN($Seconds) -or [double]::IsInfinity($Seconds)) {
        return "--:--:--"
    }

    return Format-ElapsedText ([TimeSpan]::FromSeconds([Math]::Max(0, [int][Math]::Round($Seconds))))
}

function Write-ItemStartLine {
    param(
        [System.Diagnostics.Stopwatch]$Stopwatch,
        [int]$Index,
        [int]$Total,
        [string]$ItemName,
        [string]$Indent = ""
    )

    Write-Host ("{0}[{1}] Starting {2} / {3}: {4}" -f $Indent, (Format-ElapsedText $Stopwatch.Elapsed), $Index, $Total, $ItemName)
}

function Write-ProgressSummaryLine {
    param(
        [System.Diagnostics.Stopwatch]$Stopwatch,
        [int]$Processed,
        [int]$Total,
        [string]$ItemName,
        [int]$Succeeded,
        [int]$Errors,
        [string]$Indent = ""
    )

    $elapsed = $Stopwatch.Elapsed
    $avgSeconds = if ($Processed -gt 0) { $elapsed.TotalSeconds / $Processed } else { 0.0 }
    $etaText = if ($Processed -gt 0 -and $Processed -lt $Total) {
        Format-EtaText (($Total - $Processed) * $avgSeconds)
    } else {
        "00:00:00"
    }
    $avgText = if ($Processed -gt 0) { "{0:N1}s" -f $avgSeconds } else { "n/a" }

    Write-Host "${Indent}[$((Format-ElapsedText $elapsed))] Processed $Processed / $Total, ok=$Succeeded, errors=$Errors, last=$ItemName, avg=$avgText, eta=$etaText"
}

function Invoke-7ZipExtract {
    param(
        [string]$SevenZipPath,
        [string]$ArchivePath,
        [string]$ExtractDir,
        [string]$Indent = "  "
    )

    $extractStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "${Indent}Extracting archive: $ArchivePath"
    & $SevenZipPath x "-o$ExtractDir" $ArchivePath -y -bb1 -bsp1
    if ($LASTEXITCODE -ne 0) { throw "7z extraction failed" }
    Write-Host "${Indent}Extraction complete in $(Format-ElapsedText $extractStopwatch.Elapsed)"
}