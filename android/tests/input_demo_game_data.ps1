#!/usr/bin/env pwsh
# Shared host-side game-data resolver for input-demo tests.

. (Join-Path (Split-Path $PSScriptRoot) 'helpers' 'test_host_platform.ps1')

function Get-InputDemoCaseInsensitiveChildFile {
    param(
        [Parameter(Mandatory)][string]$Directory,
        [Parameter(Mandatory)][string]$Name
    )

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $Directory -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } |
        Select-Object -First 1
}

function Update-InputDemoGameDataHashIndex {
    param([Parameter(Mandatory)][string]$RepoRoot)

    $generator = Join-RegressionPath $RepoRoot 'game_data' 'generate_game_data_index.ps1'
    if (-not (Test-Path -LiteralPath $generator -PathType Leaf)) {
        return $false
    }

    $pwsh = Get-RegressionCurrentPwshPath
    & $pwsh -NoProfile -ExecutionPolicy Bypass -File $generator | Out-Host
    return ($LASTEXITCODE -eq 0)
}

function Read-InputDemoGameDataHashIndex {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [switch]$Regenerate
    )

    $indexFile = Join-RegressionPath $RepoRoot 'game_data' 'game_data_index.txt'
    if ($Regenerate -or -not (Test-Path -LiteralPath $indexFile -PathType Leaf)) {
        [void](Update-InputDemoGameDataHashIndex -RepoRoot $RepoRoot)
    }
    if (-not (Test-Path -LiteralPath $indexFile -PathType Leaf)) {
        return $null
    }

    $index = @{}
    $staleCount = 0
    foreach ($line in [System.IO.File]::ReadLines($indexFile)) {
        if ($line -match '^\s*(#|$)') {
            continue
        }
        $parts = $line -split '\s{2}', 2
        if ($parts.Count -ne 2) {
            continue
        }
        $path = Join-Path $RepoRoot $parts[1]
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $index[$parts[0].ToLowerInvariant()] = (Resolve-Path -LiteralPath $path).Path
        } else {
            $staleCount++
        }
    }

    if ($staleCount -gt 0 -and -not $Regenerate) {
        return Read-InputDemoGameDataHashIndex -RepoRoot $RepoRoot -Regenerate
    }
    return $index
}

function Get-InputDemoIndexedDataDirCandidates {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][hashtable]$Config
    )

    if (-not $Config.ContainsKey('RequiredHashes')) {
        return @()
    }

    $index = Read-InputDemoGameDataHashIndex -RepoRoot $RepoRoot
    if (-not $index) {
        return @()
    }

    $dirs = @()
    foreach ($entry in $Config.RequiredHashes) {
        $path = $index[$entry.Sha256.ToLowerInvariant()]
        if (-not $path) {
            return @()
        }
        $dirs += (Split-Path -Parent $path)
    }

    $uniqueDirs = @($dirs | Select-Object -Unique)
    if ($uniqueDirs.Count -eq 1) {
        return $uniqueDirs
    }
    return @()
}

function Get-InputDemoDiscoveredDataDirCandidates {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][hashtable]$Config
    )

    $firstRequired = $Config.RequiredFiles[0]
    $roots = @(
        (Join-RegressionPath $RepoRoot 'game_data_to_copy_to_emulator'),
        (Join-RegressionPath $RepoRoot 'game_data' 'extracted'),
        (Join-RegressionPath $RepoRoot 'game_data' 'CD images'),
        (Join-RegressionPath $RepoRoot 'game_data' 'gog installers')
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Container } | Select-Object -Unique

    $dirs = [System.Collections.Generic.List[string]]::new()
    foreach ($root in $roots) {
        Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name.Equals($firstRequired, [System.StringComparison]::OrdinalIgnoreCase) } |
            ForEach-Object {
                if (-not $dirs.Contains($_.DirectoryName)) {
                    $dirs.Add($_.DirectoryName)
                }
            }
    }
    return @($dirs | Sort-Object)
}

