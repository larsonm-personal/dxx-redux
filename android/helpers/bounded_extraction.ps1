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
    New-Item -ItemType Directory -Force -Path $DestinationPath | Out-Null
    $root = [IO.Path]::GetFullPath($DestinationPath).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $total = 0L
    $count = 0
    try {
        foreach ($entry in $archive.Entries) {
            $count++
            if ($count -gt $MaxEntries) { throw "Archive exceeds $MaxEntries entries" }
            if ([string]::IsNullOrWhiteSpace($entry.Name)) { continue }
            if ($entry.Length -gt $MaxEntryBytes) { throw "$($entry.FullName) exceeds $MaxEntryBytes bytes" }
            if ($entry.CompressedLength -gt 0) {
                $quotient = [math]::Floor($entry.Length / $entry.CompressedLength)
                $remainder = $entry.Length % $entry.CompressedLength
                if ($quotient -gt $MaxRatio -or ($quotient -eq $MaxRatio -and $remainder -gt 0)) {
                    throw "$($entry.FullName) exceeds the ${MaxRatio}:1 expansion ratio"
                }
            }
            if ($entry.Length -gt $MaxTotalBytes - $total) { throw "Archive exceeds $MaxTotalBytes bytes" }
            $total += $entry.Length
        }

        $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($root))
        if ($drive.AvailableFreeSpace -lt $total + $FreeSpaceHeadroomBytes) {
            throw 'Insufficient free space for bounded ZIP extraction'
        }

        $actualTotal = 0L
        foreach ($entry in $archive.Entries) {
            if ([string]::IsNullOrWhiteSpace($entry.Name)) { continue }
            $target = [IO.Path]::GetFullPath((Join-Path $DestinationPath $entry.FullName))
            if (-not $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Archive entry escapes destination: $($entry.FullName)"
            }
            $targetDir = Split-Path $target -Parent
            if ($targetDir) { New-Item -ItemType Directory -Force -Path $targetDir | Out-Null }
            $input = $entry.Open()
            $output = [IO.File]::Open($target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
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
            } finally {
                $output.Dispose()
                $input.Dispose()
            }
        }
    } catch {
        $archive.Dispose()
        Remove-Item -LiteralPath $DestinationPath -Recurse -Force -ErrorAction SilentlyContinue
        throw
    } finally {
        $archive.Dispose()
    }
}
