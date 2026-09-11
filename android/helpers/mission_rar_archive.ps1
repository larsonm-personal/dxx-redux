function Invoke-MissionRarArchive {
    param(
        [Parameter(Mandatory)][string]$ArchivePath,
        [Parameter(Mandatory)][string]$Destination,
        [switch]$List
    )

    # Keep the bounded extractor's strict mode local to this operation
    . (Join-Path $PSScriptRoot 'bounded_extraction.ps1')

    $tar = Get-Command bsdtar, tar -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $tar) { throw 'RAR support requires bsdtar/libarchive on PATH' }
    $ArchivePath = (Resolve-Path -LiteralPath $ArchivePath).Path
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $arguments = if ($List) { @('-tf', $ArchivePath) } else { @('-xf', $ArchivePath, '-C', $Destination) }
    $result = Invoke-BoundedExtractor -OutputDirectory $Destination -FilePath $tar.Source `
        -ArgumentList $arguments -TimeoutSeconds 120 -MaxDiagnosticBytes 1048576
    if ($result.ExitCode -ne 0) {
        throw "RAR operation failed for ${ArchivePath} (requires bsdtar/libarchive RAR support): $($result.Output -join ' ')"
    }
    if ($List) {
        $entries = @($result.Output | Where-Object { $_ })
        if ($entries.Count -gt 4096) { throw 'RAR listing exceeds 4096 entries' }
        return $entries
    }
}
