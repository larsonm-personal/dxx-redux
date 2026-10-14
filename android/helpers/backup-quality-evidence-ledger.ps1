param(
    [string]$DestinationDirectory = "G:\My Drive\descent android port\audit ledger"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Join-Path $repoRoot "android/ai tool plans/code management/general_code_quality_evidence_ledger_20260811.md"
if (-not (Test-Path -LiteralPath $DestinationDirectory -PathType Container)) {
    throw "Drive backup directory is unavailable: $DestinationDirectory"
}

$destination = Join-Path $DestinationDirectory (Split-Path -Leaf $source)
$checksumPath = "$destination.sha256"
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if ((Test-Path -LiteralPath $destination -PathType Leaf) -and
    (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() -eq $sourceHash -and
    (Test-Path -LiteralPath $checksumPath -PathType Leaf) -and
    [System.IO.File]::ReadAllText($checksumPath).Trim() -eq $sourceHash) {
    Write-Host "Drive backup is unchanged"
    return
}

$pending = "$destination.pending"
try {
    Copy-Item -LiteralPath $source -Destination $pending -Force
    $copyHash = (Get-FileHash -LiteralPath $pending -Algorithm SHA256).Hash.ToLowerInvariant()
    $currentHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($copyHash -ne $sourceHash -or $currentHash -ne $sourceHash) {
        throw "Ledger changed during backup; previous Drive backup retained"
    }
    Move-Item -LiteralPath $pending -Destination $destination -Force
    [System.IO.File]::WriteAllText($checksumPath, "$sourceHash`n", [System.Text.UTF8Encoding]::new($false))
    Write-Host "Verified Drive backup: $destination"
} finally {
    if (Test-Path -LiteralPath $pending -PathType Leaf) {
        Remove-Item -LiteralPath $pending
    }
}
