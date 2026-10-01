#!/usr/bin/env pwsh
#Requires -Version 5.1

<#
.SYNOPSIS
Build, verify and publish a signed universal APK for Android 7.0 or newer
.DESCRIPTION
Run from the repo root with PowerShell 5.1/7, Android tools/JDK 21 and keystore.properties
Publishing needs clean, pushed app source and gh auth login; origin is the default (-Repository overrides)
Reusing a version does a clean build, moves android-vVERSION and replaces APK/checksums/build metadata
Use -UploadOnly to verify and upload saved release files without rebuilding; the saved source commit is used
Output: android/build-outputs/github/android-vVERSION/; -BuildOnly stays local, -Draft stages new releases
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0-rc.1 -Draft
.EXAMPLE
./android/release-github.ps1 -Version 1.2.0 -BuildOnly
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z]+([.-][0-9A-Za-z]+)*)?$')][string]$Version,
    [ValidateRange(1, 2100000000)][int]$VersionCode,
    [ValidatePattern('^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$')][string]$Repository,
    [string]$NotesFile,
    [switch]$BuildOnly,
    [switch]$UploadOnly,
    [switch]$Draft
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot 'helpers/test_host_platform.ps1')
# These release-only files are not Gradle/APK inputs
$sourcePaths = @('.', ':(exclude)android/release-github.ps1',
    ':(exclude)android/tests/test_github_release.ps1', ':(exclude)android/ai tool plans')

function Invoke-ReleaseTool {
    param([string]$Tool, [string[]]$Arguments)
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE" }
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
        throw "Publishing requires a clean working tree for app source. Changed files:`n$status`nCommit or stash these before building (or use -BuildOnly). The final check prevents publishing an APK built from changing source; release-only helper edits are allowed"
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
    # Pagination includes drafts and avoids interpreting an auth/network error as absence
    $releaseTags = Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/releases", '--paginate', '--jq', '.[].tag_name')
    if ($tag -notin $releaseTags) { return $null }
    $release = (Invoke-ReleaseTool gh @('api', '--hostname', 'github.com', "repos/$Repository/releases/tags/$tag")) | ConvertFrom-Json
    if ($release.PSObject.Properties['immutable'] -and $release.immutable) {
        throw "Release $tag is immutable; GitHub does not allow replacing its assets, so use a new version"
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

if ($BuildOnly -and $UploadOnly) { throw '-BuildOnly and -UploadOnly cannot be combined' }
$tag = "android-v$Version"
$outDir = Join-Path $PSScriptRoot "build-outputs/github/$tag"
$apkName = "dxx-redux-$Version-android-universal.apk"
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

if (-not $BuildOnly) {
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
    $gradleArgs = @('-p', $PSScriptRoot, '--no-build-cache', '-PgithubRelease=true',
        "-PversionNameOverride=$Version", "-PversionCodeOverride=$VersionCode", '-PskipBuildInfo', '--console=plain')
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
if ($badging -notmatch "package: name='com.dxxredux.app.github' versionCode='$VersionCode' versionName='$([regex]::Escape($Version))'") {
    throw 'APK package or version does not match the requested release'
}
if ($badging -match 'application-debuggable') { throw 'Refusing to release a debuggable APK' }
$abiLine = ($badging -split "`n" | Where-Object { $_ -like 'native-code:*' }) -join ' '
foreach ($abi in @('armeabi-v7a', 'arm64-v8a', 'x86_64')) {
    if (-not $abiLine.Contains("'$abi'")) { throw "Universal APK is missing $abi" }
}

if ($UploadOnly) {
    $hash = (Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $savedMetadata.apkSha256 -or $certificate -ne $savedMetadata.certificateSha256 -or
        $savedMetadata.applicationId -ne 'com.dxxredux.app.github' -or
        (Get-Content -LiteralPath $checksumPath -Raw -Encoding UTF8).Trim() -ne "$hash  $apkName") {
        throw 'Saved APK/hash/signing metadata does not match; rebuild without -UploadOnly'
    }
    Write-Host "Verified saved APK for $tag ($VersionCode) from $commit; skipping build"
} else {
    Copy-Item -LiteralPath $sourceApk -Destination $apk -Force
    $hash = (Get-FileHash -LiteralPath $apk -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($checksumPath, "$hash  $apkName`n")
    $metadata = [ordered]@{
        version = $Version; versionCode = $VersionCode; commit = $commit
        builtAtUtc = $builtAt.ToString('o'); applicationId = 'com.dxxredux.app.github'
        certificateSha256 = $certificate; apkSha256 = $hash
        abis = @('armeabi-v7a', 'arm64-v8a', 'x86_64')
        sourceClean = -not [bool](Get-ReleaseSourceStatus)
        sourceVerified = $false
    }
    [IO.File]::WriteAllText($metadataPath, ($metadata | ConvertTo-Json) + "`n")
    $notes = @"
DXX-Redux $Version for Android (Android 7.0 or newer).

- universal APK: includes ARM32, ARM64 and x86_64

Source commit: $commit
Android versionCode: $VersionCode
Signing certificate SHA-256: $certificate
"@
    # UTF-8 -NotesFile appends to local generated notes; existing GitHub notes are preserved
    if ($NotesFile) { $notes += "`n`n" + (Get-Content -LiteralPath $NotesFile -Raw -Encoding UTF8) }
    [IO.File]::WriteAllText($notesPath, $notes + "`n")
    Write-Host "Verified APK and release files: $outDir"
    # Keep this check: Gradle reads live app files, so source edits could produce a mixed-commit APK
    if (-not $BuildOnly) { Assert-ReleaseSource }
    $metadata.sourceVerified = $sourceCleanAtStart -and $metadata.sourceClean -and (Get-ReleaseGit @('rev-parse', 'HEAD')) -eq $commit
    [IO.File]::WriteAllText($metadataPath, ($metadata | ConvertTo-Json) + "`n")
    if ($BuildOnly) { return }
}
$existingRelease = Get-ExistingRelease
$ghRepository = "github.com/$Repository"
$createArgs = @('release', 'create', $tag, $apk, $checksumPath, $metadataPath,
    '--repo', $ghRepository, '--target', $commit, '--title', "DXX-Redux $Version for Android",
    '--notes-file', $notesPath, '--draft')
if ($prerelease) { $createArgs += '--prerelease' }
try {
    # Only mutate the remote tag after APK verification and the final source/release checks
    # Tag/assets updates are not atomic; rerun the same command to repair a partial failure
    Set-ReleaseTag
    if ($existingRelease) {
        # Preserve existing notes/status; build-info.json records the current commit/versionCode
        Invoke-ReleaseTool gh @('release', 'edit', $tag, '--repo', $ghRepository, '--target', $commit)
        Write-Host "Replacing generated assets in $tag"
        Invoke-ReleaseTool gh @('release', 'upload', $tag, $apk, $checksumPath, $metadataPath, '--repo', $ghRepository, '--clobber')
    } else {
        Invoke-ReleaseTool gh $createArgs
        if (-not $Draft) {
            Invoke-ReleaseTool gh @('release', 'edit', $tag, '--repo', $ghRepository, '--draft=false')
        }
    }
} catch {
    throw "Release tag/upload/publish failed; inspect $tag on GitHub for its commit and missing or incomplete assets. The tag may already have moved. Local assets remain in $outDir; retry without rebuilding: ./android/release-github.ps1 -Version $Version -Repository $Repository -UploadOnly. Existing drafts remain drafts. $_"
}
Invoke-ReleaseTool gh @('release', 'view', $tag, '--repo', $ghRepository, '--json', 'url', '--jq', '.url')
