# Shared host access to the launcher mission archive variant policy

function Initialize-MetadataKotlinCli {
    $cli = Join-Path $androidRoot 'mission-metadata-cli\build\install\mission-metadata-cli\bin\mission-metadata-cli.bat'
    $jdkHome = 'C:\local\jdk-21'
    if (Test-Path -LiteralPath $jdkHome -PathType Container) {
        $env:JAVA_HOME = $jdkHome
        $env:Path = "$jdkHome\bin;$env:Path"
    }
    if (-not $NoBuild) {
        & (Join-Path $androidRoot 'gradlew.bat') -p $androidRoot :mission-metadata-cli:installDist --console=plain 2>&1 |
            ForEach-Object { Write-Host ([string]$_) }
        if ($LASTEXITCODE -ne 0) { throw "Mission metadata Kotlin CLI build failed with exit code $LASTEXITCODE" }
    }
    if (-not (Test-Path -LiteralPath $cli -PathType Leaf)) { throw "Mission metadata Kotlin CLI not found: $cli" }
    return $cli
}

function Get-PreferredMissionChildArchive {
    param([string[]]$Names)

    if (-not $Names) { return '' }
    if (-not (Get-Variable -Name missionArchiveVariantCli -Scope Script -ErrorAction SilentlyContinue) -or
        -not $script:missionArchiveVariantCli) {
        $script:missionArchiveVariantCli = Initialize-MetadataKotlinCli
    }
    $output = @(& $script:missionArchiveVariantCli --select-archive-variant @Names)
    if ($LASTEXITCODE -ne 0) { throw 'Shared mission archive variant selection failed' }
    $selected = @($output | Where-Object { $_.StartsWith("DXXVARIANT`t") } | ForEach-Object { $_.Substring(11) })
    if ($selected.Count -eq 0) { return '' }
    if ($selected.Count -ne 1 -or $selected[0] -notin $Names) {
        throw 'Shared mission archive variant selection returned an invalid child'
    }
    return $selected[0]
}

function Expand-MissionZipContent {
    param(
        [Parameter(Mandatory)][string]$ArchivePath,
        [Parameter(Mandatory)][string]$Destination
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    $childPath = ''
    try {
        try {
            $names = @($archive.Entries | ForEach-Object FullName)
            $descriptors = @($names | Where-Object { [IO.Path]::GetExtension($_) -in @('.msn', '.mn2') })
            if ($descriptors.Count -eq 0) {
                $selected = Get-PreferredMissionChildArchive -Names @($names | Where-Object { [IO.Path]::GetExtension($_) -eq '.zip' })
                if ($selected) {
                    # Stage only the selected child, never the unused XL soundtrack
                    $childPath = Join-Path $Destination ([guid]::NewGuid().ToString('N') + '.zip')
                    [IO.Compression.ZipFileExtensions]::ExtractToFile($archive.GetEntry($selected), $childPath)
                }
            }
        } finally {
            $archive.Dispose()
        }
        $source = if ($childPath) { $childPath } else { $ArchivePath }
        [IO.Compression.ZipFile]::ExtractToDirectory($source, $Destination)
    } finally {
        if ($childPath -and (Test-Path -LiteralPath $childPath)) { Remove-Item -LiteralPath $childPath -Force }
    }
}
