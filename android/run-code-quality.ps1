#!/usr/bin/env pwsh
# run-code-quality.ps1 -- Run all code quality checks.
# Tools: clang-format (C/C++/Java/C#), ktlint (Kotlin), PSScriptAnalyzer (PowerShell),
#        UTF-8 BOM lint, shellcheck (bash lint), shfmt (bash format),
#        cmake-format / cmake-lint, Ruff (Python), rustfmt (Rust), Prettier (text).
# Only files added since the upstream merge base are eligible, including with -Paths.
#   .\run-code-quality.ps1 -List   # preview eligible files
#   .\run-code-quality.ps1 -Fix -Changed -Tools clang-format,ruff
#   .\run-code-quality.ps1 -InstallTools # install extra pinned tools, then check
# Usage:
#   .\run-code-quality.ps1          # check only (exit 1 if issues)
#   .\run-code-quality.ps1 -Fix     # auto-format all supported languages
#   .\run-code-quality.ps1 -Fix -Paths path\to\file path\to\dir

[CmdletBinding(PositionalBinding = $false)]
param(
    [switch]$Fix,
    [string[]]$Paths,
    [string]$BaseRef,
    [switch]$Changed,
    [switch]$List,
    [switch]$InstallTools,
    [ValidateSet('clang-format', 'ktlint', 'psscriptanalyzer', 'utf8-bom', 'shellcheck', 'shfmt', 'cmake-format', 'cmake-lint', 'ruff', 'rustfmt', 'prettier')]
    [string[]]$Tools,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingPaths
)

# PowerShell script calls bind only the first space-separated value to a named
# array parameter; preserve remaining positional paths used by the documented CLI
$explicitScope = $PSBoundParameters.ContainsKey('Paths') -or $RemainingPaths.Count -gt 0
$Paths = if ($explicitScope) { @($Paths | Where-Object { $null -ne $_ }) + @($RemainingPaths | Where-Object { $null -ne $_ }) } else { @() }
$ErrorActionPreference = "Continue"
$scriptDir = $PSScriptRoot
$helpersDir = Join-Path $scriptDir "helpers"
$repoRoot = Split-Path $scriptDir
$failed = @()
$lockDir = Join-Path $scriptDir "temp"
$lockFile = Join-Path $lockDir "run-code-quality.lock.json"
$summaryFile = Join-Path $lockDir "run-code-quality.summary.json"
$resolvedPaths = @()
$exitCode = 0
. (Join-Path $helpersDir "powershell_compat.ps1")
. (Join-Path $helpersDir "code-quality-files.ps1")

function Resolve-CodeQualityPaths {
    param(
        [string[]]$InputPaths
    )

    $results = @()
    foreach ($inputPath in $InputPaths) {
        if ([string]::IsNullOrWhiteSpace($inputPath)) {
            throw 'An explicit code-quality path cannot be empty'
        }

        $candidate = $inputPath
        if (-not [System.IO.Path]::IsPathRooted($candidate)) {
            $candidate = Join-Path $repoRoot $candidate
        }

        $item = Get-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue
        if (-not $item) {
            throw "Code-quality path not found: $inputPath"
        }

        $results += $item.FullName
    }

    return @($results | Sort-Object -Unique)
}

