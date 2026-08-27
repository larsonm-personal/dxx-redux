function Get-MissionArchiveSources {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    return @(
        [pscustomobject]@{
            Id = "mission_files"
            Directory = Join-Path $RepoRoot "game_data\mission_files"
            Required = $true
            FingerprintAlbumPrefix = "Mission ZIP - "
        },
        [pscustomobject]@{
            Id = "d2xxl_downloads"
            Directory = Join-Path $RepoRoot "game_data\mission_files\d2xxl_downloads"
            Required = $false
            FingerprintAlbumPrefix = "Mission ZIP - D2X-XL - "
        }
    )
}

function Get-AvailableMissionArchiveSources {
    param(
        [Parameter(Mandatory = $true)][object[]]$Sources,
        [string[]]$Extensions = @(".zip", ".7z")
    )

    $available = @()
    foreach ($source in $Sources) {
        if (-not (Test-Path -LiteralPath $source.Directory -PathType Container)) {
            if ($source.Required) {
                throw "Mission archive source directory not found: $($source.Directory)"
            }
            continue
        }
        $archives = @(Get-MissionArchives -Source $source -Extensions $Extensions)
        if ($archives.Count -gt 0) {
            $available += $source
        } elseif ($source.Required) {
            throw "No mission archives found in $($source.Directory)"
        }
    }
    return $available
}

function Get-MissionArchives {
    param(
        [Parameter(Mandatory = $true)]$Source,
        [string[]]$Extensions = @(".zip", ".7z")
    )

    return @(Get-ChildItem -LiteralPath $Source.Directory -File |
            Where-Object { $_.Extension.ToLowerInvariant() -in $Extensions } |
            Sort-Object Name)
}

function Get-MissionArchiveSampleItems {
    param(
        [Parameter(Mandatory = $true)]$Source,
        [string[]]$Extensions = @(".zip", ".7z", ".rar")
    )

    return @(Get-MissionArchives -Source $Source -Extensions $Extensions | ForEach-Object {
            [pscustomobject]@{
                Name = "$($Source.Id)/$($_.Name)"
                Archive = $_
                Source = $Source
            }
        })
}

function Get-MissionMetadataFiles {
    param([Parameter(Mandatory = $true)]$Source)

    return @(Get-ChildItem -LiteralPath $Source.Directory -File -Filter "*.json" |
            Where-Object { $_.Name -notlike "*.tracklist.json" } |
            Sort-Object Name |
            ForEach-Object {
                $metadataKey = if ($Source.Id -eq "mission_files") { $_.Name } else { "$($Source.Id)/$($_.Name)" }
                $_ | Add-Member -NotePropertyName MissionSourceId -NotePropertyValue $Source.Id -Force
                $_ | Add-Member -NotePropertyName MissionMetadataKey -NotePropertyValue $metadataKey -Force
                $_
            })
}

function Get-MissionTracklistFiles {
    param([Parameter(Mandatory = $true)]$Source)

    return @(Get-ChildItem -LiteralPath $Source.Directory -File -Filter "*.tracklist.json" | Sort-Object Name)
}

function Get-MissingMissionMetadataArchives {
    param([Parameter(Mandatory = $true)]$Source)

    return @(Get-MissionArchives -Source $Source | Where-Object {
            $regressionPath = Join-Path $_.DirectoryName "$($_.BaseName).json"
            -not (Test-Path -LiteralPath $regressionPath -PathType Leaf)
        })
}
