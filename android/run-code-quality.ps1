#!/usr/bin/env pwsh
# run-code-quality.ps1 -- Run all code quality checks.
# Tools: clang-format (C/C++), ktlint (Kotlin), PSScriptAnalyzer (PowerShell),
#        UTF-8 BOM lint, shellcheck (bash lint), shfmt (bash format),
#        cmake-format / cmake-lint (cheshirekow/cmakelang).
# Usage:
#   .\run-code-quality.ps1          # check only (exit 1 if issues)
#   .\run-code-quality.ps1 -Fix     # auto-format all supported languages
#   .\run-code-quality.ps1 -Fix -Paths path\to\file path\to\dir

param(
    [switch]$Fix,
    [string[]]$Paths
)

$ErrorActionPreference = "Continue"
$scriptDir = $PSScriptRoot
$repoRoot = Split-Path $scriptDir
$failed = @()
$lockDir = Join-Path $scriptDir "temp"
$lockFile = Join-Path $lockDir "run-code-quality.lock.json"
$summaryFile = Join-Path $lockDir "run-code-quality.summary.json"
$resolvedPaths = @()
$exitCode = 0

function Resolve-CodeQualityPaths {
    param(
        [string[]]$InputPaths
    )

    $results = @()
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
            Write-Warning "Skipping missing path: $inputPath"
            continue
        }

        $results += $item.FullName
    }

    return @($results | Sort-Object -Unique)
}

function Get-RepoRelativePath {
    param(
        [string]$FullName
    )

    return [System.IO.Path]::GetRelativePath($repoRoot, $FullName)
}

function Get-GitDirtyPaths {
    param(
        [string[]]$TargetPaths
    )

    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        return @()
    }

    $gitArgs = @('-C', $repoRoot, 'status', '--porcelain=v1', '--untracked-files=no')
    $relativeTargets = @()
    foreach ($targetPath in $TargetPaths) {
        $relativeTargets += Get-RepoRelativePath $targetPath
    }
    if ($relativeTargets.Count -gt 0) {
        $gitArgs += '--'
        $gitArgs += $relativeTargets
    }

    $lines = & $git.Source @gitArgs 2>$null
    if ($LASTEXITCODE -ne 0) {
        return @()
    }

    $dirty = @()
    foreach ($line in $lines) {
        if ([string]::IsNullOrWhiteSpace($line) -or $line.Length -lt 4) {
            continue
        }

        $pathText = $line.Substring(3).Trim()
        if ($pathText.Contains(' -> ')) {
            $pathText = ($pathText -split ' -> ', 2)[1]
        }

        $dirty += $pathText.Replace('/', '\')
    }

    return @($dirty | Sort-Object -Unique)
}

function Get-CodeQualityFiles {
    param(
        [string[]]$TargetPaths
    )

    $results = @()
    if ($TargetPaths.Count -gt 0) {
        foreach ($targetPath in $TargetPaths) {
            $item = Get-Item -LiteralPath $targetPath -ErrorAction SilentlyContinue
            if (-not $item) {
                continue
            }

            if ($item.PSIsContainer) {
                $results += Get-ChildItem -LiteralPath $item.FullName -File -Recurse -ErrorAction SilentlyContinue |
                    ForEach-Object { $_.FullName }
            } else {
                $results += $item.FullName
            }
        }

        return @($results | Sort-Object -Unique)
    }

    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        return @()
    }

    $trackedFiles = & $git.Source -C $repoRoot ls-files 2>$null
    if ($LASTEXITCODE -ne 0) {
        return @()
    }

    foreach ($trackedFile in $trackedFiles) {
        if ([string]::IsNullOrWhiteSpace($trackedFile)) {
            continue
        }

        $fullPath = Join-Path $repoRoot $trackedFile
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $results += [System.IO.Path]::GetFullPath($fullPath)
        }
    }

    return @($results | Sort-Object -Unique)
}

function Test-Utf8BomFile {
    param(
        [string]$Path
    )

    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
    } catch {
        Write-Warning "Skipping unreadable file: $Path"
        return $false
    }

    return $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
}

function Remove-Utf8Bom {
    param(
        [string]$Path
    )

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 3) {
        return $false
    }

    if (-not ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)) {
        return $false
    }

    $replacement = if ($bytes.Length -eq 3) { [byte[]]@() } else { [byte[]]$bytes[3..($bytes.Length - 1)] }
    [System.IO.File]::WriteAllBytes($Path, $replacement)
    return $true
}

