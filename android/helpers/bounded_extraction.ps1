Set-StrictMode -Version Latest

function Invoke-BoundedExtractor {
    param(
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [int]$TimeoutSeconds = 300,
        [int]$MaxFiles = 4096,
        [long]$MaxFileBytes = 536870912,
        [long]$MaxTotalBytes = 2147483648,
        [int]$MaxDiagnosticBytes = 1048576
    )

    $python = Get-Command python -ErrorAction SilentlyContinue
    $launcherArgs = @()
    if (-not $python) {
        $python = Get-Command py -ErrorAction SilentlyContinue
        $launcherArgs = @('-3')
    }
    if (-not $python) { throw 'Python 3 is required for bounded child execution' }

    $helper = Join-Path $PSScriptRoot 'run_bounded_extractor.py'
    $arguments = $launcherArgs + @(
        $helper,
        '--output-dir', $OutputDirectory,
        '--timeout-seconds', $TimeoutSeconds,
        '--max-files', $MaxFiles,
        '--max-file-bytes', $MaxFileBytes,
        '--max-total-bytes', $MaxTotalBytes,
        '--max-diagnostic-bytes', $MaxDiagnosticBytes,
        '--', $FilePath
    ) + $ArgumentList

    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $python.Source @arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldPreference
    }
    [pscustomobject]@{ Output = @($output); ExitCode = $exitCode }
}

function Publish-ExtractionDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$StagingDirectory,
        [Parameter(Mandatory = $true)][string]$DestinationDirectory
    )

    $staging = [IO.Path]::GetFullPath($StagingDirectory)
    $destination = [IO.Path]::GetFullPath($DestinationDirectory)
    $parent = [IO.Path]::GetDirectoryName($destination)
    if (-not (Test-Path -LiteralPath $staging -PathType Container) -or
        [IO.Path]::GetDirectoryName($staging) -ne $parent) {
        throw 'Extraction staging and destination directories must be existing siblings'
    }

    $backup = "$destination.rollback-$([Guid]::NewGuid().ToString('N'))"
    $hadDestination = Test-Path -LiteralPath $destination
    try {
        if ($hadDestination) {
            Move-Item -LiteralPath $destination -Destination $backup
        }
        Move-Item -LiteralPath $staging -Destination $destination
    } catch {
        if (Test-Path -LiteralPath $destination) {
            Remove-Item -LiteralPath $destination -Recurse -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $backup) {
            Move-Item -LiteralPath $backup -Destination $destination
        }
        throw
    }
    if (Test-Path -LiteralPath $backup) {
        Remove-Item -LiteralPath $backup -Recurse -Force
    }
}

