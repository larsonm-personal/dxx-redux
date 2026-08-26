function Get-MissionMetadataArchiveSources {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    return @(
        [pscustomobject]@{
            Id = "mission_files"
            Directory = Join-Path $RepoRoot "game_data\mission_files"
            Required = $true
        },
        [pscustomobject]@{
            Id = "d2xxl_downloads"
            Directory = Join-Path $RepoRoot "game_data\mission_files\d2xxl_downloads"
            Required = $false
        }
    )
}

function Get-AvailableMissionMetadataArchiveSources {
    param([Parameter(Mandatory = $true)][object[]]$Sources)

    $available = @()
    foreach ($source in $Sources) {
        if (-not (Test-Path -LiteralPath $source.Directory -PathType Container)) {
            if ($source.Required) {
                throw "Mission metadata source directory not found: $($source.Directory)"
            }
            continue
        }
        $archives = @(Get-ChildItem -LiteralPath $source.Directory -File |
                Where-Object { $_.Extension.ToLowerInvariant() -in @(".zip", ".7z") })
        if ($archives.Count -gt 0) {
            $available += $source
        } elseif ($source.Required) {
            throw "No mission archives found in $($source.Directory)"
        }
    }
    return $available
}

function Get-MissionMetadataArchives {
    param([Parameter(Mandatory = $true)]$Source)

    return @(Get-ChildItem -LiteralPath $Source.Directory -File |
            Where-Object { $_.Extension.ToLowerInvariant() -in @(".zip", ".7z") } |
            Sort-Object Name)
}

function Get-MissingMissionMetadataArchives {
    param([Parameter(Mandatory = $true)]$Source)

    return @(Get-MissionMetadataArchives -Source $Source | Where-Object {
            $regressionPath = Join-Path $_.DirectoryName "$($_.BaseName).json"
            -not (Test-Path -LiteralPath $regressionPath -PathType Leaf)
        })
}