function Invoke-Utf8BomLint {
    param(
        [string[]]$TargetPaths,
        [switch]$Fix
    )

    $files = Get-CodeQualityFiles $TargetPaths
    $offenders = @()
    $fixed = @()

    foreach ($file in $files) {
        if (-not (Test-Utf8BomFile $file)) {
            continue
        }

        $relativePath = Get-RepoRelativePath $file
        if ($Fix) {
            if (Remove-Utf8Bom $file) {
                $fixed += $relativePath
            } else {
                $offenders += $relativePath
            }
            continue
        }

        $offenders += $relativePath
    }

    if ($Fix) {
        if ($fixed.Count -gt 0) {
            Write-Host "Removed UTF-8 BOM from:"
            foreach ($fixedPath in $fixed) {
                Write-Host "  $fixedPath"
            }
        } else {
            Write-Host "No UTF-8 BOM files found"
        }

        if ($offenders.Count -eq 0) {
            return $true
        }

        Write-Host "Could not remove UTF-8 BOM from:"
        foreach ($offender in $offenders) {
            Write-Host "  $offender"
        }
        return $false
    }

    if ($offenders.Count -eq 0) {
        Write-Host "No UTF-8 BOM files found"
        return $true
    }

    Write-Host "UTF-8 BOM is not allowed in tracked or scoped files"
    foreach ($offender in $offenders) {
        Write-Host "  $offender"
    }
    return $false
}

function Write-CodeQualityLock {
    param(
        [string]$Stage
    )

    @{
        pid = $PID
        started = (Get-Date).ToString("s")
        fix = [bool]$Fix
        host = $Host.Name
        stage = $Stage
        paths = @($resolvedPaths | ForEach-Object { Get-RepoRelativePath $_ })
    } | ConvertTo-Json | Set-Content -LiteralPath $lockFile -Encoding utf8
}

