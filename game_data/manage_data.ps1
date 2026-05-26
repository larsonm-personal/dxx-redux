#!/usr/bin/env pwsh

param(
    [ValidateSet("Menu", "List", "Regenerate", "Verify", "Check", "Export")]
    [string]$Action = "Menu",

    [string]$ManifestPath,

    [string]$ZipPath
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $ScriptDir "test_data_manifest.json"
}
if (-not $ZipPath) {
    $ZipPath = Join-Path $ScriptDir "test_data.zip"
}

$CdExtensions = @(".bin", ".cue", ".iso", ".img", ".ccd", ".sub")
$GogExtensions = @(".exe", ".pkg")
$DemoExtensions = @(".exe", ".zip", ".sit", ".hqx")
$MusicExtensions = @(".mp3")
$MusicTestRoots = @(
    "game_data/music/D2 infinite abyss redbook mp3/",
    "game_data/music/D2 redbook mp3 rips/"
)
$ExcludedPathParts = @("extracted", "data_tracks")

function ConvertTo-RepoRelativePath {
    param([string]$Path)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetFullPath($RepoRoot)
    if (-not $root.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $root += [System.IO.Path]::DirectorySeparatorChar
    }
    if (-not $fullPath.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside repo root: $Path"
    }
    return $fullPath.Substring($root.Length).Replace('\', '/')
}

function ConvertTo-FullPath {
    param([string]$RelativePath)
    if ([System.IO.Path]::IsPathRooted($RelativePath)) {
        throw "Manifest path must be relative: $RelativePath"
    }
    return Join-Path $RepoRoot ($RelativePath.Replace('/', '\'))
}

function Test-SourceDataFile {
    param([System.IO.FileInfo]$File)

    $relative = ConvertTo-RepoRelativePath $File.FullName
    $parts = $relative -split "/"
    foreach ($part in $parts) {
        if ($ExcludedPathParts -contains $part) {
            return $false
        }
    }

    $extension = $File.Extension.ToLowerInvariant()
    if ($relative.StartsWith("game_data/CD images/", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $CdExtensions -contains $extension
    }
    if ($relative.StartsWith("game_data/gog installers/", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $GogExtensions -contains $extension
    }
    if ($relative.StartsWith("game_data/demo installers/", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $DemoExtensions -contains $extension
    }
    foreach ($musicRoot in $MusicTestRoots) {
        if ($relative.StartsWith($musicRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $MusicExtensions -contains $extension
        }
    }
    return $false
}

function Get-TestDataKind {
    param([string]$RelativePath)
    if ($RelativePath.StartsWith("game_data/CD images/", [System.StringComparison]::OrdinalIgnoreCase)) {
        return "cd_image"
    }
    if ($RelativePath.StartsWith("game_data/gog installers/", [System.StringComparison]::OrdinalIgnoreCase)) {
        return "gog_installer"
    }
    if ($RelativePath.StartsWith("game_data/demo installers/", [System.StringComparison]::OrdinalIgnoreCase)) {
        return "demo_installer"
    }
    foreach ($musicRoot in $MusicTestRoots) {
        if ($RelativePath.StartsWith($musicRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            return "d2_redbook_mp3"
        }
    }
    return "unknown"
}

function Get-TestDataFiles {
    Get-ChildItem -LiteralPath $ScriptDir -File -Recurse -Force |
        Where-Object { Test-SourceDataFile $_ } |
        Sort-Object { ConvertTo-RepoRelativePath $_.FullName }
}

function Get-FileSha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Read-TestDataManifest {
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "Manifest not found: $ManifestPath"
    }
    return Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
}

function Show-TestDataManifest {
    $manifest = Read-TestDataManifest
    $manifestFiles = @($manifest.files)
    Write-Host "Manifest: $ManifestPath"
    Write-Host "Files: $($manifestFiles.Count), total size: $([math]::Round([Int64]$manifest.total_size / 1GB, 2)) GB"
    foreach ($entry in $manifestFiles) {
        Write-Host "$($entry.kind) $($entry.size) $($entry.sha256) $($entry.path)"
    }
}

function Write-TestDataManifest {
    $files = @(Get-TestDataFiles)
    $entries = @()
    $index = 0
    $totalSize = ($files | Measure-Object Length -Sum).Sum
    if (-not $totalSize) {
        $totalSize = 0
    }

    Write-Host "Hashing $($files.Count) test data files ($([math]::Round($totalSize / 1GB, 2)) GB)"
    foreach ($file in $files) {
        $index++
        $relative = ConvertTo-RepoRelativePath $file.FullName
        Write-Host "  [$index/$($files.Count)] $relative"
        $entries += [pscustomobject]@{
            path = $relative
            name = $file.Name
            size = $file.Length
            sha256 = Get-FileSha256 $file.FullName
            kind = Get-TestDataKind $relative
        }
    }

    $manifest = [ordered]@{
        schema = 1
        generated_by = "game_data/manage_data.ps1"
        base_path = "dxx-redux"
        file_count = $entries.Count
        total_size = [Int64]$totalSize
        files = $entries
    }

    $json = $manifest | ConvertTo-Json -Depth 5
    Set-Content -LiteralPath $ManifestPath -Value ($json + "`n") -NoNewline -Encoding UTF8
    Write-Host "Wrote $($entries.Count) entries to $ManifestPath"
}

function Compare-TestDataManifest {
    $manifest = Read-TestDataManifest
    $manifestFiles = @($manifest.files)
    $manifestByPath = @{}
    $failures = New-Object System.Collections.Generic.List[string]

    foreach ($entry in $manifestFiles) {
        if (-not $entry.path) {
            $failures.Add("Manifest entry is missing path")
            continue
        }
        if ($manifestByPath.ContainsKey($entry.path)) {
            $failures.Add("Duplicate manifest path: $($entry.path)")
            continue
        }
        $manifestByPath[$entry.path] = $entry
    }

    $actualFiles = @(Get-TestDataFiles)
    $actualPaths = @{}
    foreach ($file in $actualFiles) {
        $actualPaths[(ConvertTo-RepoRelativePath $file.FullName)] = $file
    }

    foreach ($actualPath in $actualPaths.Keys) {
        if (-not $manifestByPath.ContainsKey($actualPath)) {
            $failures.Add("Unlisted test data file: $actualPath")
        }
    }

    $index = 0
    foreach ($entry in $manifestFiles) {
        $index++
        $fullPath = ConvertTo-FullPath $entry.path
        Write-Host "  [$index/$($manifestFiles.Count)] $($entry.path)"

        if ($entry.name -ne [System.IO.Path]::GetFileName($entry.path)) {
            $failures.Add("Name mismatch in manifest for $($entry.path): $($entry.name)")
        }
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            $failures.Add("Missing file: $($entry.path)")
            continue
        }

        $file = Get-Item -LiteralPath $fullPath
        if ([Int64]$entry.size -ne [Int64]$file.Length) {
            $failures.Add("Size mismatch for $($entry.path): expected $($entry.size), got $($file.Length)")
            continue
        }

        $hash = Get-FileSha256 $fullPath
        if ($hash -ne $entry.sha256) {
            $failures.Add("Hash mismatch for $($entry.path): expected $($entry.sha256), got $hash")
        }
    }

    if ($failures.Count -gt 0) {
        Write-Host "Verification failed with $($failures.Count) issue(s)" -ForegroundColor Red
        foreach ($failure in $failures) {
            Write-Host "  $failure" -ForegroundColor Red
        }
        return $false
    }

    Write-Host "Verified $($manifestFiles.Count) test data files"
    return $true
}

function Export-TestDataZip {
    $manifest = Read-TestDataManifest
    $manifestFiles = @($manifest.files)
    $zipFullPath = [System.IO.Path]::GetFullPath($ZipPath)
    $zipDir = Split-Path $zipFullPath
    if ($zipDir -and -not (Test-Path -LiteralPath $zipDir)) {
        New-Item -ItemType Directory -Path $zipDir | Out-Null
    }
    if (Test-Path -LiteralPath $zipFullPath) {
        Remove-Item -LiteralPath $zipFullPath -Force
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::Open($zipFullPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $index = 0
        foreach ($entry in $manifestFiles) {
            $index++
            $fullPath = ConvertTo-FullPath $entry.path
            if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
                throw "Cannot export missing file: $($entry.path)"
            }
            $file = Get-Item -LiteralPath $fullPath
            if ([Int64]$entry.size -ne [Int64]$file.Length) {
                throw "Cannot export file with size mismatch: $($entry.path)"
            }
            $hash = Get-FileSha256 $fullPath
            if ($hash -ne $entry.sha256) {
                throw "Cannot export file with hash mismatch: $($entry.path)"
            }
            Write-Host "  [$index/$($manifestFiles.Count)] $($entry.path)"
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zip,
                $fullPath,
                $entry.path,
                [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $zip.Dispose()
    }

    Write-Host "Exported $($manifestFiles.Count) files to $zipFullPath"
}

function Show-Menu {
    while ($true) {
        Write-Host ""
        Write-Host "Game data manager"
        Write-Host "1. Regenerate the test data JSON manifest"
        Write-Host "2. List the manifest entries"
        Write-Host "3. Verify name, size, and hash of all test data"
        Write-Host "4. Export listed test data to a zip file"
        Write-Host "5. Exit"
        $choice = Read-Host "Select an option"
        switch ($choice) {
            "1" { Write-TestDataManifest; return }
            "2" { Show-TestDataManifest; return }
            "3" { if (-not (Compare-TestDataManifest)) { exit 1 }; return }
            "4" { Export-TestDataZip; return }
            "5" { return }
            default { Write-Host "Select 1, 2, 3, 4, or 5" -ForegroundColor Yellow }
        }
    }
}

switch ($Action) {
    "Menu" { Show-Menu }
    "List" { Show-TestDataManifest }
    "Regenerate" { Write-TestDataManifest }
    "Verify" { if (-not (Compare-TestDataManifest)) { exit 1 } }
    "Check" { if (-not (Compare-TestDataManifest)) { exit 1 } }
    "Export" { Export-TestDataZip }
}
