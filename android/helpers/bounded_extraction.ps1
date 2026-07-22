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
