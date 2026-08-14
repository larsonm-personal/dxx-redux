param()

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path

function Invoke-Git {
    param([string[]]$Arguments)

    $output = & git -C $repositoryRoot @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed: $output"
    }
    return @($output)
}

$forbiddenTrackedPaths = @(
    ".tmp"
    "android/build_output.txt"
    "android/test_output.txt"
    "d1_d2_ogl_diff.txt"
    "dxx_matchmaking.db"
    "dxx_matchmaking.db-shm"
    "dxx_matchmaking.db-wal"
    "game_data_to_copy_to_emulator/debuglog_20260520_211753.txt"
    "test.txt"
)

# These exact source fixtures must stay tracked and visible to Git
$maintainedFixtureAllowlist = @(
    "android/tests/test_redbook_data/test_disc.bin"
)

$forbiddenTrackedPatterns = @(
    '^dxx_matchmaking\.db(?:-(?:shm|wal))?$'
    '^game_data_to_copy_to_emulator/debuglog_[^/]+\.txt$'
    '^android/(?:build|test)_output\.txt$'
    '^(?:\.tmp|d1_d2_ogl_diff\.txt|test\.txt)$'
)

$tracked = @(Invoke-Git -Arguments @("ls-files"))
$deleted = @(Invoke-Git -Arguments @("ls-files", "--deleted"))
$deletedSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($path in $deleted) {
    $null = $deletedSet.Add($path)
}
$effectiveTracked = @($tracked | Where-Object { -not $deletedSet.Contains($_) })
$trackedSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($path in $effectiveTracked) {
    $null = $trackedSet.Add($path)
}

$unexpected = @(
    $effectiveTracked | Where-Object {
        $candidate = $_
        @($forbiddenTrackedPatterns | Where-Object { $candidate -match $_ }).Count -ne 0
    }
)
if ($unexpected.Count -ne 0) {
    throw "Runtime or scratch artifacts are tracked: $($unexpected -join ', ')"
}

foreach ($path in $forbiddenTrackedPaths) {
    $ignored = & git -C $repositoryRoot check-ignore --quiet --no-index -- $path
    if ($LASTEXITCODE -ne 0) {
        throw "Removed runtime or scratch path is not ignored: $path"
    }
}

foreach ($path in $maintainedFixtureAllowlist) {
    if (-not $trackedSet.Contains($path)) {
        throw "Maintained fixture is not tracked: $path"
    }
    & git -C $repositoryRoot check-ignore --quiet --no-index -- $path
    if ($LASTEXITCODE -eq 0) {
        throw "Maintained fixture is hidden by an ignore rule: $path"
    }
}

Write-Output "Repository artifact policy passed"
exit 0
