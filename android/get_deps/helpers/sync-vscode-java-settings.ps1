#!/usr/bin/env pwsh
# Keep the checked-in VS Code Java settings aligned with the managed JDK.

param(
    [string]$SettingsPath,
    [string]$DependencyBase,
    [int]$JdkMajor
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = $PSScriptRoot
$repoRoot = Split-Path (Split-Path (Split-Path $scriptDir -Parent) -Parent) -Parent
$confPath = Join-Path (Split-Path $scriptDir -Parent) "tool_versions.conf"

if (-not $SettingsPath) {
    $SettingsPath = Join-Path $repoRoot ".vscode/settings.json"
}
if (-not $DependencyBase) {
    . (Join-Path $scriptDir "Get-DepPlatform.ps1")
    $DependencyBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing
}
if (-not $JdkMajor) {
    $majorLine = Get-Content -LiteralPath $confPath | Where-Object { $_ -match '^JDK_MAJOR=' } | Select-Object -First 1
    if (-not $majorLine) {
        throw "JDK_MAJOR is missing from $confPath"
    }
    $JdkMajor = [int](($majorLine -split '=', 2)[1].Trim("'`" "))
}
if (-not (Test-Path -LiteralPath $SettingsPath)) {
    throw "VS Code settings file not found: $SettingsPath"
}

$jdkPath = Join-Path $DependencyBase "jdk-$JdkMajor"
$jsonJdkPath = ConvertTo-Json -InputObject $jdkPath -Compress
$content = Get-Content -LiteralPath $SettingsPath -Raw

$gradleHomePattern = '(?m)^(\s*"java\.import\.gradle\.java\.home"\s*:\s*)"(?:\\.|[^"])*"'
if ($content -match $gradleHomePattern) {
    $content = [regex]::Replace($content, $gradleHomePattern, "`$1$jsonJdkPath", 1)
} else {
    $content = [regex]::Replace(
        $content,
        '(?m)^\{\s*$',
        "{`n    `"java.import.gradle.java.home`": $jsonJdkPath,",
        1
    )
}

$runtimePattern = '(?s)("java\.configuration\.runtimes"\s*:\s*\[\s*\{\s*"name"\s*:\s*)"JavaSE-\d+"(\s*,\s*"path"\s*:\s*)"(?:\\.|[^"])*"'
if ($content -notmatch $runtimePattern) {
    throw "Managed Java runtime entry not found in $SettingsPath"
}
$runtimeReplacement = "`$1`"JavaSE-$JdkMajor`"`$2$jsonJdkPath"
$content = [regex]::Replace($content, $runtimePattern, $runtimeReplacement, 1)

[System.IO.File]::WriteAllText($SettingsPath, $content, [System.Text.UTF8Encoding]::new($false))
Write-Host "VS Code Gradle JVM set to $jdkPath"
