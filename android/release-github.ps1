#!/usr/bin/env pwsh
#Requires -Version 5.1

<#
.SYNOPSIS
Build, verify and publish current and Android 6.0 APKs in one release
.DESCRIPTION
Run from the repo root with PowerShell 5.1/7, Android tools/JDK 21 and keystore.properties
Publishing needs clean, pushed app source and gh auth login; Markdown and release-helper edits are allowed
Origin is the default (-Repository overrides)
Reusing a version builds both editions, moves android-vVERSION and replaces both APKs and generated notes
Only APKs are uploaded; metadata/checksums stay local for recovery and obsolete uploads are removed
GitHub automatically includes source ZIP/tarball links; these cannot be removed by the helper
Use -UploadOnly to verify and upload saved release files without rebuilding; the saved source commit is used
Output: android/build-outputs/github/android-vVERSION/; -BuildOnly stays local, -Draft stages new releases
Use -Legacy -BuildOnly to prepare just the API-23 edition locally; publishing always includes both
Public filenames use android-1-recommended and android-2-only-for-android-6.0 to sort the current APK first
Release notes report the minimum and target SDK inspected from each APK
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0-rc.1 -Draft
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0 -BuildOnly
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0 -Legacy -BuildOnly
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z]+([.-][0-9A-Za-z]+)*)?$')][string]$Version,
    [ValidateRange(1, 2100000000)][int]$VersionCode,
    [ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository,
    [string]$NotesFile,
    [switch]$BuildOnly,
    [switch]$UploadOnly,
    [switch]$Draft,
    [switch]$Legacy,
    [Parameter(DontShow)][switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot 'helpers/test_host_platform.ps1')
# Markdown documentation and release-only files are not Gradle/APK inputs
$sourcePaths = @('.', ':(exclude,icase)*.md', ':(exclude)android/release-github.ps1',
    ':(exclude)android/tests/test_github_release.ps1', ':(exclude)android/ai tool plans')

function Invoke-ReleaseTool {
    param([string]$Tool, [string[]]$Arguments)
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE" }
}

function Get-AndroidVersionLabel {
    param([int]$Api)
    $versions = @{
        19 = '4.4'; 20 = '4.4W'; 21 = '5.0'; 22 = '5.1'; 23 = '6.0'; 24 = '7.0'; 25 = '7.1'
        26 = '8.0'; 27 = '8.1'; 28 = '9'; 29 = '10'; 30 = '11'; 31 = '12'; 32 = '12L'
        33 = '13'; 34 = '14'; 35 = '15'; 36 = '16'; 37 = '17'
    }
    if ($versions.ContainsKey($Api)) { return $versions[$Api] }
    return 'unknown'
}

function Get-ReleaseGit {
    param([string[]]$Arguments)
    return ((Invoke-ReleaseTool git (@('-C', $repoRoot) + $Arguments)) -join "`n").Trim()
}

function Get-ReleaseSourceStatus {
    return Get-ReleaseGit (@('status', '--porcelain', '--untracked-files=all', '--') + $sourcePaths)
}

function Assert-ReleaseSource {
    if ((Get-ReleaseGit @('rev-parse', 'HEAD')) -ne $commit) {
        throw "HEAD changed during the build of $commit; rebuild from the intended commit. App source must stay unchanged while Gradle reads it"
    }
    $status = Get-ReleaseSourceStatus
    if ($status) {
        throw "App source has uncommitted changes:`n$status`nCommit or stash these files, or use -BuildOnly"
    }
}

function Assert-RemoteCommit {
    # Capture gh's diagnostic so HTTP 422 becomes an actionable error on PowerShell 5.1 too
    $ErrorActionPreference = 'Continue'
    $PSNativeCommandUseErrorActionPreference = $false
    $result = @(& gh api --hostname github.com "repos/$Repository/commits/$commit" --jq .sha 2>&1)
    $exitCode = $LASTEXITCODE
    $detail = ($result -join "`n").Trim()
    if ($exitCode -ne 0) {
        if ($detail -match 'HTTP (404|422)|No commit found') {
            throw "GitHub cannot find source commit $commit in $Repository. Push the branch containing it to that repository (usually: git push origin HEAD), then rerun. Check -Repository and gh auth status if the commit was already pushed; private repositories can also return 404 for missing access. GitHub said: $detail"
        }
        throw "Could not verify source commit $commit in $Repository; check gh auth status, repository access and the network. GitHub said: $detail"
    }
    if ($detail -ne $commit) { throw "GitHub returned a different source commit for $commit in $Repository" }
}

function Get-ExistingRelease {
    param([string]$ReleaseTag = $tag)
    # Pagination includes drafts and avoids interpreting an auth/network error as absence
    $releaseTags = Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/releases", '--paginate', '--jq', '.[].tag_name')
    if ($ReleaseTag -notin $releaseTags) { return $null }
    $release = (Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/releases/tags/$ReleaseTag")) | ConvertFrom-Json
    if ($release.PSObject.Properties['immutable'] -and $release.immutable) {
        throw "Release $ReleaseTag is immutable; GitHub does not allow replacing its assets, so use a new version"
    }
    return $release
}

function Set-ReleaseTag {
    $refs = (Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/git/matching-refs/tags/$tag")) | ConvertFrom-Json
    $hasTag = @($refs | Where-Object ref -EQ "refs/tags/$tag").Count -gt 0
    if ($hasTag) {
        # Resolve annotated tags to their source commit before deciding whether to move them
        $tagCommit = Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/commits/$tag", '--jq', '.sha')
        if ($tagCommit -ne $commit) {
            Write-Host "Moving $tag from $tagCommit to $commit"
            Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/git/refs/tags/$tag",
                '--method', 'PATCH', '-f', "sha=$commit", '-F', 'force=true', '--silent')
        }
    } else {
        Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/git/refs",
            '--method', 'POST', '-f', "ref=refs/tags/$tag", '-f', "sha=$commit", '--silent')
    }
    $tagCommit = Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/commits/$tag", '--jq', '.sha')
    if ($tagCommit -ne $commit) { throw "Remote tag $tag does not resolve to the built commit after updating" }
}

function Merge-ReleaseNotes {
    param([string]$Generated, [string]$Existing)
    # Replace only helper-owned notes, including notes from helpers predating the markers
    $managedPattern = '(?s)<!-- dxx-redux-build:start -->.*?<!-- dxx-redux-build:end -->\s*'
    $oldPattern = '(?s)\ADXX-Redux [^\r\n]+\r?\n\s*(?:minsdk:[^\r\n]+\r?\n\s*)?- universal APK:[^\r\n]+\r?\n\s*Source commit: [0-9a-f]+\r?\nAndroid versionCode: [0-9]+\r?\nSigning certificate SHA-256: [0-9a-f]+(?:\r?\nAPK SHA-256: [0-9a-f]+)?\s*'
    $custom = [regex]::Replace($Existing, $managedPattern, '')
    $custom = [regex]::Replace($custom, $oldPattern, '').Trim()
    if ($custom -and -not $Generated.Contains($custom)) { return $Generated.TrimEnd() + "`n`n" + $custom + "`n" }
    return $Generated
}

if ($BuildOnly -and $UploadOnly) { throw '-BuildOnly and -UploadOnly cannot be combined' }
if ($VerifyOnly -and -not $UploadOnly) { throw '-VerifyOnly requires -UploadOnly' }
if ($Legacy -and -not ($BuildOnly -or $VerifyOnly)) { throw '-Legacy is only for local preparation with -BuildOnly; publish both editions without -Legacy' }
$tag = "android-v$Version"
$outputTag = if ($Legacy) { "android-legacy-v$Version" } else { $tag }
$outDir = Join-Path $PSScriptRoot "build-outputs/github/$outputTag"
$apkName = if ($Legacy) { "dxx-redux-$Version-android-legacy-universal.apk" } else { "dxx-redux-$Version-android-universal.apk" }
$applicationId = if ($Legacy) { 'com.dxxredux.app.github.legacy' } else { 'com.dxxredux.app.github' }
$distribution = if ($Legacy) { 'legacy' } else { 'github' }
$apk = Join-Path $outDir $apkName
$checksumPath = Join-Path $outDir 'SHA256SUMS.txt'
$metadataPath = Join-Path $outDir 'build-info.json'
$notesPath = Join-Path $outDir 'release-notes.md'
$savedMetadata = $null
if ($UploadOnly) {
    if ($NotesFile) { throw '-UploadOnly uses saved release notes; omit -NotesFile' }
    foreach ($file in @($apk, $checksumPath, $metadataPath, $notesPath)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Saved release file missing: $file. Run without -UploadOnly to build first" }
    }
    $savedMetadata = Get-Content -LiteralPath $metadataPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($savedMetadata.version -ne $Version -or $savedMetadata.commit -notmatch '^[0-9a-f]{40}$' -or
        $savedMetadata.versionCode -lt 1 -or $savedMetadata.versionCode -gt 2100000000) {
        throw 'Saved release metadata has an invalid version or source commit; rebuild without -UploadOnly'
    }
    $commit = $savedMetadata.commit
    if ($PSBoundParameters.ContainsKey('VersionCode') -and $VersionCode -ne $savedMetadata.versionCode) {
        throw '-VersionCode differs from the saved build; omit it or rebuild without -UploadOnly'
    }
    $VersionCode = [int]$savedMetadata.versionCode
    if ($savedMetadata.PSObject.Properties['sourceVerified']) {
        if ($savedMetadata.sourceVerified -isnot [bool] -or -not $savedMetadata.sourceVerified) {
            throw 'Saved APK source validation did not pass; rebuild without -UploadOnly'
        }
    } else {
        # Older helpers wrote metadata before the final check; verify their recorded tree locally
        $status = Get-ReleaseSourceStatus
        $differences = Get-ReleaseGit (@('diff', '--name-only', $commit, '--') + $sourcePaths)
        if ($status -or $differences) {
            throw "Cannot validate app source for this older saved APK. Changed files:`n$status`n$differences`nRebuild without -UploadOnly from clean app source"
        }
    }
} else {
    $commit = Get-ReleaseGit @('rev-parse', 'HEAD')
    $sourceCleanAtStart = -not [bool](Get-ReleaseSourceStatus)
}
if (-not $UploadOnly -and (Get-ReleaseGit @('rev-parse', '--is-shallow-repository')) -eq 'true') {
    throw 'Use a full clone (git fetch --unshallow) for the commit-count versionCode'
}
# Version labels/codes do not set the engine multiplayer protocol; GitHub and Play use separate app IDs
# Increase versionCode for app upgrades; use -VersionCode when the commit-count default is insufficient
if (-not $UploadOnly -and -not $PSBoundParameters.ContainsKey('VersionCode')) {
    $VersionCode = [int](Get-ReleaseGit @('rev-list', '--count', 'HEAD')) * 10
}
$prerelease = $Version.Contains('-')
if ($NotesFile) { $NotesFile = (Resolve-Path -LiteralPath $NotesFile).Path }

if (-not ($BuildOnly -or $VerifyOnly)) {
    if (-not $UploadOnly) { Assert-ReleaseSource }
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        throw 'Install GitHub CLI and run gh auth login before publishing, or use -BuildOnly'
    }
    if (-not $Repository) {
        $remote = Get-ReleaseGit @('remote', 'get-url', 'origin')
        if ($remote -notmatch '^(?:https://github\.com/|git@github\.com:)([^/]+/[^/]+?)(?:\.git)?$') {
            throw 'Cannot infer a github.com repository from origin; pass -Repository OWNER/REPO'
        }
        $Repository = $Matches[1]
    }
    # Fail before building if the exact source commit has not reached this repository
    Assert-RemoteCommit
    $existingRelease = Get-ExistingRelease
}

