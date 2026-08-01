#!/usr/bin/env pwsh
# Verify safe runtime update contracts used by check-updates.ps1

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$checkUpdatesPath = Join-Path $repoRoot "android/get_deps/check-updates.ps1"
$powerShellUpdaterPath = Join-Path $repoRoot "android/get_deps/update-powershell.ps1"
$versionsPath = Join-Path $repoRoot "android/get_deps/tool_versions.conf"
$jdkUpdaterPath = Join-Path $repoRoot "android/get_deps/helpers/get_jdk.sh"
$checkUpdates = Get-Content -LiteralPath $checkUpdatesPath -Raw
$powerShellUpdater = Get-Content -LiteralPath $powerShellUpdaterPath -Raw
$versions = Get-Content -LiteralPath $versionsPath -Raw
$jdkUpdater = Get-Content -LiteralPath $jdkUpdaterPath -Raw

function Assert-Matches($content, $pattern, $label) {
    if ($content -notmatch $pattern) {
        throw "Missing contract: $label"
    }
    Write-Host "PASS: $label"
}

Assert-Matches $checkUpdates '(?s)"JDK\*"\s*\{.*?Invoke-InstallSyncForDependency \$dep \$new' `
    "a selected JDK target update immediately runs its install helper"
Assert-Matches $checkUpdates 'Installer script failed with exit code \$LASTEXITCODE' `
    "a failed Bash installer stops check-updates"
Assert-Matches $jdkUpdater 'mktemp -d .*\.jdk-\$JDK_MAJOR-stage-' `
    "JDK updates extract into a staging directory"
Assert-Matches $jdkUpdater '(?s)STAGED_VERSION=.*?mv "\$DEST" "\$BACKUP_DIR"' `
    "the staged JDK is validated before the current install is moved"
Assert-Matches $jdkUpdater 'mv "\$BACKUP_DIR" "\$DEST"' `
    "a failed replacement restores the previous JDK directory"
Assert-Matches $jdkUpdater '(?s)recover_matching_incomplete_install.*?cmp -s.*?cp -a -n' `
    "an incomplete JDK is recovered only when surviving files match the staged replacement"
Assert-Matches $versions '(?m)^POWERSHELL_INSTALL_CMD=.*update-powershell\.ps1.*-System' `
    "routine PowerShell sync updates the system runtime"
Assert-Matches $powerShellUpdater 'active PowerShell 7 processes' `
    "explicit system updates warn about terminating active processes"
Assert-Matches $powerShellUpdater 'Type INSTALL to continue' `
    "explicit system updates require confirmation unless forced"

Write-Host "All get_deps runtime update tests passed"
