#!/usr/bin/env pwsh
# generate_game_data_index.ps1 -- Build a SHA-256 hash index of local game data files.
#
# Scans local game data directories for required test game asset files and writes
# game_data/game_data_index.txt with format:  <sha256>  <relative-path>
# Paths are relative to the repo root. One canonical path per hash is kept.
# Used by test infrastructure to resolve declarative game data dependencies.
#
# Usage:  .\generate_game_data_index.ps1
#         .\generate_game_data_index.ps1 -AllLocalFiles

param([switch]$AllLocalFiles)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot  = [System.IO.Path]::GetFullPath((Split-Path $ScriptDir))
$OutFile   = Join-Path $ScriptDir "game_data_index.txt"

$GameExtensions = @(".hog", ".pig", ".ham", ".mvl", ".s11", ".s22", ".mn2", ".zip", ".7z", ".gog", ".inst", ".exe", ".pkg", ".dxa")

function Join-RepoPath {
    param(
        [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
        [string[]]$Segments
    )

    $path = $RepoRoot
    foreach ($segment in $Segments) {
        $path = Join-Path $path $segment
    }
    return $path
}

function Add-RequiredHash {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$HashSet,
        [Parameter(Mandatory = $true)][string]$Hash
    )

    if ($Hash -match '^[0-9a-fA-F]{64}$') {
        $HashSet[$Hash.ToLowerInvariant()] = $true
    }
}

function Get-JsoncObject {
    param([Parameter(Mandatory = $true)][string]$Path)

    $raw = Get-Content -LiteralPath $Path -Raw
    $raw = [regex]::Replace($raw, '(?s)/\*.*?\*/', '')
    $raw = [regex]::Replace($raw, '(?m)//.*$', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    return $raw | ConvertFrom-Json
}

function Get-DeclaredGameDataHash {
    param([Parameter(Mandatory = $true)][string]$Path)

    try {
        $document = Get-JsoncObject -Path $Path
    } catch {
        return @()
    }

    $root = @($document)[0]
    if (-not $root._info -or -not $root._info._deps) {
        return @()
    }

    $placeholderHashes = @{}
    if ($root._info.params) {
        foreach ($paramDef in $root._info.params.PSObject.Properties) {
            if (-not $paramDef.Value.options) { continue }
            foreach ($option in $paramDef.Value.options.PSObject.Properties) {
                foreach ($property in $option.Value.PSObject.Properties) {
                    $value = [string]$property.Value
                    if ($value -notmatch '^[0-9a-fA-F]{64}$') { continue }
                    if (-not $placeholderHashes.ContainsKey($property.Name)) {
                        $placeholderHashes[$property.Name] = [System.Collections.Generic.List[string]]::new()
                    }
                    $placeholderHashes[$property.Name].Add($value.ToLowerInvariant())
                }
            }
        }
    }

    $hashes = [ordered]@{}
    foreach ($dep in @($root._info._deps)) {
        $shaText = [string]$dep.sha256
        Add-RequiredHash -HashSet $hashes -Hash $shaText
        foreach ($match in [regex]::Matches($shaText, '\$\{([^}]+)\}')) {
            $placeholderName = $match.Groups[1].Value
            if (-not $placeholderHashes.ContainsKey($placeholderName)) { continue }
            foreach ($hash in $placeholderHashes[$placeholderName]) {
                Add-RequiredHash -HashSet $hashes -Hash $hash
            }
        }
    }
    return $hashes.Keys
}

function Get-RequiredGameDataHash {
    $hashes = [ordered]@{}

    $powerShellSources = @(
        (Join-RepoPath "android" "helpers" "test_helpers.ps1"),
        (Join-RepoPath "android" "tests" "run_input_demo_replay.ps1"),
        (Join-RepoPath "android" "tests" "test_input_demo_runtime_smoke.ps1")
    )

    foreach ($source in $powerShellSources) {
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        $text = Get-Content -LiteralPath $source -Raw
        foreach ($match in [regex]::Matches($text, '(?i)\bsha256\s*=\s*[''\"]([0-9a-f]{64})[''\"]')) {
            Add-RequiredHash -HashSet $hashes -Hash $match.Groups[1].Value
        }
    }

    $gameScriptDir = Join-RepoPath "android" "game_scripts"
    if (Test-Path -LiteralPath $gameScriptDir -PathType Container) {
        foreach ($script in Get-ChildItem -LiteralPath $gameScriptDir -Recurse -File -Filter "*.jsonc" -ErrorAction SilentlyContinue) {
            foreach ($hash in Get-DeclaredGameDataHash -Path $script.FullName) {
                Add-RequiredHash -HashSet $hashes -Hash $hash
            }
        }
    }

    return $hashes.Keys
}

function Get-RelativeRepoPath {
    param([Parameter(Mandatory = $true)][System.IO.FileSystemInfo]$File)

    $relativePath = [System.IO.Path]::GetRelativePath($RepoRoot, $File.FullName)
    return $relativePath.Replace([System.IO.Path]::DirectorySeparatorChar, '/').Replace([System.IO.Path]::AltDirectorySeparatorChar, '/')
}

function Get-RelativeDirectoryPath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $lastSlash = $RelativePath.LastIndexOf('/')
    if ($lastSlash -lt 0) {
        return ''
    }
    return $RelativePath.Substring(0, $lastSlash)
}