# Ignored keystore.properties format; storeFile is relative to android/ (use forward slashes)
# storeFile=release.keystore
# storePassword=YOUR_STORE_PASSWORD
# keyAlias=YOUR_KEY_ALIAS
# keyPassword=YOUR_KEY_PASSWORD
# Keep the file/key private, back up the key and use the same key for every GitHub update
if (-not $UploadOnly -and -not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'keystore.properties'))) {
    throw 'Missing android/keystore.properties; see the signing setup comments in this script'
}
Initialize-RegressionJavaEnvironment -RepoRoot $repoRoot
$buildToolsPin = Select-String -LiteralPath (Join-Path $PSScriptRoot 'get_deps/tool_versions.conf') -Pattern '^BUILD_TOOLS_VERSION=(.+)$'
if (-not $buildToolsPin) { throw 'BUILD_TOOLS_VERSION is missing from tool_versions.conf' }
$depBase = Get-RegressionDependencyBase -RepoRoot $repoRoot
$toolDir = "build-tools/$($buildToolsPin.Matches[0].Groups[1].Value)"
$signerName = if (Test-RegressionWindowsHost) { 'apksigner.bat' } else { 'apksigner' }
$signer = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir $toolDir -ToolName $signerName
$aapt = Resolve-RegressionAndroidSdkTool -DepBase $depBase -Subdir $toolDir -ToolName 'aapt2'
foreach ($tool in @($signer, $aapt)) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing Android build tool: $tool" }
}

