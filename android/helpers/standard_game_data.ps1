function Get-StandardGameDataDeps {
    # Keep emulator provisioning and host metadata generation on identical data
    return @(
        @{file = "descent2.hog"; sha256 = "f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703" }
        @{file = "descent2.ham"; sha256 = "5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d" }
        @{file = "groupa.pig"; sha256 = "facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b" }
        @{file = "descent2.s22"; sha256 = "4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728" }
        @{file = "alien1.pig"; sha256 = "811fc58caa3e2a72cdfa07d7530b2bb0ca71836a6a2d8a3cb401e4284949c233" }
        @{file = "alien2.pig"; sha256 = "75ef8fa0cba03410c61ad1b58f57dcb1481f1f302985828aab0af90639926055" }
        @{file = "fire.pig"; sha256 = "26a5a5f4e91456abf31f79578d0922e7bc3348b6aa92489a84033de83f358156" }
        @{file = "ice.pig"; sha256 = "ae6152ef69502b00e51a98d8f04b21f2855a332cd2988ecceb3b909a49fa26a1" }
        @{file = "water.pig"; sha256 = "de88ead87dcb32f16936b3e2a08b81a2248440f29e6f8be0c4c3a5f9fe4b63c1" }
        @{file = "descent.hog"; sha256 = "83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052" }
        @{file = "descent.pig"; sha256 = "093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe" }
    )
}

function Get-StandardGameDataCandidates {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][ValidateSet('d1', 'd2')][string]$Game
    )

    # Prefer existing staging; discover extracted media without copying game data
    $legacy = if ($Game -eq 'd1') {
        @('game_data/extracted/d1 mac extracted', 'game_data_to_copy_to_emulator/data', 'game_data_to_copy_to_emulator/temp')
    } else {
        @('game_data_to_copy_to_emulator/temp', 'game_data_to_copy_to_emulator/data', 'game_data/extracted/descent 2 demo 1-0_extracted')
    }
    $candidates = @($legacy | ForEach-Object { Join-Path $RepoRoot $_ })
    $gameData = Join-Path $RepoRoot 'game_data'
    $archiveName = if ($Game -eq 'd1') { 'descent.hog' } else { 'descent2.hog' }
    if (Test-Path -LiteralPath $gameData -PathType Container) {
        $candidates += @(Get-ChildItem -LiteralPath $gameData -Recurse -File -Filter '*.hog' |
                Where-Object Name -eq $archiveName | Select-Object -ExpandProperty DirectoryName | Sort-Object -Unique)
    }
    # Resolution must still validate all required hashes, not only the HOG name
    $candidates | Select-Object -Unique
}

function Resolve-StandardGameDataDirectory {
    param(
        [Parameter(Mandatory = $true)][string[]]$Candidates,
        [Parameter(Mandatory = $true)][object[]]$Dependencies,
        [Parameter(Mandatory = $true)][string]$Label
    )

    foreach ($dir in $Candidates) {
        if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
            continue
        }
        $filesByName = @{}
        Get-ChildItem -LiteralPath $dir -File | ForEach-Object {
            $filesByName[$_.Name.ToLowerInvariant()] = $_
        }
        $hashes = [ordered]@{}
        foreach ($dependency in $Dependencies) {
            $name = ([string]$dependency.file).ToLowerInvariant()
            $file = $filesByName[$name]
            if (-not $file) {
                $hashes = $null
                break
            }
            $actual = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($actual -cne ([string]$dependency.sha256).ToLowerInvariant()) {
                $hashes = $null
                break
            }
            $hashes[$name] = $actual
        }
        if ($hashes) {
            return [pscustomobject]@{
                Path = (Resolve-Path -LiteralPath $dir).Path
                Hashes = $hashes
            }
        }
    }
    throw "$Label data directory matching pinned hashes not found"
}

function New-StandardGameDataStage {
    param(
        [Parameter(Mandatory)][string]$Destination,
        [Parameter(Mandatory)][string[]]$Candidates,
        [Parameter(Mandatory)][object[]]$Dependencies
    )

    # The caller owns the parent run directory and its producer lock
    $Destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Destination)
    if (Get-Item -LiteralPath $Destination -Force -ErrorAction SilentlyContinue) {
        throw "Game data stage already exists: $Destination"
    }
    $files = @($Candidates | Select-Object -Unique | Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
            ForEach-Object { Get-ChildItem -LiteralPath $_ -File })
    $selected = [ordered]@{}
    $bytes = 0L
    foreach ($dependency in $Dependencies) {
        $name = ([string]$dependency.file).ToLowerInvariant()
        if ($name -match '[/\\]' -or $name -in @('', '.', '..') -or $selected.Contains($name)) {
            throw "Invalid or duplicate game data filename: $name"
        }
        $source = @($files | Where-Object Name -ieq $name | Where-Object {
                (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -ieq [string]$dependency.sha256
            } | Select-Object -First 1)
        if (-not $source.Count) { throw "Game data file matching pinned hash not found: $name" }
        $selected[$name] = $source[0]
        $bytes += $source[0].Length
    }
    if (-not $selected.Count) { throw 'No game data files were requested' }
    . (Join-Path $PSScriptRoot 'output_disk_space.ps1')
    Assert-OutputDiskSpace -Paths @($Destination) -MinimumFreeGB (4 + $bytes / 1GB)
    $created = $false
    $complete = $false
    try {
        New-Item -ItemType Directory -Path $Destination -ErrorAction Stop | Out-Null
        $created = $true
        foreach ($name in $selected.Keys) {
            [IO.File]::Copy($selected[$name].FullName, (Join-Path $Destination $name), $false)
        }
        # Recheck copied bytes, including changes to source files during staging
        $result = Resolve-StandardGameDataDirectory -Candidates @($Destination) -Dependencies $Dependencies -Label 'staged game'
        $complete = $true
        return $result
    } finally {
        if ($created -and -not $complete) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    }
}
