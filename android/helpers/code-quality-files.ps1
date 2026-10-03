# Shared file selection for code quality wrappers

function Get-CodeQualityScriptPaths {
    param([string[]]$InputPaths, [string[]]$RemainingPaths, [bool]$ExplicitScope)
    $paths = @($InputPaths | Where-Object { $null -ne $_ }) + @($RemainingPaths)
    if ($ExplicitScope -and $paths.Count -eq 0) { throw 'Explicit code-quality scope resolved to no paths' }
    foreach ($path in $paths) {
        if ([string]::IsNullOrWhiteSpace($path)) { throw 'An explicit code-quality path cannot be empty' }
    }
    return $paths
}

function Get-CodeQualityPython {
    $names = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) { @('python3', 'python') } else { @('python', 'python3') }
    foreach ($name in $names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    throw 'Python 3.11+ is required; put python3 or python in PATH'
}

function Get-CodeQualityNode {
    param([string]$RepoRoot)
    $command = Get-Command node -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $cache = Join-Path $RepoRoot 'android/temp/code-quality/node-path.txt'
    if (Test-Path -LiteralPath $cache -PathType Leaf) {
        $candidate = (Get-Content -LiteralPath $cache -Raw).Trim()
        if ([IO.Path]::IsPathRooted($candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $candidate }
    }
    throw 'Node.js 20+ is required; put node/npm in PATH and run android/run-code-quality.ps1 -InstallTools'
}

function Get-CodeQualityToolVersion {
    param([string]$RepoRoot, [string]$Name)
    $setting = Get-Content -LiteralPath (Join-Path $RepoRoot 'android/get_deps/tool_versions.conf') |
        Where-Object { $_ -match "^$([regex]::Escape($Name))=" } | Select-Object -First 1
    if (-not $setting) { throw "Missing code quality version: $Name" }
    return ($setting -split '=', 2)[1].Trim()
}

function Invoke-CodeQualityGit {
    param([string]$RepoRoot, [string[]]$Arguments)
    $output = & git -C $RepoRoot @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Git file selection failed: git $($Arguments -join ' ')" }
    return @((($output -join "`n") -split "`0") | Where-Object { $_ })
}

function Get-CodeQualityRepositoryFiles {
    param([string]$RepoRoot, [string]$BaseRef, [switch]$Changed)

    if (-not $BaseRef) { $BaseRef = $env:DXX_CODE_QUALITY_BASE_REF }
    if (-not $BaseRef) {
        foreach ($candidate in @('upstream/main', 'main', 'origin/main')) {
            & git -C $RepoRoot rev-parse --verify --quiet $candidate 2>$null | Out-Null
            if ($LASTEXITCODE -eq 0) { $BaseRef = $candidate; break }
        }
    }
    if (-not $BaseRef) { throw 'No upstream baseline found; use -BaseRef or DXX_CODE_QUALITY_BASE_REF with a local upstream ref' }
    $base = @(Invoke-CodeQualityGit -RepoRoot $RepoRoot -Arguments @('merge-base', 'HEAD', $BaseRef))[0]
    $inherited = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in @(Invoke-CodeQualityGit -RepoRoot $RepoRoot -Arguments @('ls-tree', '-r', '--name-only', '-z', $base))) {
        [void]$inherited.Add($name)
    }
    # Compare the index as well as committed history so staged additions are included
    $added = @(Invoke-CodeQualityGit -RepoRoot $RepoRoot -Arguments @('diff', '--cached', '--name-only', '--diff-filter=A', '-z', $base))
    $untracked = @(Invoke-CodeQualityGit -RepoRoot $RepoRoot -Arguments @('ls-files', '--others', '--exclude-standard', '-z'))
    $names = @($added + $untracked | Sort-Object -Unique)
    if ($Changed) {
        $dirty = @(Invoke-CodeQualityGit -RepoRoot $RepoRoot -Arguments @('diff', 'HEAD', '--name-only', '-z')) + $untracked
        $names = @($names | Where-Object { $dirty -contains $_ })
    }
    foreach ($name in $names) {
        # An inherited file removed from the index can reappear as untracked
        if ($inherited.Contains($name)) { continue }
        # Keep upstream code, build output, dependency copies and generated reports untouched
        if ($name -match '^(d1|d2)/|(^|/)(build[^/]*|\.cxx|\.gradle|\.git|temp|node_modules|target|__pycache__)/' -or
            $name -match '/(SDL_androidaudio\.[ch]|SDL_config_android\.h|BuildInfo\.kt)$') { continue }
        $item = Get-Item -LiteralPath (Join-Path $RepoRoot $name) -Force -ErrorAction SilentlyContinue
        if ($item -and -not $item.PSIsContainer -and -not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { $item }
    }
}

function Get-CodeQualityScopedFiles {
    param(
        [string]$RepoRoot,
        [string]$RootPath,
        [string[]]$InputPaths,
        [string[]]$ValidExtensions,
        [string]$ExcludePattern
    )

    $comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) { [StringComparison]::Ordinal } else { [StringComparison]::OrdinalIgnoreCase }
    $rootPrefix = [IO.Path]::GetFullPath($RootPath).TrimEnd([char[]]"\/") + [IO.Path]::DirectorySeparatorChar
    $results = @(Get-CodeQualityRepositoryFiles -RepoRoot $RepoRoot)
    $results = @(Select-CodeQualityInputFiles -RepoRoot $RepoRoot -AllFiles $results -InputPaths $InputPaths)

    return @($results | Where-Object {
            $_.FullName.StartsWith($rootPrefix, $comparison) -and
            ($ValidExtensions -contains $_.Extension.ToLowerInvariant()) -and
            (-not $ExcludePattern -or $_.FullName -notmatch $ExcludePattern)
        } | Sort-Object FullName -Unique)
}