function Get-RepoRelativePath {
    param(
        [string]$FullName
    )

    return Get-CompatibleRelativePath -BasePath $repoRoot -TargetPath $FullName
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

    return @(Select-CodeQualityInputFiles -RepoRoot $repoRoot -AllFiles @(Get-CodeQualityRepositoryFiles -RepoRoot $repoRoot) -InputPaths $TargetPaths)
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
        if ([IO.Path]::GetExtension($file).ToLowerInvariant() -in @('.png', '.bin', '.hog', '.sf2', '.zip', '.7z')) { continue }
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

    $lockJson = @{
        pid = $PID
        process_start_ticks = (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString()
        repository_root = $repoRoot
        started = (Get-Date).ToString("s")
        fix = [bool]$Fix
        host = $Host.Name
        stage = $Stage
        paths = @($resolvedPaths | ForEach-Object { Get-RepoRelativePath $_ })
    } | ConvertTo-Json
    [IO.File]::WriteAllText($lockFile, $lockJson + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

function Write-CodeQualitySummary {
    param(
        [string]$Stage,
        [string[]]$PreDirty,
        [string[]]$PostDirty,
        [string[]]$CleanTransitions
    )

    $summaryJson = @{
        pid = $PID
        finished = (Get-Date).ToString("s")
        fix = [bool]$Fix
        stage = $Stage
        paths = @($resolvedPaths | ForEach-Object { Get-RepoRelativePath $_ })
        failed = @($failed)
        preDirty = @($PreDirty)
        postDirty = @($PostDirty)
        cleanTransitions = @($CleanTransitions)
    } | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText($summaryFile, $summaryJson + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

function Test-ActiveProcess {
    param(
        [int]$ProcessId
    )

    if ($ProcessId -le 0) {
        return $false
    }

    if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue) {
        $proc = Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -ErrorAction SilentlyContinue
        return $null -ne $proc
    }

    return $null -ne (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)
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

try {
    $requestedPaths = @(Resolve-CodeQualityPaths $Paths)
    if ($explicitScope -and $requestedPaths.Count -eq 0) { throw 'Explicit code-quality scope resolved to no paths' }
    $eligibleFiles = @(Get-CodeQualityRepositoryFiles -RepoRoot $repoRoot -BaseRef $BaseRef -Changed:$Changed)
    $resolvedPaths = @(Select-CodeQualityInputFiles -RepoRoot $repoRoot -AllFiles $eligibleFiles -InputPaths $requestedPaths | ForEach-Object { $_.FullName })
    if ($List) {
        $resolvedPaths | ForEach-Object { Get-RepoRelativePath $_ }
        exit 0
    }
    if ($resolvedPaths.Count -eq 0) { Write-Host 'No new files in scope'; exit 0 }
    if ($InstallTools) {
        & (Join-Path $helpersDir 'install-code-quality-tools.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Code quality tool installation failed' }
    }
} catch {
    Write-Error $_
    exit 1
}
$toolParams = @{}
if ($resolvedPaths.Count -gt 0) {
    $toolParams.Paths = $resolvedPaths
}

$preDirtyPaths = Get-GitDirtyPaths @()
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
        Write-Host "Wait for it to finish or run android\\helpers\\stop-stale-formatters.ps1 -Kill"
        exit 1
    }

    Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
}

$previousBaseRef = $env:DXX_CODE_QUALITY_BASE_REF
if ($BaseRef) { $env:DXX_CODE_QUALITY_BASE_REF = $BaseRef }
Write-CodeQualityLock -Stage 'starting'

Write-Host "=== Code Quality Checks ==="
Write-Host "Eligible new files: $($resolvedPaths.Count) (use -List to preview)"
Write-Host ""

try {
    $stages = @('clang-format', 'ktlint', 'psscriptanalyzer', 'utf8-bom', 'shellcheck', 'shfmt', 'cmake-format', 'cmake-lint', 'ruff', 'rustfmt', 'prettier')
    foreach ($stage in $stages) {
        if ($Tools -and $Tools -notcontains $stage) { continue }
        Write-CodeQualityLock -Stage $stage
        Write-Host "--- $stage ---"
        if ($stage -eq 'utf8-bom') {
            if (-not (Invoke-Utf8BomLint -TargetPaths $resolvedPaths -Fix:$Fix)) { $failed += $stage }
        } else {
            $parameters = @{ Paths = $resolvedPaths }
            if (-not $Fix -and $stage -notin @('shellcheck', 'cmake-lint')) { $parameters.Check = $true }
            try {
                & (Join-Path $helpersDir "run-$stage.ps1") @parameters
                if ($LASTEXITCODE -ne 0) { $failed += $stage }
            } catch {
                Write-Warning "${stage} failed: $_"
                $failed += $stage
            }
        }
        Write-Host ""
    }

    # --- Summary ---
    Write-Host "=== Summary ==="
    if ($failed.Count -eq 0) {
        Write-Host "All checks passed"
    } else {
        Write-Host "Failed checks: $($failed -join ', ')"
        if (-not $Fix) {
            Write-Host "Run with -Fix to auto-format and strip UTF-8 BOMs"
        }
        $exitCode = 1
    }
} catch {
    Write-Error $_
    $failed += "runner"
    $exitCode = 1
} finally {
    $postDirtyPaths = Get-GitDirtyPaths @()
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
    $env:DXX_CODE_QUALITY_BASE_REF = $previousBaseRef
}

exit $exitCode
