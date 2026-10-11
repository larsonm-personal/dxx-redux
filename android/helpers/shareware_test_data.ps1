#!/usr/bin/env pwsh
# Pinned public PC shareware fixtures; resolving data never downloads or runs installers

. (Join-Path $PSScriptRoot 'verified_dependencies.ps1')

function Get-SharewareTestGameConfig {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][ValidateSet('d1', 'd2')][string]$Game
    )

    $archive = if ($Game -eq 'd1') { 'desc14sw.exe' } else { 'd2demo10.zip' }
    $oracles = Get-Content -LiteralPath (Join-Path $RepoRoot 'android/tests/fixtures/demo_import_oracles.json') -Raw | ConvertFrom-Json
    $oracle = @($oracles.archives | Where-Object archive -CEQ $archive)
    $manifest = Get-Content -LiteralPath (Join-Path $RepoRoot 'game_data/test_data_manifest.json') -Raw | ConvertFrom-Json
    $package = @($manifest.files | Where-Object path -CEQ "game_data/demo installers/$archive")
    if ($oracle.Count -ne 1 -or $package.Count -ne 1 -or $oracle[0].sha256 -cne $package[0].sha256) {
        throw "Shareware package identity disagrees between the source manifest and import oracle: $archive"
    }
    $dataDir = Join-Path $RepoRoot "game_data/shareware/$Game"
    return @{
        Name = $Game
        Game = $Game
        Edition = 'shareware'
        Mission = if ($Game -eq 'd1') { 'd1' } else { 'd2demo' }
        MissionTitle = if ($Game -eq 'd1') { 'Descent Demo' } else { 'Descent 2 Demo' }
        FirstLevel = if ($Game -eq 'd1') { 'level01.sdl' } else { 'd2leva-1.sl2' }
        LevelCount = if ($Game -eq 'd1') { 7 } else { 3 }
        TitleArg = if ($Game -eq 'd1') { '-notitles' } else { '-nomovies' }
        DataDir = $dataDir
        DefaultDataDirs = @($dataDir)
        RequiredFiles = @($oracle[0].files | ForEach-Object { [string]$_.file })
        RequiredHashes = @($oracle[0].files | ForEach-Object {
                @{ File = [string]$_.file; Sha256 = [string]$_.sha256; Size = [long]$_.size }
            })
        Archive = $archive
        ArchiveSha256 = [string]$package[0].sha256
        ArchiveSize = [long]$package[0].size
        # Same public release assets offered by SetupGameFiles.kt
        DownloadUrl = "https://github.com/larsonm-personal/dxx-redux/releases/download/demo_installers/$archive"
    }
}

function Assert-SharewareTestDataDirectory {
    param(
        [Parameter(Mandatory)][hashtable]$Config,
        [Parameter(Mandatory)][string]$DataDir,
        # Used only to validate raw SOW output before copying the pinned files
        [switch]$AllowExtraFiles
    )

    if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) {
        throw "Shareware data directory not found: $DataDir"
    }
    $children = @(Get-ChildItem -LiteralPath $DataDir -Force)
    $files = @{}
    foreach ($file in $children | Where-Object { -not $_.PSIsContainer }) {
        $name = $file.Name.ToLowerInvariant()
        if ($files.ContainsKey($name)) { throw "Ambiguous shareware filename: $name in $DataDir" }
        $files[$name] = $file
    }
    # The engine prefers these retail names over the D2 demo assets
    if ($Config.Name -eq 'd2') {
        foreach ($name in @('descent2.hog', 'descent2.ham', 'descent2.s11', 'descent2.s22', 'groupa.pig')) {
            if ($files.ContainsKey($name)) { throw "Retail file conflicts with the D2 shareware fixture: $name" }
        }
    }
    foreach ($expected in $Config.RequiredHashes) {
        $file = $files[$expected.File.ToLowerInvariant()]
        if (-not $file) { throw "Required shareware file missing: $($expected.File) in $DataDir" }
        if ($file.Length -ne $expected.Size) { throw "Shareware file size mismatch: $($file.FullName)" }
        Assert-DxxFileSha256 -Path $file.FullName -ExpectedSha256 $expected.Sha256 -Label $expected.File | Out-Null
    }
    if (-not $AllowExtraFiles) {
        $allowed = @('.extraction-complete.json') + @($Config.RequiredHashes | ForEach-Object { $_.File.ToLowerInvariant() })
        foreach ($child in $children) {
            if ($child.PSIsContainer -or $child.Name.ToLowerInvariant() -cnotin $allowed) {
                throw "Unexpected shareware fixture entry could shadow pinned game data: $($child.Name)"
            }
        }
    }
    return (Resolve-Path -LiteralPath $DataDir).Path
}

function Resolve-SharewareTestData {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][ValidateSet('d1', 'd2')][string]$Game,
        [string]$DataDir
    )

    $config = Get-SharewareTestGameConfig -RepoRoot $RepoRoot -Game $Game
    if (-not $DataDir) { $DataDir = $config.DataDir }
    try {
        return Assert-SharewareTestDataDirectory -Config $config -DataDir $DataDir
    } catch {
        throw "$($_.Exception.Message)`nPrepare the pinned public fixtures explicitly: pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Game $Game -Download"
    }
}