function Select-CodeQualityInputFiles {
    param([string]$RepoRoot, [System.IO.FileInfo[]]$AllFiles, [string[]]$InputPaths)
    if ($null -eq $InputPaths -or $InputPaths.Count -eq 0) { return $AllFiles }
    $comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Unix) { [StringComparison]::Ordinal } else { [StringComparison]::OrdinalIgnoreCase }
    $comparer = if ($comparison -eq [StringComparison]::Ordinal) { [StringComparer]::Ordinal } else { [StringComparer]::OrdinalIgnoreCase }
    $leafPaths = [Collections.Generic.HashSet[string]]::new($comparer)
    $directoryPrefixes = @()
    foreach ($p in $InputPaths) {
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $candidate = $p
        if (-not [System.IO.Path]::IsPathRooted($candidate)) {
            $candidate = Join-Path $repoRoot $candidate
        }
        $item = Get-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue
        if ($item) {
            if ($item.PSIsContainer) {
                $directoryPrefixes += $item.FullName.TrimEnd([char[]]"\/") + [IO.Path]::DirectorySeparatorChar
            } else { [void]$leafPaths.Add($item.FullName) }
        }
    }
    if ($leafPaths.Count -eq 0 -and $directoryPrefixes.Count -eq 0) { return @() }
    return @($AllFiles | Where-Object {
            $f = $_
            if ($leafPaths.Contains($f.FullName)) { return $true }
            foreach ($prefix in $directoryPrefixes) {
                if ($f.FullName.StartsWith($prefix, $comparison)) { return $true }
            }
            return $false
        })
}

function Get-CodeQualityCmakeFiles {
    param([string]$RepoRoot, [string[]]$InputPaths)

    $all = @(Get-CodeQualityRepositoryFiles -RepoRoot $RepoRoot | Where-Object { $_.Name -eq 'CMakeLists.txt' -or $_.Extension -eq '.cmake' })
    Select-CodeQualityInputFiles -RepoRoot $RepoRoot -AllFiles @($all | Sort-Object FullName -Unique) -InputPaths $InputPaths
}