function Test-InputDemoDataDirMatchesGame {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][hashtable]$Config
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return $false
    }
    foreach ($requiredFile in $Config.RequiredFiles) {
        if (-not (Get-InputDemoCaseInsensitiveChildFile -Directory $Path -Name $requiredFile)) {
            return $false
        }
    }
    return $true
}

function Invoke-InputDemoGameDataExtractionScript {
    param([Parameter(Mandatory)][string]$ScriptPath)

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        return $false
    }

    $pwsh = Get-RegressionCurrentPwshPath
    Write-Host "Game data: running $([System.IO.Path]::GetFileName($ScriptPath)) to materialize extracted files" -ForegroundColor Yellow
    & $pwsh -NoProfile -ExecutionPolicy Bypass -File $ScriptPath | Out-Host
    return ($LASTEXITCODE -eq 0)
}

function Try-InputDemoExtractLocalGameDataSources {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][hashtable]$Config
    )

    if ($env:DXX_SKIP_AUTO_EXTRACT_GAME_DATA -eq '1') {
        return $false
    }

    $gameDataDir = Join-RegressionPath $RepoRoot 'game_data'
    $extractors = @(
        @{
            Script = Join-RegressionPath $gameDataDir 'extract_all_gog.ps1'
            SourceDir = Join-RegressionPath $gameDataDir 'gog installers'
        },
        @{
            Script = Join-RegressionPath $gameDataDir 'extract_all_cds.ps1'
            SourceDir = Join-RegressionPath $gameDataDir 'CD images'
        }
    )

    $ranAny = $false
    foreach ($extractor in $extractors) {
        if (-not (Test-Path -LiteralPath $extractor.SourceDir -PathType Container)) {
            continue
        }
        $sourceCount = @(Get-ChildItem -LiteralPath $extractor.SourceDir -File -Recurse -ErrorAction SilentlyContinue).Count
        if ($sourceCount -eq 0) {
            continue
        }

        [void](Invoke-InputDemoGameDataExtractionScript -ScriptPath $extractor.Script)
        $ranAny = $true
        [void](Update-InputDemoGameDataHashIndex -RepoRoot $RepoRoot)
        if ((Get-InputDemoIndexedDataDirCandidates -RepoRoot $RepoRoot -Config $Config).Count -gt 0) {
            return $true
        }
    }

    return $ranAny
}

function Resolve-InputDemoDataDir {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][hashtable]$Config,
        [string]$RequestedDataDir,
        [string]$Purpose = 'input-demo test'
    )

    $candidates = @()
    if ($RequestedDataDir) {
        $candidates += $RequestedDataDir
    }
    $candidates += Get-InputDemoIndexedDataDirCandidates -RepoRoot $RepoRoot -Config $Config
    $candidates += $Config.DefaultDataDirs
    $candidates += Get-InputDemoDiscoveredDataDirCandidates -RepoRoot $RepoRoot -Config $Config

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (Test-InputDemoDataDirMatchesGame -Path $candidate -Config $Config) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    if (-not $RequestedDataDir -and (Try-InputDemoExtractLocalGameDataSources -RepoRoot $RepoRoot -Config $Config)) {
        $candidates += Get-InputDemoIndexedDataDirCandidates -RepoRoot $RepoRoot -Config $Config
        $candidates += Get-InputDemoDiscoveredDataDirCandidates -RepoRoot $RepoRoot -Config $Config
        foreach ($candidate in ($candidates | Select-Object -Unique)) {
            if (Test-InputDemoDataDirMatchesGame -Path $candidate -Config $Config) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    $missingSummary = foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            $missing = @()
            foreach ($requiredFile in $Config.RequiredFiles) {
                if (-not (Get-InputDemoCaseInsensitiveChildFile -Directory $candidate -Name $requiredFile)) {
                    $missing += $requiredFile
                }
            }
            if ($missing.Count -gt 0) {
                "${candidate} missing: $($missing -join ', ')"
            }
        } else {
            "${candidate} missing: directory not found"
        }
    }

    $gameName = if ($Config.ContainsKey('Name')) { $Config.Name } else { 'game' }
    throw "Could not find a valid data dir for $gameName ($Purpose)`n$($missingSummary -join "`n")"
}