function Write-CodeQualitySummary {
    param(
        [string]$Stage,
        [string[]]$PreDirty,
        [string[]]$PostDirty,
        [string[]]$CleanTransitions
    )

    @{
        pid = $PID
        finished = (Get-Date).ToString("s")
        fix = [bool]$Fix
        stage = $Stage
        paths = @($resolvedPaths | ForEach-Object { Get-RepoRelativePath $_ })
        failed = @($failed)
        preDirty = @($PreDirty)
        postDirty = @($PostDirty)
        cleanTransitions = @($CleanTransitions)
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryFile -Encoding utf8
}

function Test-ActiveProcess {
    param(
        [int]$ProcessId
    )

    if ($ProcessId -le 0) {
        return $false
    }

    $proc = Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -ErrorAction SilentlyContinue
    return $null -ne $proc
}

function Remove-CodeQualityLock {
    if (-not (Test-Path -LiteralPath $lockFile)) {
        return
    }

    $lockText = Get-Content -LiteralPath $lockFile -Raw -ErrorAction SilentlyContinue
    if (-not $lockText) {
        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
        return
    }

    $lockInfo = $null
    try {
        $lockInfo = $lockText | ConvertFrom-Json
    } catch {
        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
        return
    }

    if ($lockInfo.pid -eq $PID) {
        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Test-Path -LiteralPath $lockDir)) {
    New-Item -ItemType Directory -Path $lockDir | Out-Null
}

$resolvedPaths = Resolve-CodeQualityPaths $Paths
$toolParams = @{}
if ($resolvedPaths.Count -gt 0) {
    $toolParams.Paths = $resolvedPaths
}

$preDirtyPaths = Get-GitDirtyPaths $resolvedPaths
$postDirtyPaths = @()
$cleanTransitions = @()

if (Test-Path -LiteralPath $lockFile) {
    $lockText = Get-Content -LiteralPath $lockFile -Raw -ErrorAction SilentlyContinue
    $lockInfo = $null
    if ($lockText) {
        try {
            $lockInfo = $lockText | ConvertFrom-Json
        } catch {
            $lockInfo = $null
        }
    }

    if ($lockInfo -and (Test-ActiveProcess -ProcessId ([int]$lockInfo.pid))) {
        Write-Host "Another run-code-quality.ps1 is still active"
        Write-Host "Lock file: $lockFile"
        Write-Host "Active PID: $($lockInfo.pid)"
        Write-Host "Started: $($lockInfo.started)"
        Write-Host "Wait for it to finish or run android\\stop-stale-formatters.ps1 -Kill"
        exit 1
    }

    Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
}

Write-CodeQualityLock -Stage 'starting'

Write-Host "=== Code Quality Checks ==="
if ($resolvedPaths.Count -gt 0) {
    Write-Host "Scoped paths:"
    foreach ($resolvedPath in $resolvedPaths) {
        Write-Host "  $(Get-RepoRelativePath $resolvedPath)"
    }
    Write-Host ""
}
Write-Host ""

try {
    # --- clang-format ---
    Write-CodeQualityLock -Stage 'clang-format'
    Write-Host "--- C/C++ (clang-format) ---"
    if ($Fix) {
        & "$scriptDir\run-clang-format.ps1" @toolParams
    } else {
        $checkParams = @{ Check = $true } + $toolParams
        & "$scriptDir\run-clang-format.ps1" @checkParams
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "clang-format"
    }
    Write-Host ""

    # --- ktlint ---
    Write-CodeQualityLock -Stage 'ktlint'
    Write-Host "--- Kotlin (ktlint) ---"
    if ($Fix) {
        & "$scriptDir\run-ktlint.ps1" @toolParams
    } else {
        $checkParams = @{ Check = $true } + $toolParams
        & "$scriptDir\run-ktlint.ps1" @checkParams
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "ktlint"
    }
    Write-Host ""

    # --- PSScriptAnalyzer ---
    Write-CodeQualityLock -Stage 'psscriptanalyzer'
    Write-Host "--- PowerShell (PSScriptAnalyzer) ---"
    if ($Fix) {
        & "$scriptDir\run-psscriptanalyzer.ps1" @toolParams
    } else {
        $checkParams = @{ Check = $true } + $toolParams
        & "$scriptDir\run-psscriptanalyzer.ps1" @checkParams
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "psscriptanalyzer"
    }
    Write-Host ""

    # --- UTF-8 BOM lint ---
    Write-CodeQualityLock -Stage 'utf8-bom'
    Write-Host "--- UTF-8 BOM lint ---"
    if (-not (Invoke-Utf8BomLint -TargetPaths $resolvedPaths -Fix:$Fix)) {
        $failed += "utf8-bom"
    }
    Write-Host ""

    # --- shellcheck ---
    Write-CodeQualityLock -Stage 'shellcheck'
    Write-Host "--- Bash lint (shellcheck) ---"
    # shellcheck has no auto-fix; always runs in report mode
    & "$scriptDir\run-shellcheck.ps1" @toolParams
    if ($LASTEXITCODE -ne 0) {
        $failed += "shellcheck"
    }
    Write-Host ""

    # --- shfmt ---
    Write-CodeQualityLock -Stage 'shfmt'
    Write-Host "--- Bash format (shfmt) ---"
    if ($Fix) {
        & "$scriptDir\run-shfmt.ps1" @toolParams
    } else {
        $checkParams = @{ Check = $true } + $toolParams
        & "$scriptDir\run-shfmt.ps1" @checkParams
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "shfmt"
    }
    Write-Host ""

    # --- cmake-format ---
    Write-CodeQualityLock -Stage 'cmake-format'
    Write-Host "--- CMake format (cmake-format) ---"
    if ($Fix) {
        & "$scriptDir\run-cmake-format.ps1" @toolParams
    } else {
        $checkParams = @{ Check = $true } + $toolParams
        & "$scriptDir\run-cmake-format.ps1" @checkParams
    }
    if ($LASTEXITCODE -ne 0) {
        $failed += "cmake-format"
    }
    Write-Host ""

    # --- cmake-lint ---
    Write-CodeQualityLock -Stage 'cmake-lint'
    Write-Host "--- CMake lint (cmake-lint) ---"
    # cmake-lint has no auto-fix; always runs in report mode
    & "$scriptDir\run-cmake-lint.ps1" @toolParams
    if ($LASTEXITCODE -ne 0) {
        $failed += "cmake-lint"
    }
    Write-Host ""

    # --- Summary ---
    Write-Host "=== Summary ==="
    if ($failed.Count -eq 0) {
        Write-Host "All checks passed"
    } else {
        Write-Host "Failed checks: $($failed -join ', ')"
        if (-not $Fix) {
            Write-Host "Run with --fix to auto-format and strip UTF-8 BOMs"
        }
        $exitCode = 1
    }
} finally {
    $postDirtyPaths = Get-GitDirtyPaths $resolvedPaths
    $cleanTransitions = @($preDirtyPaths | Where-Object { $postDirtyPaths -notcontains $_ })
    Write-CodeQualitySummary -Stage 'finished' -PreDirty $preDirtyPaths -PostDirty $postDirtyPaths -CleanTransitions $cleanTransitions
    if ($Fix -and $cleanTransitions.Count -gt 0) {
        Write-Host ""
        Write-Host "Review: modified files became clean during this cleanup pass"
        Write-Host "These files matched HEAD after formatting or were overwritten externally"
        foreach ($cleanPath in $cleanTransitions) {
            Write-Host "  $cleanPath"
        }
    }
    Remove-CodeQualityLock
}

exit $exitCode