function Get-CanonicalSortKey {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][int]$DirectoryPriority,
        [Parameter(Mandatory = $true)][int]$DirectoryRequiredHashCount
    )

    $volatileRank = 0
    if ($RelativePath.StartsWith('game_data_to_copy_to_emulator/temp/', [System.StringComparison]::OrdinalIgnoreCase)) {
        $volatileRank = 1
    }

    $directoryCohesionRank = 9999 - [Math]::Min($DirectoryRequiredHashCount, 9999)
    return '{0:D2}|{1:D4}|{2:D2}|{3}|{4}' -f $volatileRank, $directoryCohesionRank, $DirectoryPriority, $RelativePath.ToLowerInvariant(), $RelativePath
}

function Get-SortedCanonicalRecord {
    param([AllowEmptyCollection()][object[]]$Records)

    if ($Records.Count -eq 0) {
        return @()
    }

    [System.Array]::Sort($Records, [System.Comparison[object]]{
            param($Left, $Right)
            return [System.StringComparer]::Ordinal.Compare($Left.SortKey, $Right.SortKey)
        })
    return $Records
}

# Search directories in priority order. Temp is scanned but loses duplicate ties.
$SearchDirs = @(
    (Join-RepoPath "game_data_to_copy_to_emulator" "temp"),
    (Join-RepoPath "game_data_to_copy_to_emulator" "data"),
    (Join-RepoPath "game_data" "CD images"),
    (Join-RepoPath "game_data" "gog installers"),
    (Join-RepoPath "game_data" "demo installers"),
    (Join-RepoPath "game_data" "extracted" "VERTIGO"),
    (Join-RepoPath "game_data" "extracted" "d1 mac extracted"),
    (Join-RepoPath "game_data" "extracted" "d2_mac_demo"),
    (Join-RepoPath "game_data" "extracted" "descent 1 demo 1-4_extracted"),
    (Join-RepoPath "game_data" "extracted" "descent 2 demo 1-0_extracted"),
    (Join-RepoPath "game_data" "mods"),
    (Join-RepoPath "game_data")
)

# hash -> relative path (best canonical candidate wins)
$index = [ordered]@{}
$records = [System.Collections.Generic.List[object]]::new()
$requiredHashes = [ordered]@{}
if (-not $AllLocalFiles) {
    foreach ($hash in Get-RequiredGameDataHash) {
        $requiredHashes[$hash] = $true
    }
}

for ($dirIndex = 0; $dirIndex -lt $SearchDirs.Count; $dirIndex++) {
    $dir = $SearchDirs[$dirIndex]
    if (-not (Test-Path -LiteralPath $dir)) { continue }
    $files = Get-ChildItem -LiteralPath $dir -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $GameExtensions -contains $_.Extension.ToLower() -and $_.Length -gt 1 }
    foreach ($file in $files) {
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if (-not $AllLocalFiles -and -not $requiredHashes.Contains($hash)) { continue }
        $relativePath = Get-RelativeRepoPath -File $file
        $records.Add([pscustomobject]@{
                Hash = $hash
                RelativePath = $relativePath
                Directory = Get-RelativeDirectoryPath -RelativePath $relativePath
                DirectoryPriority = $dirIndex
                SortKey = $null
            })
    }
}

$directoryHashes = @{}
foreach ($record in $records) {
    if (-not $directoryHashes.ContainsKey($record.Directory)) {
        $directoryHashes[$record.Directory] = [ordered]@{}
    }
    $directoryHashes[$record.Directory][$record.Hash] = $true
}
foreach ($record in $records) {
    $record.SortKey = Get-CanonicalSortKey `
        -RelativePath $record.RelativePath `
        -DirectoryPriority $record.DirectoryPriority `
        -DirectoryRequiredHashCount $directoryHashes[$record.Directory].Count
}

foreach ($record in (Get-SortedCanonicalRecord -Records $records.ToArray())) {
    if (-not $index.Contains($record.Hash)) {
        $index[$record.Hash] = $record.RelativePath
    }
}

if (-not $AllLocalFiles) {
    $missing = @($requiredHashes.Keys | Where-Object { -not $index.Contains($_) } | Sort-Object)
    if ($missing.Count -gt 0) {
        Write-Warning "Could not resolve $($missing.Count) required game data hash(es)"
        foreach ($hash in $missing) {
            Write-Warning "  $hash"
        }
    }
}

# Write index file
$lines = @("# game_data_index.txt -- SHA-256 hash index of local game data files",
           "# Generated by generate_game_data_index.ps1 -- do not edit manually",
           "# Format: <sha256>  <relative-path-from-repo-root>",
           "")
foreach ($hash in $index.Keys) {
    $lines += "$hash  $($index[$hash])"
}
$lines += ""
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($OutFile, ($lines -join "`n"), $utf8NoBom)

if ($AllLocalFiles) {
    Write-Output "Wrote $($index.Count) entries to $OutFile"
} else {
    Write-Output "Wrote $($index.Count) of $($requiredHashes.Count) required entries to $OutFile"
}