function Test-ExtractionCompletionManifest {
    param([Parameter(Mandatory = $true)][string]$Directory)

    $manifestPath = Join-Path $Directory '.extraction-complete.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { return $false }
    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $expected = @{}
        foreach ($file in @($manifest.files)) {
            $name = ([string]$file.name).Replace('\', '/').ToLowerInvariant()
            if (-not $name -or $expected.ContainsKey($name)) { return $false }
            $expected[$name] = $file
        }
        $actual = @(Get-ChildItem -LiteralPath $Directory -File -Recurse |
                Where-Object { $_.FullName -ne $manifestPath })
        if ($actual.Count -ne $expected.Count) { return $false }
        foreach ($file in $actual) {
            $name = $file.FullName.Substring($Directory.Length).TrimStart('\', '/').Replace('\', '/').ToLowerInvariant()
            if (-not $expected.ContainsKey($name) -or
                $file.Length -ne [long]$expected[$name].size -or
                (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne [string]$expected[$name].sha256) {
                return $false
            }
        }
        return $true
    } catch {
        return $false
    }
}

function Expand-BoundedZipArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [int]$MaxEntries = 4096,
        [long]$MaxEntryBytes = 536870912,
        [long]$MaxTotalBytes = 2147483648,
        [long]$MaxRatio = 1000,
        [int]$TimeoutSeconds = 300,
        [long]$FreeSpaceHeadroomBytes = 52428800
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $comparison = if ($env:OS -eq 'Windows_NT') {
        [StringComparison]::OrdinalIgnoreCase
    } else {
        [StringComparison]::Ordinal
    }
    $comparer = if ($env:OS -eq 'Windows_NT') {
        [StringComparer]::OrdinalIgnoreCase
    } else {
        [StringComparer]::Ordinal
    }
    $trimSeparators = [char[]]@([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $rootPath = [IO.Path]::GetFullPath($DestinationPath).TrimEnd($trimSeparators)
    $root = $rootPath + [IO.Path]::DirectorySeparatorChar
    if (Test-Path -LiteralPath $rootPath) {
        $rootItem = Get-Item -LiteralPath $rootPath -Force
        if (-not $rootItem.PSIsContainer -or
            ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'ZIP destination must be a regular directory'
        }
        if (Get-ChildItem -LiteralPath $rootPath -Force | Select-Object -First 1) {
            throw 'ZIP destination must be empty'
        }
    }
    $archive = $null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $total = 0L
    $count = 0
    $plans = [Collections.Generic.List[object]]::new()
    $targets = [Collections.Generic.Dictionary[string, bool]]::new($comparer)
    try {
        $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
        foreach ($entry in $archive.Entries) {
            $count++
            if ($count -gt $MaxEntries) { throw "Archive exceeds $MaxEntries entries" }
            $name = $entry.FullName
            if ([string]::IsNullOrWhiteSpace($name) -or $name.IndexOf([char]0) -ge 0 -or
                $name.Contains('\') -or $name.StartsWith('/') -or
                $name -match '^[A-Za-z]:') {
                throw "Archive entry has an unsafe name: $name"
            }
            $isDirectory = $name.EndsWith('/')
            $components = @($name.Split('/'))
            if ($isDirectory) { $components = @($components[0..($components.Count - 2)]) }
            if ($components.Count -eq 0) { throw "Archive entry has an empty path: $name" }
            foreach ($component in $components) {
                if ([string]::IsNullOrEmpty($component) -or $component -in @('.', '..') -or
                    $component.EndsWith('.') -or $component.EndsWith(' ') -or
                    $component.IndexOfAny([char[]]'<>:"|?*') -ge 0) {
                    throw "Archive entry has an unsafe component: $name"
                }
                foreach ($character in $component.ToCharArray()) {
                    if ([int]$character -lt 32) { throw "Archive entry has a control character: $name" }
                }
                $deviceName = $component.Split('.')[0]
                if ($deviceName -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$') {
                    throw "Archive entry uses a reserved device name: $name"
                }
            }
            $relative = [string]::Join([IO.Path]::DirectorySeparatorChar, $components)
            $target = [IO.Path]::GetFullPath((Join-Path $rootPath $relative))
            if (-not $target.StartsWith($root, $comparison)) {
                throw "Archive entry escapes destination: $name"
            }
            $targetKey = $target.Normalize([Text.NormalizationForm]::FormC)
            if ($targets.ContainsKey($targetKey)) {
                throw "Archive contains a duplicate destination: $name"
            }
            $targets.Add($targetKey, $isDirectory)
            $plans.Add([pscustomobject]@{
                    Entry = $entry
                    Name = $name
                    Target = $target
                    IsDirectory = $isDirectory
                })
            if ($isDirectory) {
                if ($entry.Length -ne 0) { throw "Archive directory contains file data: $name" }
                continue
            }
            if ($entry.Length -gt $MaxEntryBytes) { throw "$name exceeds $MaxEntryBytes bytes" }
            if ($entry.CompressedLength -gt 0) {
                $quotient = [math]::Floor($entry.Length / $entry.CompressedLength)
                $remainder = $entry.Length % $entry.CompressedLength
                if ($quotient -gt $MaxRatio -or ($quotient -eq $MaxRatio -and $remainder -gt 0)) {
                    throw "$name exceeds the ${MaxRatio}:1 expansion ratio"
                }
            }
            if ($entry.Length -gt $MaxTotalBytes - $total) { throw "Archive exceeds $MaxTotalBytes bytes" }
            $total += $entry.Length
        }

        foreach ($plan in $plans) {
            $parent = [IO.Path]::GetDirectoryName($plan.Target)
            while ($parent -and $parent.StartsWith($root, $comparison)) {
                $parentKey = $parent.Normalize([Text.NormalizationForm]::FormC)
                if ($targets.ContainsKey($parentKey) -and -not $targets[$parentKey]) {
                    throw "Archive file conflicts with a child entry: $($plan.Name)"
                }
                $parent = [IO.Path]::GetDirectoryName($parent)
            }
        }

        $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($root))
        if ($drive.AvailableFreeSpace -lt $total + $FreeSpaceHeadroomBytes) {
            throw 'Insufficient free space for bounded ZIP extraction'
        }

        if (-not (Test-Path -LiteralPath $rootPath)) {
            New-Item -ItemType Directory -Path $rootPath | Out-Null
        }
        $actualTotal = 0L
        foreach ($plan in $plans) {
            $targetDir = if ($plan.IsDirectory) {
                $plan.Target
            } else {
                [IO.Path]::GetDirectoryName($plan.Target)
            }
            $relativeDir = $targetDir.Substring($rootPath.Length).TrimStart($trimSeparators)
            $currentDir = $rootPath
            foreach ($component in @($relativeDir.Split($trimSeparators, [StringSplitOptions]::RemoveEmptyEntries))) {
                $currentDir = Join-Path $currentDir $component
                if (Test-Path -LiteralPath $currentDir) {
                    $item = Get-Item -LiteralPath $currentDir -Force
                    if (-not $item.PSIsContainer -or
                        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                        throw "ZIP extraction encountered an unsafe directory: $currentDir"
                    }
                } else {
                    New-Item -ItemType Directory -Path $currentDir | Out-Null
                }
            }
            if ($plan.IsDirectory) { continue }

            $input = $plan.Entry.Open()
            $output = [IO.File]::Open($plan.Target, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            try {
                $buffer = New-Object byte[] 65536
                $entryTotal = 0L
                while (($read = $input.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    $entryTotal += $read
                    $actualTotal += $read
                    if ($entryTotal -gt $MaxEntryBytes -or $actualTotal -gt $MaxTotalBytes) { throw 'ZIP output budget exceeded' }
                    if ($watch.Elapsed.TotalSeconds -gt $TimeoutSeconds) { throw "ZIP extraction exceeded $TimeoutSeconds seconds" }
                    if (($actualTotal % 8388608) -lt $read -and $drive.AvailableFreeSpace -lt $FreeSpaceHeadroomBytes) {
                        throw 'ZIP extraction exhausted free-space headroom'
                    }
                    $output.Write($buffer, 0, $read)
                }
                if ($entryTotal -ne $plan.Entry.Length) {
                    throw "ZIP entry length mismatch: $($plan.Name)"
                }
            } finally {
                $output.Dispose()
                $input.Dispose()
            }
        }
    } catch {
        Remove-Item -LiteralPath $DestinationPath -Recurse -Force -ErrorAction SilentlyContinue
        throw
    } finally {
        if ($archive) { $archive.Dispose() }
    }
}
