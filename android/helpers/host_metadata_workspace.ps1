# Release reproducible payloads while retaining metadata JSON and diagnostic logs
function Remove-GuidebotTestPayloads {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string]$OutputText
    )
    $tempRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'android/temp')).TrimEnd('\', '/')
    $separator = [IO.Path]::DirectorySeparatorChar
    $comparison = if ($separator -eq '\') { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    $plainText = $OutputText -replace '\x1b\[[0-9;]*m', ''
    foreach ($match in [regex]::Matches($plainText, '(?m)^\[\d{2}:\d{2}:\d{2}\] Output: ([^\r\n]+)')) {
        $runRoot = [IO.Path]::GetFullPath($match.Groups[1].Value.Trim()).TrimEnd('\', '/')
        if (-not $runRoot.StartsWith($tempRoot + $separator, $comparison) -or
            -not (Test-Path -LiteralPath (Join-Path $runRoot 'summary.json') -PathType Leaf)) {
            throw "Refusing to release an unrecognized route test workspace: $runRoot"
        }
        $ancestor = $runRoot
        while ($ancestor.Length -ge $tempRoot.Length) {
            if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing to release a linked route workspace: $ancestor"
            }
            $ancestor = [IO.Path]::GetDirectoryName($ancestor)
        }
        Remove-HostMetadataPayloads -RunRoot $runRoot
        $engine = Join-Path $runRoot 'engine'
        if (Test-Path -LiteralPath $engine -PathType Container) {
            if ((Get-Item -LiteralPath $engine -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing to release a linked route engine: $engine"
            }
            # Keep engine hashes and settings; only successful tests release executable copies
            Get-ChildItem -LiteralPath $engine -File | Where-Object Extension -in @('.exe', '.dll', '.pdb') |
                ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
        }
    }
}

function Remove-HostMetadataPayloads {
    param(
        [Parameter(Mandatory)][string]$RunRoot,
        [string[]]$Paths
    )

    $root = [IO.Path]::GetFullPath($RunRoot)
    $parents = @('raw', 'stages' | ForEach-Object { Join-Path $root $_ })
    foreach ($parent in @($root) + $parents) {
        if ((Test-Path -LiteralPath $parent) -and
            ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to clean through a linked workspace: $parent"
        }
    }
    if (-not $Paths) {
        $Paths = @($parents | Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
                ForEach-Object { Get-ChildItem -LiteralPath $_ -Directory | ForEach-Object FullName })
    }
    foreach ($path in $Paths) {
        $full = [IO.Path]::GetFullPath($path).TrimEnd([char[]]@('\', '/'))
        if ([IO.Path]::GetDirectoryName($full) -notin $parents) {
            throw "Refusing to clean payload outside the run's raw/stages directories: $full"
        }
        if (-not (Test-Path -LiteralPath $full)) { continue }
        $item = Get-Item -LiteralPath $full -Force
        if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to clean a non-directory or linked payload: $full"
        }
        try {
            Remove-Item -LiteralPath $full -Recurse -Force -ErrorAction Stop
        } catch {
            Write-Warning "Could not release metadata payload ${full}: $($_.Exception.Message)"
        }
    }
}

function Write-HostMetadataWorkerLog {
    [CmdletBinding()]
    param([string]$Path, [AllowEmptyString()][string]$Text)

    try {
        Write-Utf8NoBomTextAtomically -Path $Path -Text $Text
    } catch {
        Write-Warning "Could not save metadata worker log ${Path}: $($_.Exception.Message)"
        if ($Text) { Write-Host $Text }
    }
}
