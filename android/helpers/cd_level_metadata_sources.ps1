function Read-CdLevelMetadataSourceManifest {
    param([Parameter(Mandatory = $true)][string]$Path)

    $text = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    $text = [regex]::Replace($text, '(?m)//.*$', '')
    $text = [regex]::Replace($text, ',\s*([}\]])', '$1')
    return $text | ConvertFrom-Json
}

function Resolve-CdLevelMetadataSources {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$OutputDir
    )

    $repoFull = [IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $outputFull = [IO.Path]::GetFullPath($OutputDir).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $manifest = Read-CdLevelMetadataSourceManifest -Path $ManifestPath
    $resolved = @()
    $ids = @{}
    $outputs = @{}

    foreach ($source in @($manifest.sources)) {
        $id = [string]$source.id
        $sourceDirValue = [string]$source.source_dir
        $descriptorValue = [string]$source.descriptor
        $outputValue = [string]$source.output
        $discover = [bool]$source.discover
        $files = @($source.files | Where-Object { $_ } | ForEach-Object { [string]$_ })
        $excludeDescriptors = @(
            $source.exclude_descriptors |
                Where-Object { $_ } |
                ForEach-Object { ([string]$_).ToLowerInvariant() }
        )
        if (-not $id -or -not $sourceDirValue -or -not $outputValue) {
            throw "CD level metadata sources require id, source_dir, and output"
        }
        if (-not $discover -and (-not $descriptorValue -or $files.Count -eq 0)) {
            throw "Explicit CD level metadata sources require descriptor and files"
        }
        if ($ids.ContainsKey($id)) {
            throw "Duplicate CD level metadata source id: $id"
        }
        $ids[$id] = $true

        $sourceDir = [IO.Path]::GetFullPath((Join-Path $RepoRoot $sourceDirValue))
        if (-not ($sourceDir + [IO.Path]::DirectorySeparatorChar).StartsWith($repoFull, [StringComparison]::OrdinalIgnoreCase)) {
            throw "CD level metadata source is outside the repository: $sourceDirValue"
        }
        if (-not (Test-Path -LiteralPath $sourceDir -PathType Container)) {
            throw "CD level metadata source directory not found: $sourceDirValue"
        }

        $resolvedFiles = @()
        foreach ($fileValue in $files) {
            $filePath = [IO.Path]::GetFullPath((Join-Path $sourceDir $fileValue))
            if (-not ($filePath + [IO.Path]::DirectorySeparatorChar).StartsWith(
                    $sourceDir + [IO.Path]::DirectorySeparatorChar,
                    [StringComparison]::OrdinalIgnoreCase
                )) {
                throw "CD level metadata file is outside its source directory: $fileValue"
            }
            if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
                throw "CD level metadata source file not found: $fileValue"
            }
            $resolvedFiles += Get-Item -LiteralPath $filePath
        }
        $descriptorMatches = @()
        if (-not $discover) {
            $descriptorMatches = @($resolvedFiles | Where-Object {
                    $_.FullName -eq [IO.Path]::GetFullPath((Join-Path $sourceDir $descriptorValue))
                })
            if ($descriptorMatches.Count -ne 1) {
                throw "CD level metadata descriptor must appear exactly once in files: $descriptorValue"
            }
        }

        $outputPath = [IO.Path]::GetFullPath((Join-Path $OutputDir $outputValue))
        if (-not ($outputPath + [IO.Path]::DirectorySeparatorChar).StartsWith($outputFull, [StringComparison]::OrdinalIgnoreCase)) {
            throw "CD level metadata output is outside the metadata directory: $outputValue"
        }
        if ([IO.Path]::GetExtension($outputPath).ToLowerInvariant() -ne '.json') {
            throw "CD level metadata output must be a .json file: $outputValue"
        }
        if ($outputs.ContainsKey($outputPath)) {
            throw "Duplicate CD level metadata output: $outputValue"
        }
        $outputs[$outputPath] = $true
        $seenExclusions = @{}
        foreach ($excludedDescriptor in $excludeDescriptors) {
            if ([IO.Path]::GetFileName($excludedDescriptor) -ne $excludedDescriptor -or
                [IO.Path]::GetExtension($excludedDescriptor) -notin @('.msn', '.mn2')) {
                throw "CD level metadata exclusion must be an MSN or MN2 leaf filename: $excludedDescriptor"
            }
            if ($seenExclusions.ContainsKey($excludedDescriptor)) {
                throw "Duplicate CD level metadata exclusion: $excludedDescriptor"
            }
            $seenExclusions[$excludedDescriptor] = $true
        }

        $resolved += [pscustomobject]@{
            Id = $id
            SourceDir = $sourceDir
            Discover = $discover
            ExcludeDescriptors = $excludeDescriptors
            Files = $resolvedFiles
            Descriptor = if ($descriptorMatches.Count -eq 1) { $descriptorMatches[0] } else { $null }
            OutputPath = $outputPath
        }
    }
    return $resolved
}
