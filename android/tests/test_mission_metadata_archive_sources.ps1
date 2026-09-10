#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot "android\helpers\mission_archive_sources.ps1")

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "dxx-mission-sources-$([guid]::NewGuid().ToString('N'))"
try {
    $primary = New-Item -ItemType Directory -Force -Path (Join-Path $testRoot "game_data\mission_files")
    $secondary = New-Item -ItemType Directory -Force -Path (Join-Path $primary.FullName "d2xxl_downloads")
    New-Item -ItemType File -Path (Join-Path $primary.FullName "primary.zip") | Out-Null
    New-Item -ItemType File -Path (Join-Path $primary.FullName "complete.7z") | Out-Null
    New-Item -ItemType File -Path (Join-Path $primary.FullName "complete.json") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "secondary.7z") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "complete.zip") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "complete.json") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "secondary.rar") | Out-Null

    $sources = @(Get-MissionArchiveSources -RepoRoot $testRoot)
    Assert-True ($sources.Count -eq 2) "Expected two configured mission archive sources"
    Assert-True ($sources[0].Directory -eq $primary.FullName) "Primary source path is incorrect"
    Assert-True ($sources[1].Directory -eq $secondary.FullName) "D2X-XL source path is incorrect"
    Assert-True ($sources[1].FingerprintAlbumPrefix -eq 'Mission ZIP - D2X-XL - ') `
        "D2X-XL fingerprint output should have a collision-safe prefix"

    $available = @(Get-AvailableMissionArchiveSources -Sources $sources)
    Assert-True ($available.Count -eq 2) "Expected both populated mission archive sources"

    # Execute the host runner's real parameter binding and archive selection without building or regenerating data
    $hostScript = Join-Path $repoRoot "android\helpers\regenerate_all_mission_metadata_host.ps1"
    $hostAst = [Management.Automation.Language.Parser]::ParseFile($hostScript, [ref]$null, [ref]$null)
    $filterAssignment = $hostAst.Find({
            param($node)
            $node -is [Management.Automation.Language.AssignmentStatementAst] -and
            $node.Left.Extent.Text -eq '$hasArchiveFilter'
        }, $true)
    $archiveAssignment = $hostAst.Find({
            param($node)
            $node -is [Management.Automation.Language.AssignmentStatementAst] -and
            $node.Left.Extent.Text -eq '$archives'
        }, $true)
    $emptyCheck = $hostAst.Find({
            param($node)
            $node -is [Management.Automation.Language.IfStatementAst] -and
            $node.Clauses[0].Item1.Extent.Text -eq '$archives.Count -eq 0'
        }, $true)
    $selectionText = $hostAst.Extent.Text.Substring($archiveAssignment.Extent.StartOffset,
        $emptyCheck.Extent.StartOffset - $archiveAssignment.Extent.StartOffset)
    $selectArchives = [scriptblock]::Create($hostAst.ParamBlock.Extent.Text + "`n" +
        $filterAssignment.Extent.Text + "`n" + $selectionText + "`n" +
        '[pscustomobject]@{ Filtered = $hasArchiveFilter; Archives = @($archives) }')
    $archiveSources = $available
    foreach ($options in @(@{}, @{ ArchiveNames = $null; ArchivePaths = $null },
            @{ ArchiveNames = @(); ArchivePaths = @() })) {
        $selection = & $selectArchives @options
        Assert-True (-not $selection.Filtered) "Absent or empty filters must retain built-in and CD missions"
        Assert-True ($selection.Archives.Count -eq 5) "Absent or empty filters must retain all archives"
    }
    $selection = & $selectArchives -ArchiveName 'primary.zip'
    Assert-True ($selection.Filtered -and $selection.Archives.Count -eq 1) "Single-name selection must still filter"
    $selection = & $selectArchives -ArchiveNames @('primary.zip', 'secondary.7z')
    Assert-True ($selection.Filtered -and $selection.Archives.Count -eq 2) "Multiple-name selection must still filter"
    $selection = & $selectArchives -ArchiveName 'secondary.rar'
    Assert-True ($selection.Archives.Count -eq 1) 'RAR name selection must retain the archive'
    $selectedPath = Join-Path $secondary.FullName "complete.zip"
    $selection = & $selectArchives -ArchivePaths @($selectedPath)
    Assert-True ($selection.Filtered -and $selection.Archives.Count -eq 1 -and
        $selection.Archives[0].Archive.FullName -eq $selectedPath) "Worker path selection must retain only the requested archive"

    $primaryMissing = @(Get-MissingMissionMetadataArchives -Source $sources[0])
    Assert-True (($primaryMissing.Name -join ',') -eq 'primary.zip') `
        "Primary missing selection should exclude archives with matching JSON"
    $secondaryMissing = @(Get-MissingMissionMetadataArchives -Source $sources[1])
    Assert-True (($secondaryMissing.Name -join ',') -eq 'secondary.7z,secondary.rar') `
        "Special-source missing selection should support .7z/.rar and exclude matching JSON"

    $fingerprintItems = @($sources | ForEach-Object { Get-MissionArchiveSampleItems -Source $_ })
    Assert-True ($fingerprintItems.Count -eq 5) "Fingerprint discovery should include both archive sources and RAR files"
    Assert-True ($fingerprintItems.Name -contains 'mission_files/primary.zip') `
        "Primary fingerprint sample keys should include their source"
    Assert-True ($fingerprintItems.Name -contains 'd2xxl_downloads/secondary.7z') `
        "D2X-XL fingerprint sample keys should include their source"

    Remove-Item -LiteralPath (Join-Path $secondary.FullName "secondary.7z"), `
    (Join-Path $secondary.FullName "complete.zip"), (Join-Path $secondary.FullName "secondary.rar")
    $available = @(Get-AvailableMissionArchiveSources -Sources $sources)
    Assert-True ($available.Count -eq 1) "An empty optional source should be skipped"

    Remove-Item -LiteralPath (Join-Path $primary.FullName "primary.zip"), `
    (Join-Path $primary.FullName "complete.7z")
    $requiredFailure = $false
    try {
        Get-AvailableMissionArchiveSources -Sources $sources | Out-Null
    } catch {
        $requiredFailure = $_.Exception.Message -like "No mission archives found*"
    }
    Assert-True $requiredFailure "An empty required source should fail"
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}

Write-Host "Mission archive source tests passed"
