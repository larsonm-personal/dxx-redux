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
. (Join-Path $repoRoot 'android/get_deps/helpers/safe_conf_value.ps1')

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
Assert-Matches $jdkUpdater 'create_temp_dir "\.jdk-\$JDK_MAJOR-stage" "\$INSTALL_DIR"' `
    "JDK updates extract into a staging directory"
Assert-Matches $jdkUpdater '(?s)STAGED_VERSION=.*?mv "\$DEST" "\$BACKUP_DIR"' `
    "the staged JDK is validated before the current install is moved"
Assert-Matches $jdkUpdater 'mv "\$BACKUP_DIR" "\$DEST"' `
    "a failed replacement restores the previous JDK directory"
Assert-Matches $jdkUpdater '(?s)recover_matching_incomplete_install.*?cmp -s.*?cp -a -n' `
    "an incomplete JDK is recovered only when surviving files match the staged replacement"
Assert-Matches $versions '(?m)^PLAY_SERVICES_GAMES_VERSION=21\.0\.0$' `
    "Play Games remains on the newest release compatible with minSdk 23"
Assert-Matches $checkUpdates '(?s)Name = "play-services-games-v2".*?SuppressTargetUpdate = \$true.*?requires minSdk 24' `
    "dependency checks do not automatically select Play Games releases that require minSdk 24"
Assert-Matches $versions '(?m)^POWERSHELL_INSTALL_CMD=.*update-powershell\.ps1.*-System' `
    "routine PowerShell sync updates the system runtime"
Assert-Matches $powerShellUpdater 'active PowerShell 7 processes' `
    "explicit system updates warn about terminating active processes"
Assert-Matches $powerShellUpdater 'Type INSTALL to continue' `
    "explicit system updates require confirmation unless forced"

foreach ($case in @(
        @{ Key = 'GRADLE_VERSION'; Value = '9.6.1' },
        @{ Key = 'NDK_VERSION'; Value = 'r27d' },
        @{ Key = 'MINIMP3_COMMIT'; Value = '853a0a171759f1ddba0de1442133a75912bbeffa' },
        @{ Key = 'JDK_URL'; Value = 'https://api.adoptium.net/v3/binary/latest/21/ga?project=jdk&image_type=jdk' }
    )) {
    if (-not (Test-SafeToolConfValue -Key $case.Key -Value $case.Value)) {
        throw "Valid value rejected: $($case.Key)=$($case.Value)"
    }
    if ((Format-SafeToolConfAssignment -Key $case.Key -Value $case.Value) -ne "$($case.Key)='$($case.Value)'") {
        throw 'Safe tool configuration assignment was not single-quoted'
    }
}
foreach ($value in @(
        '999$(Write-Output_PWNED)', '999`Write-Output_PWNED`', '1.2.3;echo', '1.2.3|echo',
        '1.2.3 value', "1.2.3`necho", '${HOME}', '../version', '-option'
    )) {
    if (Test-SafeToolConfValue -Key 'TEST_VERSION' -Value $value) {
        throw "Unsafe value accepted: $value"
    }
}
if (Test-SafeToolConfValue -Key 'TEST_URL' -Value 'http://example.com/tool.zip') {
    throw 'Non-HTTPS URL accepted'
}
if (Test-SafeToolConfValue -Key 'TEST_COMMIT' -Value 'abc123') {
    throw 'Short commit accepted'
}

Write-Host "All get_deps runtime update tests passed"