if (-not $UploadOnly) {
    & (Join-Path $PSScriptRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $outDir
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    $builtAt = [DateTime]::UtcNow
    $buildInfo = @"
package com.dxxredux.app

// Generated by release-github.ps1
object BuildInfo {
    const val GIT_COMMIT_COUNT = "$VersionCode"
    const val GIT_SHORT_HASH = "$($commit.Substring(0, 12))"
    const val BUILD_DATE = "$($builtAt.ToString('yyyy-MM-dd'))"
    const val BUILD_TIME = "$($builtAt.ToString('HH:mm')) UTC"
    const val BUILD_TYPE = "release"
}
"@
    $buildInfoPath = Join-Path $PSScriptRoot 'app/src/main/java/com/dxxredux/app/BuildInfo.kt'
    [IO.File]::WriteAllText($buildInfoPath, $buildInfo + "`n")
    $gradle = Resolve-RegressionGradleWrapper -AndroidDir $PSScriptRoot
    Write-Host "Building $tag ($VersionCode) from $commit"
    # Separate GitHub app: Play sign-in/updates disabled; single-player/LAN still require game data
    # Clean releases do not benefit from Kotlin's incremental state across distributions
    $gradleArgs = @('-p', $PSScriptRoot, '--no-build-cache', '-Pkotlin.incremental=false', '-PgithubRelease=true',
        "-PversionNameOverride=$Version", "-PversionCodeOverride=$VersionCode", '-PskipBuildInfo', '--console=plain')
    if ($Legacy) { $gradleArgs += '-PlegacyRelease=true' }
    # Separate task graphs avoid clean/assemble scheduling conflicts with generated assets
    Invoke-ReleaseTool $gradle (@(':app:clean') + $gradleArgs)
    Invoke-ReleaseTool $gradle (@(':app:assembleRelease') + $gradleArgs)
}

# Inspect the APK itself, not just Gradle's output metadata
$sourceApk = if ($UploadOnly) { $apk } else { Join-Path $PSScriptRoot 'app/build/outputs/apk/release/app-release.apk' }
if (-not (Test-Path -LiteralPath $sourceApk)) { throw "Signed APK missing: $sourceApk" }
$signature = (Invoke-ReleaseTool $signer @('verify', '--verbose', '--print-certs', $sourceApk)) -join "`n"
# SDK versions label this as either "Signer #1" or "V2 Signer"
if ($signature -notmatch 'certificate SHA-256 digest: ([0-9a-fA-F]{64})') { throw 'Missing signing certificate digest' }
$certificate = $Matches[1].ToLowerInvariant()
$badging = (Invoke-ReleaseTool $aapt @('dump', 'badging', $sourceApk)) -join "`n"
if ($badging -notmatch "package: name='$([regex]::Escape($applicationId))' versionCode='$VersionCode' versionName='$([regex]::Escape($Version))'") {
    throw 'APK package or version does not match the requested release'
}
if ($badging -notmatch "(?m)^(?:sdkVersion|minSdkVersion):'([0-9]+)'\s*$") { throw 'APK minimum SDK is missing or invalid' }
$minSdk = [int]$Matches[1]
if ($badging -notmatch "(?m)^targetSdkVersion:'([0-9]+)'\s*$") { throw 'APK target SDK is missing or invalid' }
$targetSdk = [int]$Matches[1]
if ($targetSdk -lt $minSdk) { throw 'APK target SDK is lower than its minimum SDK' }
$sdkLabel = "minsdk: api $minSdk (android $(Get-AndroidVersionLabel $minSdk)), targetsdk: api $targetSdk (android $(Get-AndroidVersionLabel $targetSdk))"
$releaseTitle = "DXX-Revival $Version for Android"
if ($Legacy) { $releaseTitle += ' (Legacy)' }
$releaseTitle += " - $sdkLabel"
Write-Host $sdkLabel
if ($badging -match 'application-debuggable') { throw 'Refusing to release a debuggable APK' }
$abiLine = ($badging -split "`n" | Where-Object { $_ -like 'native-code:*' }) -join ' '
foreach ($abi in @('armeabi-v7a', 'arm64-v8a', 'x86_64')) {
    if (-not $abiLine.Contains("'$abi'")) { throw "Universal APK is missing $abi" }
}

if ($UploadOnly) {
    $hash = (Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $savedMetadata.apkSha256 -or $certificate -ne $savedMetadata.certificateSha256 -or
        $savedMetadata.applicationId -ne $applicationId -or
        (Get-Content -LiteralPath $checksumPath -Raw -Encoding UTF8).Trim() -ne "$hash  $apkName") {
        throw 'Saved APK/hash/signing metadata does not match; rebuild without -UploadOnly'
    }
    if (-not $savedMetadata.PSObject.Properties['minSdk'] -or -not $savedMetadata.PSObject.Properties['targetSdk'] -or
        -not $savedMetadata.PSObject.Properties['distribution'] -or
        $savedMetadata.minSdk -ne $minSdk -or $savedMetadata.targetSdk -ne $targetSdk -or
        $savedMetadata.distribution -ne $distribution) {
        throw 'Saved APK SDK/distribution metadata does not match; rebuild without -UploadOnly'
    }
    Write-Host "Verified saved APK for $tag ($VersionCode) from $commit; skipping build"
} else {
    $distributionVersions = @{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $PSScriptRoot 'distribution_versions.conf')) {
        if ($line -match '^((?:CURRENT|LEGACY)_(?:MIN|TARGET)_SDK)=([0-9]+)$') {
            $distributionVersions[$Matches[1]] = [int]$Matches[2]
        }
    }
    $sdkPrefix = if ($Legacy) { 'LEGACY' } else { 'CURRENT' }
    if ($minSdk -ne [int]$distributionVersions["${sdkPrefix}_MIN_SDK"] -or
        $targetSdk -ne [int]$distributionVersions["${sdkPrefix}_TARGET_SDK"]) {
        throw 'APK SDK versions do not match the selected distribution configuration'
    }
    Copy-Item -LiteralPath $sourceApk -Destination $apk -Force
    $hash = (Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($checksumPath, "$hash  $apkName`n")
    $metadata = [ordered]@{
        version = $Version; versionCode = $VersionCode; commit = $commit
        builtAtUtc = $builtAt.ToString('o'); applicationId = $applicationId; distribution = $distribution
        minSdk = $minSdk; targetSdk = $targetSdk
        certificateSha256 = $certificate; apkSha256 = $hash
        abis = @('armeabi-v7a', 'arm64-v8a', 'x86_64')
        sourceClean = -not [bool](Get-ReleaseSourceStatus)
        sourceVerified = $false
    }
    [IO.File]::WriteAllText($metadataPath, ($metadata | ConvertTo-Json) + "`n")
    # Keep this check: Gradle reads live app files, so source edits could produce a mixed-commit APK
    if (-not ($BuildOnly -or $VerifyOnly)) { Assert-ReleaseSource }
    $metadata.sourceVerified = $sourceCleanAtStart -and $metadata.sourceClean -and (Get-ReleaseGit @('rev-parse', 'HEAD')) -eq $commit
    [IO.File]::WriteAllText($metadataPath, ($metadata | ConvertTo-Json) + "`n")
}
# Regenerate from the verified APK even when recovering an older saved build
$notes = @"
<!-- dxx-redux-build:start -->
DXX-Revival $Version for Android ($distribution).

$sdkLabel

- universal APK: includes ARM32, ARM64 and x86_64

- Source commit: $commit
- Android versionCode: $VersionCode
- Signing certificate SHA-256: $certificate
- APK SHA-256: $hash
<!-- dxx-redux-build:end -->
"@
# UTF-8 -NotesFile appends custom notes outside the generated block
if ($NotesFile) { $notes += "`n`n" + (Get-Content -LiteralPath $NotesFile -Raw -Encoding UTF8) }
if ($UploadOnly) { $notes = Merge-ReleaseNotes -Generated $notes -Existing (Get-Content -LiteralPath $notesPath -Raw -Encoding UTF8) }
[IO.File]::WriteAllText($notesPath, $notes.TrimEnd() + "`n")
Write-Host "Verified APK and release files: $outDir"
if ($Legacy -or $VerifyOnly) { return }

# Prepare the other edition before any remote mutation; each build gets its own verification
$legacyParameters = @{ Version = $Version; VersionCode = $VersionCode; Legacy = $true }
if ($UploadOnly) {
    $legacyParameters.UploadOnly = $true
    $legacyParameters.VerifyOnly = $true
} else {
    $legacyParameters.BuildOnly = $true
}
& $PSCommandPath @legacyParameters
$legacyDir = Join-Path $PSScriptRoot "build-outputs/github/android-legacy-v$Version"
$legacyInfo = Get-Content -LiteralPath (Join-Path $legacyDir 'build-info.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($legacyInfo.commit -ne $commit -or $legacyInfo.versionCode -ne $VersionCode -or $legacyInfo.certificateSha256 -ne $certificate) {
    throw 'Current and legacy APKs must have the same source commit, versionCode and signing certificate; rebuild both editions'
}
if (-not $BuildOnly) {
    if (-not $UploadOnly) { Assert-ReleaseSource }
    if (-not $legacyInfo.sourceVerified) { throw 'Legacy APK source validation did not pass; rebuild both editions' }
}
$legacyAndroid = Get-AndroidVersionLabel $legacyInfo.minSdk
$recommendedApkName = "dxx-revival-$Version-android-1-recommended-universal.apk"
$recommendedApk = Join-Path $outDir $recommendedApkName
Copy-Item -LiteralPath $apk -Destination $recommendedApk -Force
$legacyApkName = "dxx-revival-$Version-android-2-only-for-android-$legacyAndroid-universal.apk"
$legacyApk = Join-Path $outDir $legacyApkName
Copy-Item -LiteralPath (Join-Path $legacyDir "dxx-redux-$Version-android-legacy-universal.apk") -Destination $legacyApk -Force
$legacySdkLabel = "minsdk: api $($legacyInfo.minSdk) (android $legacyAndroid), targetsdk: api $($legacyInfo.targetSdk) (android $(Get-AndroidVersionLabel $legacyInfo.targetSdk))"
$combinedNotes = @"
<!-- dxx-redux-build:start -->
Both APKs include ARM32, ARM64 and x86_64

### Android $(Get-AndroidVersionLabel $minSdk) or newer (recommended)

- Download: $recommendedApkName
- $sdkLabel
- APK SHA-256: $hash

### special apk with support for older android $legacyAndroid - use the recommended apk for newer devices

- Download: $legacyApkName
- this is a special build to go one step lower on minimum android version (to $legacyAndroid). On Android $(Get-AndroidVersionLabel $minSdk) or newer, use the recommended APK instead
- $legacySdkLabel
- APK SHA-256: $($legacyInfo.apkSha256)

### Build details

- Source commit: $commit
- Android versionCode: $VersionCode
- Signing certificate SHA-256: $certificate
<!-- dxx-redux-build:end -->
"@
$notes = Merge-ReleaseNotes -Generated $combinedNotes -Existing (Get-Content -LiteralPath $notesPath -Raw -Encoding UTF8)
[IO.File]::WriteAllText($notesPath, $notes.TrimEnd() + "`n")
if ($BuildOnly) { return }
$releaseTitle = "DXX-Revival $Version for Android"
$existingRelease = Get-ExistingRelease
$ghRepository = "github.com/$Repository"
$createArgs = @('release', 'create', $tag, $recommendedApk, $legacyApk,
    '--repo', $ghRepository, '--target', $commit, '--title', $releaseTitle,
    '--notes-file', $notesPath, '--draft')
if ($prerelease) { $createArgs += '--prerelease' }
try {
    # Only mutate the remote tag after APK verification and the final source/release checks
    # Tag/assets updates are not atomic; rerun the same command to repair a partial failure
    Set-ReleaseTag
    if ($existingRelease) {
        Invoke-ReleaseTool gh @('release', 'edit', $tag, '--repo', $ghRepository, '--target', $commit, '--title', $releaseTitle)
        Write-Host "Replacing both APKs in $tag"
        Invoke-ReleaseTool gh @('release', 'upload', $tag, $recommendedApk, $legacyApk, '--repo', $ghRepository, '--clobber')
        $notes = Merge-ReleaseNotes -Generated (Get-Content -LiteralPath $notesPath -Raw -Encoding UTF8) -Existing $existingRelease.body
        [IO.File]::WriteAllText($notesPath, $notes)
        Invoke-ReleaseTool gh @('release', 'edit', $tag, '--repo', $ghRepository, '--notes-file', $notesPath)
        # Remove old helper uploads after the replacement succeeds; leave unrelated assets alone
        foreach ($asset in $existingRelease.assets) {
            if ($asset.name -in @('build-info.json', 'build.json', 'SHA256SUMS.txt', "dxx-redux-$Version-android-legacy-universal.apk",
                    $apkName, "dxx-redux-$Version-android-only-for-android-$legacyAndroid-universal.apk",
                    "dxx-redux-$Version-android-1-recommended-universal.apk", "dxx-redux-$Version-android-2-only-for-android-$legacyAndroid-universal.apk")) {
                Invoke-ReleaseTool gh @('release', 'delete-asset', $tag, $asset.name, '--repo', $ghRepository, '--yes')
            }
        }
    } else {
        Invoke-ReleaseTool gh $createArgs
        if (-not $Draft) {
            $publishArgs = @('release', 'edit', $tag, '--repo', $ghRepository, '--draft=false')
            Invoke-ReleaseTool gh $publishArgs
        }
    }
} catch {
    throw "Release tag/upload/publish failed; inspect $tag on GitHub for its commit and missing or incomplete assets. The tag may already have moved. Local assets remain in $outDir; retry without rebuilding: ./android/release-github.ps1 -Version $Version -Repository $Repository -UploadOnly. Existing drafts remain drafts. $_"
}

# Retire the old separate release only after GitHub confirms both uploaded APK hashes
$uploadedRelease = Get-ExistingRelease
foreach ($expected in @(@{ name = $recommendedApkName; hash = $hash }, @{ name = $legacyApkName; hash = $legacyInfo.apkSha256 })) {
    $uploadedAsset = @($uploadedRelease.assets | Where-Object name -EQ $expected.name)
    if ($uploadedAsset.Count -ne 1 -or $uploadedAsset[0].digest -ne "sha256:$($expected.hash)") {
        throw "Uploaded APK verification failed for $($expected.name); keep the separate legacy release and retry with -UploadOnly"
    }
}
$separateLegacyTag = "android-legacy-v$Version"
$separateLegacy = Get-ExistingRelease -ReleaseTag $separateLegacyTag
if ($separateLegacy -and -not $uploadedRelease.draft) {
    $unknownAssets = @($separateLegacy.assets | Where-Object { $_.name -notin @("dxx-redux-$Version-android-legacy-universal.apk", 'build-info.json', 'build.json', 'SHA256SUMS.txt') })
    if ($unknownAssets.Count -gt 0) { throw "Separate legacy release has unrelated assets; move them before retiring $separateLegacyTag" }
    try {
        Invoke-ReleaseTool gh @('release', 'delete', $separateLegacyTag, '--repo', $ghRepository, '--cleanup-tag', '--yes')
    } catch {
        throw "Combined release is verified, but retiring $separateLegacyTag failed; retry with -UploadOnly. $_"
    }
}
Invoke-ReleaseTool gh @('release', 'view', $tag, '--repo', $ghRepository, '--json', 'url', '--jq', '.url')
