param(
    [Parameter(Mandatory = $true)]
    [string[]]$ReportPath,

    [string]$LedgerPath = "android/ai tool plans/code management/general_code_quality_evidence_ledger_20260811.md",

    [switch]$RemoveImportedSource,

    [switch]$RewriteCanonicalReferences
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $PSScriptRoot "powershell_compat.ps1")
$ledgerFullPath = Join-Path $repoRoot $LedgerPath
$ledgerDirectory = Split-Path -Parent $ledgerFullPath
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

if (-not (Test-Path -LiteralPath $ledgerDirectory -PathType Container)) {
    throw "Evidence ledger directory does not exist: $ledgerDirectory"
}

if (-not (Test-Path -LiteralPath $ledgerFullPath -PathType Leaf)) {
    throw "Evidence ledger does not exist: $ledgerFullPath"
}

$ledgerText = [System.IO.File]::ReadAllText($ledgerFullPath)
$resolvedReports = foreach ($path in $ReportPath) {
    $candidate = if ([System.IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $repoRoot $path }
    $resolved = (Resolve-Path -LiteralPath $candidate).Path
    if (-not $resolved.StartsWith($repoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Report is outside the repository: $resolved"
    }
    $resolved
}

foreach ($report in $resolvedReports) {
    $bytes = [System.IO.File]::ReadAllBytes($report)
    if ($bytes.Length -eq 0) {
        throw "Report is empty: $report"
    }
    if ($bytes | Where-Object { $_ -gt 127 } | Select-Object -First 1) {
        throw "Report is not printable ASCII: $report"
    }
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xef -and $bytes[1] -eq 0xbb -and $bytes[2] -eq 0xbf) {
        throw "Report has a UTF-8 BOM: $report"
    }

    $reportText = [System.Text.Encoding]::ASCII.GetString($bytes)
    if ($reportText.Contains([char]0)) {
        throw "Report contains a NUL byte: $report"
    }
    if ($reportText -match "(?m)[ `t]+`r?$") {
        throw "Report has trailing whitespace: $report"
    }

    $firstHeading = ($reportText -split "`r?`n" | Where-Object { $_ -match '^# ' } | Select-Object -First 1)
    if (-not $firstHeading) {
        throw "Report has no level-one heading: $report"
    }
    $reportId = $firstHeading.Substring(2).Trim()
    $sha256 = (Get-FileHash -LiteralPath $report -Algorithm SHA256).Hash.ToLowerInvariant()
    $beginMarker = "<!-- BEGIN IMPORT: $reportId SHA256:$sha256 -->"
    $idPrefix = "<!-- BEGIN IMPORT: $reportId SHA256:"
    if ($ledgerText.Contains($idPrefix)) {
        Write-Host "Already imported: $reportId"
        continue
    }

    $relativeReport = (Get-CompatibleRelativePath -BasePath $repoRoot -TargetPath $report).Replace('\', '/')
    $normalizedReport = $reportText.TrimEnd("`r", "`n")
    $block = @"

$beginMarker

## $reportId imported evidence

- Original workspace path: ``$relativeReport``
- Imported SHA-256: ``$sha256``

<details>
<summary>Full worker report</summary>

$normalizedReport

</details>

<!-- END IMPORT: $reportId -->
"@

    [System.IO.File]::AppendAllText($ledgerFullPath, $block, $utf8NoBom)
    $ledgerText += $block
    Write-Host "Imported: $reportId"

    if ($RemoveImportedSource) {
        $inboxDirectory = Join-Path $repoRoot "android/ai tool plans/code management/general_code_quality_inbox"
        if (-not $report.StartsWith($inboxDirectory, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a source outside the evidence inbox: $report"
        }
        Remove-Item -LiteralPath $report
    }
}

if ($RewriteCanonicalReferences) {
    $canonicalPath = Join-Path $repoRoot "android/ai tool plans/code management/general_code_quality_ledger_20260811.md"
    $canonicalText = [System.IO.File]::ReadAllText($canonicalPath)
    $reportPattern = '`temp/general_code_quality_20260811/(?:gq1_(?:chunk|preflight)_\d{4}|history_audit|process_audit|live_survey)\.md`'
    $replacement = '`general_code_quality_evidence_ledger_20260811.md`'
    $rewrittenText = [regex]::Replace($canonicalText, $reportPattern, $replacement)
    if ($rewrittenText -ne $canonicalText) {
        [System.IO.File]::WriteAllText($canonicalPath, $rewrittenText, $utf8NoBom)
        Write-Host "Rewrote canonical evidence references"
    }
}
