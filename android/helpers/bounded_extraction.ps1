Set-StrictMode -Version Latest

function Resolve-BoundedPythonRuntime {
    param(
        [string]$RuntimePath,
        [string]$ExpectedSha256
    )

    $repoRoot = Split-Path (Split-Path $PSScriptRoot)
    $expectedVersion = '3.12.8'
    $provenance = 'explicit-sha256'
    if ([string]::IsNullOrWhiteSpace($RuntimePath) -and
        [string]::IsNullOrWhiteSpace($ExpectedSha256) -and
        (-not [string]::IsNullOrWhiteSpace($env:DXX_BOUNDED_PYTHON_RUNTIME) -or
        -not [string]::IsNullOrWhiteSpace($env:DXX_BOUNDED_PYTHON_SHA256))) {
        $RuntimePath = $env:DXX_BOUNDED_PYTHON_RUNTIME
        $ExpectedSha256 = $env:DXX_BOUNDED_PYTHON_SHA256
    }
    if ([string]::IsNullOrWhiteSpace($RuntimePath)) {
        if (-not [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
            throw 'A bounded Python SHA-256 requires an explicit runtime path'
        }
        if ($env:OS -ne 'Windows_NT') {
            throw 'No repository-pinned bounded Python runtime is available for this platform; supply an explicit runtime path and SHA-256'
        }

        . (Join-Path $PSScriptRoot 'verified_dependencies.ps1')
        $config = Read-DxxDependencyConfig -RepoRoot $repoRoot
        foreach ($key in @('PYTHON_EMBED_VERSION', 'PYTHON_EMBED_TREE_SHA256', 'PYTHON_ORACLE_DIR_NAME')) {
            if (-not $config.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($config[$key])) {
                throw "$key not found in tool_versions.conf"
            }
        }
        if ($config['PYTHON_EMBED_VERSION'] -cne $expectedVersion) {
            throw "Bounded Python policy requires version $expectedVersion"
        }
        $depBaseFile = Join-Path $repoRoot 'dependency_base.txt'
        if (-not (Test-Path -LiteralPath $depBaseFile -PathType Leaf)) {
            throw "dependency_base.txt not found at $depBaseFile"
        }
        $depBase = (Get-Content -LiteralPath $depBaseFile -First 1).Trim()
        $runtimeRoot = Join-Path $depBase $config['PYTHON_ORACLE_DIR_NAME']
        $verifiedRoot = Assert-DxxTreeSha256 -Path (Join-Path $runtimeRoot 'python') `
            -ExpectedSha256 $config['PYTHON_EMBED_TREE_SHA256'] -Label 'bounded Python runtime'
        $RuntimePath = Join-Path $verifiedRoot 'python.exe'
        $ExpectedSha256 = (Get-FileHash -LiteralPath $RuntimePath -Algorithm SHA256).Hash
        $provenance = 'repository-pinned-tree'
    } elseif ([string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        throw 'An explicit bounded Python runtime requires its SHA-256'
    }

    if ($ExpectedSha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw 'Invalid bounded Python SHA-256'
    }
    if (-not (Test-Path -LiteralPath $RuntimePath -PathType Leaf)) {
        throw "Bounded Python runtime not found at $RuntimePath"
    }
    $runtime = (Resolve-Path -LiteralPath $RuntimePath).Path
    $actualSha256 = (Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -cne $ExpectedSha256.ToLowerInvariant()) {
        throw "Bounded Python runtime SHA-256 mismatch expected=$($ExpectedSha256.ToLowerInvariant()) actual=$actualSha256"
    }

    $identityScript = 'import json, os, platform, sys; print(json.dumps({"executable": os.path.realpath(sys.executable), "version": platform.python_version()}))'
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $identityOutput = @(& $runtime -I -c $identityScript 2>&1)
        $identityExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldPreference
    }
    if ($identityExitCode -ne 0 -or $identityOutput.Count -ne 1) {
        throw 'Bounded Python runtime identity probe failed'
    }
    try {
        $identity = $identityOutput[0] | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw 'Bounded Python runtime returned an invalid identity'
    }
    if ([string]$identity.version -cne $expectedVersion) {
        throw "Bounded Python runtime must be version $expectedVersion"
    }
    $reportedRuntime = [IO.Path]::GetFullPath([string]$identity.executable)
    $comparison = if ($env:OS -eq 'Windows_NT') {
        [StringComparison]::OrdinalIgnoreCase
    } else {
        [StringComparison]::Ordinal
    }
    if (-not $reportedRuntime.Equals([IO.Path]::GetFullPath($runtime), $comparison)) {
        throw 'Bounded Python runtime executable identity mismatch'
    }

    return [pscustomobject][ordered]@{
        Path = $runtime
        Version = $expectedVersion
        Sha256 = $actualSha256
        Provenance = $provenance
    }
}

function Invoke-BoundedExtractor {
    param(
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$ArgumentList = @(),
        [int]$TimeoutSeconds = 300,
        [int]$MaxFiles = 4096,
        [long]$MaxFileBytes = 536870912,
        [long]$MaxTotalBytes = 2147483648,
        [int]$MaxDiagnosticBytes = 1048576,
        [string]$PythonRuntimePath,
        [string]$PythonRuntimeSha256
    )

    $python = Resolve-BoundedPythonRuntime -RuntimePath $PythonRuntimePath `
        -ExpectedSha256 $PythonRuntimeSha256

    $helper = Join-Path $PSScriptRoot 'run_bounded_extractor.py'
    $arguments = @(
        '-I',
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
        $output = & $python.Path @arguments 2>&1
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

function Get-ExtractionPathIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $item = Get-Item -LiteralPath $Path -Force
    if (-not $item.PSIsContainer) {
        return [pscustomobject][ordered]@{
            name = $Name.Replace('\', '/')
            kind = 'file'
            size = $item.Length
            sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }

    $root = $item.FullName.TrimEnd('\', '/')
    $records = @(Get-ChildItem -LiteralPath $root -File -Recurse | Sort-Object FullName | ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
            '{0}|{1}|{2}' -f $relative, $_.Length,
            (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        })
    $bytes = [Text.Encoding]::UTF8.GetBytes(($records -join "`n"))
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = ([BitConverter]::ToString($hasher.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    } finally {
        $hasher.Dispose()
    }
    return [pscustomobject][ordered]@{
        name = $Name.Replace('\', '/')
        kind = 'directory'
        files = $records.Count
        sha256 = $digest
    }
}

function New-ExtractionProvenance {
    param(
        [Parameter(Mandatory = $true)][string]$Policy,
        [Parameter(Mandatory = $true)][object[]]$Sources,
        [Parameter(Mandatory = $true)][object[]]$Tools
    )

    $helperIdentity = Get-ExtractionPathIdentity `
        -Path (Join-Path $PSScriptRoot 'bounded_extraction.ps1') -Name 'bounded_extraction.ps1'
    return [pscustomobject][ordered]@{
        schema = 1
        policy = $Policy
        sources = @($Sources)
        tools = @($Tools) + @($helperIdentity)
    }
}

function Write-ExtractionCompletionManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][object]$Provenance
    )

    $manifestPath = Join-Path $Directory '.extraction-complete.json'
    $files = @(Get-ChildItem -LiteralPath $Directory -File -Recurse |
            Where-Object { $_.FullName -ne $manifestPath } |
            Sort-Object FullName | ForEach-Object {
                [pscustomobject][ordered]@{
                    name = $_.FullName.Substring($Directory.Length).TrimStart('\', '/').Replace('\', '/')
                    size = $_.Length
                    sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            })
    [pscustomobject][ordered]@{
        provenance = $Provenance
        files = $files
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -NoNewline
}

function Get-CueReferencedFiles {
    param(
        [Parameter(Mandatory = $true)][string]$CuePath
    )

    $cue = Get-Item -LiteralPath $CuePath
    $directory = $cue.Directory.FullName
    $root = $directory.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $matches = [regex]::Matches(
        (Get-Content -LiteralPath $cue.FullName -Raw),
        '(?im)^\s*FILE\s+(?:"([^"]+)"|(\S+))\s+\S+'
    )
    if ($matches.Count -eq 0) { throw "CUE has no FILE directives: $($cue.Name)" }

    $files = @($cue)
    $seen = @{}
    foreach ($match in $matches) {
        $name = if ($match.Groups[1].Success) { $match.Groups[1].Value } else { $match.Groups[2].Value }
        $path = [IO.Path]::GetFullPath((Join-Path $directory $name))
        if (-not $path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
            throw "CUE FILE escapes its directory: $name"
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "CUE FILE not found: $name"
        }
        $key = $path.ToLowerInvariant()
        if (-not $seen.ContainsKey($key)) {
            $seen[$key] = $true
            $files += Get-Item -LiteralPath $path
        }
    }
    return @($files)
}

function Resolve-DiscExtractionSource {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [switch]$CueOnly
    )

    $cues = @(Get-ChildItem -LiteralPath $Directory -Filter '*.cue' -File | Sort-Object Name)
    $isos = @(Get-ChildItem -LiteralPath $Directory -Filter '*.iso' -File | Sort-Object Name)
    if ($CueOnly -and $isos.Count -gt 0) {
        throw "Legacy Mac extraction accepts one CUE source and no ISO descriptors in $Directory"
    }
    if ($cues.Count -eq 1 -and $isos.Count -eq 1) {
        $cueFiles = @(Get-CueReferencedFiles -CuePath $cues[0].FullName)
        $payloads = @($cueFiles | Where-Object { $_.FullName -ne $cues[0].FullName })
        if ($payloads.Count -eq 1 -and $payloads[0].FullName -eq $isos[0].FullName) {
            return [pscustomobject]@{ Primary = $isos[0]; Files = $cueFiles }
        }
        throw "CUE and ISO descriptors identify different source payloads in $Directory"
    }
    $descriptors = @($cues) + @($isos)
    if ($descriptors.Count -ne 1) {
        $kind = if ($CueOnly) { 'CUE' } else { 'CUE or ISO' }
        throw "Expected exactly one $kind descriptor in $Directory, found $($descriptors.Count)"
    }
    $primary = $descriptors[0]
    $files = if ($primary.Extension -ieq '.cue') {
        @(Get-CueReferencedFiles -CuePath $primary.FullName)
    } else {
        @($primary)
    }
    return [pscustomobject]@{ Primary = $primary; Files = $files }
}

function Test-ExtractionCompletionManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][object]$ExpectedProvenance
    )

    $manifestPath = Join-Path $Directory '.extraction-complete.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { return $false }
    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        if (-not $manifest.provenance -or
            ($manifest.provenance | ConvertTo-Json -Depth 8 -Compress) -cne
            ($ExpectedProvenance | ConvertTo-Json -Depth 8 -Compress)) {
            return $false
        }
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

function Test-ExtractionCompletionSources {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$ExpectedPolicy,
        [Parameter(Mandatory = $true)][object[]]$ExpectedSources,
        [object[]]$RequiredTools = @()
    )

    $manifestPath = Join-Path $Directory '.extraction-complete.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { return $false }
    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        if ($manifest.provenance.policy -cne $ExpectedPolicy -or
            (@($manifest.provenance.sources) | ConvertTo-Json -Depth 8 -Compress) -cne
            (@($ExpectedSources) | ConvertTo-Json -Depth 8 -Compress)) {
            return $false
        }
        foreach ($required in $RequiredTools) {
            $match = @($manifest.provenance.tools | Where-Object { $_.name -ceq $required.name })
            if ($match.Count -ne 1 -or
                ($match[0] | ConvertTo-Json -Depth 8 -Compress) -cne
                ($required | ConvertTo-Json -Depth 8 -Compress)) {
                return $false
            }
        }
        return Test-ExtractionCompletionManifest -Directory $Directory `
            -ExpectedProvenance $manifest.provenance
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

    Add-Type -AssemblyName System.IO.Compression
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
