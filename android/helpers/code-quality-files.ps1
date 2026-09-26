# Shared file selection for code quality wrappers

function Get-CodeQualityScopedFiles {
    param(
        [string]$RepoRoot,
        [string]$RootPath,
        [string[]]$InputPaths,
        [string[]]$ValidExtensions,
        [string]$ExcludePattern
    )

    $rootPrefix = [IO.Path]::GetFullPath($RootPath).TrimEnd([char[]]"\/") + [IO.Path]::DirectorySeparatorChar
    $results = @()
    if ($InputPaths -and $InputPaths.Count -gt 0) {
        foreach ($inputPath in $InputPaths) {
            if ([string]::IsNullOrWhiteSpace($inputPath)) {
                continue
            }

            $candidate = $inputPath
            if (-not [System.IO.Path]::IsPathRooted($candidate)) {
                $candidate = Join-Path $repoRoot $candidate
            }

            $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
            if (-not $item) {
                continue
            }

            if ($item.PSIsContainer) {
                $results += Get-ChildItem -LiteralPath $item.FullName -Recurse -File
            } else {
                $results += $item
            }
        }
    } else {
        $results = Get-ChildItem -Path $RootPath -Recurse -File
    }

    return @($results | Where-Object {
            $_.FullName.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
            ($ValidExtensions -contains $_.Extension.ToLowerInvariant()) -and
            (-not $ExcludePattern -or $_.FullName -notmatch $ExcludePattern)
        } | Sort-Object FullName -Unique)
}

function Select-CodeQualityInputFiles {
    param([string]$RepoRoot, [System.IO.FileInfo[]]$AllFiles, [string[]]$InputPaths)
    if (-not $InputPaths -or $InputPaths.Count -eq 0) { return $AllFiles }
    $resolvedInputs = @()
    foreach ($p in $InputPaths) {
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $candidate = $p
        if (-not [System.IO.Path]::IsPathRooted($candidate)) {
            $candidate = Join-Path $repoRoot $candidate
        }
        $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
        if ($item) { $resolvedInputs += $item }
    }
    if ($resolvedInputs.Count -eq 0) { return @() }
    return @($AllFiles | Where-Object {
            $f = $_
            foreach ($r in $resolvedInputs) {
                if ($r.PSIsContainer) {
                    if ($f.FullName.StartsWith(($r.FullName.TrimEnd([char[]]"\/") + [IO.Path]::DirectorySeparatorChar), [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
                } elseif ($f.FullName -ieq $r.FullName) { return $true }
            }
            return $false
        })
}

function Get-CodeQualityCmakeFiles {
    param([string]$RepoRoot, [string[]]$InputPaths)

    $inScopeGlobs = @(
        "android\app\src\main\cpp\CMakeLists.txt",
        "android\app\src\main\cpp\extract\CMakeLists.txt",
        "android\tests\CMakeLists.txt",
        "cmake\*.cmake",
        "android\tools\etc2tool\CMakeLists.txt"
    )
    $all = foreach ($glob in $inScopeGlobs) {
        Get-ChildItem -Path (Join-Path $RepoRoot $glob) -File -ErrorAction SilentlyContinue
    }
    Select-CodeQualityInputFiles -RepoRoot $RepoRoot -AllFiles @($all | Sort-Object FullName -Unique) -InputPaths $InputPaths
}
