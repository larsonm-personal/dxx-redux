#!/usr/bin/env pwsh
# Exercise release orchestration with real Git and fake external services
#Requires -Version 5.1
[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSAvoidGlobalVars', '', Justification = 'Mock services share scenario state across script and module scopes')]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$testRoot = Join-Path $repoRoot ('android/temp/github_release_tests/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $testRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

function Assert-Test {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

# Functions are inherited by the release script, so gh can never reach the network
function gh {
    $global:LASTEXITCODE = 0
    $global:dxxReleaseTestCalls.Add(($args -join ' '))
    if ($args[0] -eq 'api') {
        $endpoint = $args[3]
        if ($endpoint -like '*/commits/*') {
            if ($global:dxxReleaseTestScenario -in @('unpushed', 'upload-only-unpushed')) {
                $global:LASTEXITCODE = 1
                Write-Error 'gh: No commit found for SHA (HTTP 422)' -ErrorAction Continue
                return
            }
            if ($global:dxxReleaseTestScenario -eq 'commit-api-failure') {
                $global:LASTEXITCODE = 1
                Write-Error 'gh: Service unavailable (HTTP 503)' -ErrorAction Continue
                return
            }
            $revision = $endpoint.Substring($endpoint.LastIndexOf('/') + 1)
            $resolved = & git -C $global:dxxReleaseTestRemote rev-parse --verify "$revision^{commit}"
            $global:LASTEXITCODE = $LASTEXITCODE
            return $resolved
        }
        if ($endpoint -like '*/git/matching-refs/*') {
            $prefix = "refs/tags/$global:dxxReleaseTestTag"
            $refs = @(& git -C $global:dxxReleaseTestRemote for-each-ref '--format=%(refname)' refs/tags |
                    Where-Object { $_.StartsWith($prefix) } | ForEach-Object { @{ ref = $_ } })
            return (ConvertTo-Json -InputObject $refs)
        }
        if ($endpoint -like '*/git/refs*') {
            $method = $args[[Array]::IndexOf($args, '--method') + 1]
            Assert-Test ($method -in @('POST', 'PATCH')) 'Unexpected Git reference method'
            Assert-Test ($args -contains "sha=$global:dxxReleaseTestCommit") 'Tag must target the built commit'
            # Remote mutations must wait until the APK and matching metadata are verified
            $info = Get-Content (Join-Path $global:dxxReleaseTestOutput 'build-info.json') -Raw | ConvertFrom-Json
            Assert-Test ($info.commit -eq $global:dxxReleaseTestCommit) 'Tag changed before build verification'
            if ($method -eq 'PATCH') {
                Assert-Test ($args -contains '-F' -and $args -contains 'force=true') 'Tag moves need a typed force flag'
                if ($global:dxxReleaseTestScenario -eq 'tag-update-failure') { $global:LASTEXITCODE = 1; return }
            } else {
                Assert-Test ($args -contains "ref=refs/tags/$global:dxxReleaseTestTag") 'Wrong tag was created'
                if ($global:dxxReleaseTestScenario -eq 'tag-create-failure') { $global:LASTEXITCODE = 1; return }
            }
            if ($global:dxxReleaseTestScenario -ne 'tag-verification-failure') {
                & git -C $global:dxxReleaseTestRemote update-ref "refs/tags/$global:dxxReleaseTestTag" $global:dxxReleaseTestCommit
                $global:LASTEXITCODE = $LASTEXITCODE
            }
            return
        }
        if ($endpoint -like '*/releases') {
            if ($global:dxxReleaseTestScenario -eq 'api-failure') { $global:LASTEXITCODE = 1 }
            if ($global:dxxReleaseTestHasRelease) { Write-Output $global:dxxReleaseTestTag }
            if ($global:dxxReleaseTestSeparateLegacy) { Write-Output "android-legacy-v$global:dxxReleaseTestVersion" }
            return
        }
        if ($endpoint -like '*/releases/tags/*') {
            if ($global:dxxReleaseTestScenario -eq 'release-api-failure') { $global:LASTEXITCODE = 1; return }
            if ($endpoint.EndsWith("/android-legacy-v$global:dxxReleaseTestVersion")) {
                return (@{ draft = $false; immutable = $false; assets = @(@{ name = "dxx-redux-$global:dxxReleaseTestVersion-android-legacy-universal.apk" }) } | ConvertTo-Json -Depth 4)
            }
            return (@{ draft = $global:dxxReleaseTestIsDraft; immutable = $global:dxxReleaseTestImmutable; target_commitish = $global:dxxReleaseTestTarget
                    body = $global:dxxReleaseTestBody; assets = @($global:dxxReleaseTestAssets.Keys | ForEach-Object {
                            $digest = $null
                            $assetPath = Join-Path $global:dxxReleaseTestOutput $_
                            if (Test-Path -LiteralPath $assetPath) { $digest = 'sha256:' + (Get-FileHash -LiteralPath $assetPath).Hash.ToLowerInvariant() }
                            if ($global:dxxReleaseTestScenario -eq 'uploaded-hash-mismatch') { $digest = 'sha256:' + ('0' * 64) }
                            @{ name = $_; digest = $digest }
                        })
                } | ConvertTo-Json -Depth 4)
        }
        throw "Unexpected gh API request: $args"
    }
    if ($args[0] -eq 'release' -and $args[1] -eq 'create') {
        $title = $args[[Array]::IndexOf($args, '--title') + 1]
        Assert-Test ($title -eq "DXX-Revival $global:dxxReleaseTestVersion for Android") 'Incorrect combined release title'
        foreach ($index in 3..4) { Assert-Test (Test-Path -LiteralPath $args[$index]) 'Missing upload APK' }
        Assert-Test ($args[5] -eq '--repo') 'Only the two APKs should be uploaded'
        Assert-Test ($args -contains '--draft') 'Upload must be staged as draft'
        Assert-Test ($args -contains $global:dxxReleaseTestCommit) 'Release must target the source commit'
        Assert-Test ((& git -C $global:dxxReleaseTestRemote rev-parse "refs/tags/$($args[2])^{commit}") -eq $global:dxxReleaseTestCommit) 'Release created before tag was updated'
        $global:dxxReleaseTestHasRelease = $true
        $global:dxxReleaseTestIsDraft = $true
        $global:dxxReleaseTestTarget = $global:dxxReleaseTestCommit
        if ($global:dxxReleaseTestScenario -in @('upload-failure', 'rerun-upload-failure')) { $global:LASTEXITCODE = 1 }
        foreach ($index in 3..4) { $global:dxxReleaseTestAssets[[IO.Path]::GetFileName($args[$index])] = [IO.File]::ReadAllText($args[$index]) }
        $global:dxxReleaseTestBody = [IO.File]::ReadAllText($args[[Array]::IndexOf($args, '--notes-file') + 1])
        return
    }
    if ($args[0] -eq 'release' -and $args[1] -eq 'upload') {
        Assert-Test ($args -contains '--clobber') 'Existing assets must be replaced'
        Assert-Test ((& git -C $global:dxxReleaseTestRemote rev-parse "refs/tags/$($args[2])^{commit}") -eq $global:dxxReleaseTestCommit) 'Assets uploaded before tag was updated'
        Assert-Test ($global:dxxReleaseTestTarget -eq $global:dxxReleaseTestCommit) 'Release target was not refreshed'
        if ($global:dxxReleaseTestScenario -eq 'replace-failure') { $global:LASTEXITCODE = 1; return }
        Assert-Test ($args[5] -eq '--repo') 'Replacement should upload both APKs'
        foreach ($index in 3..4) { $global:dxxReleaseTestAssets[[IO.Path]::GetFileName($args[$index])] = [IO.File]::ReadAllText($args[$index]) }
        return
    }
    if ($args[0] -eq 'release' -and $args[1] -eq 'delete-asset') {
        Assert-Test ($args[3] -in @('build-info.json', 'build.json', 'SHA256SUMS.txt',
                $global:dxxReleaseTestApkName, $global:dxxReleaseTestOldCombinedLegacyName,
                "dxx-redux-$global:dxxReleaseTestVersion-android-1-recommended-universal.apk", "dxx-redux-$global:dxxReleaseTestVersion-android-2-only-for-android-6.0-universal.apk")) 'Unexpected asset deletion'
        Assert-Test ($global:dxxReleaseTestCalls[$global:dxxReleaseTestCalls.Count - 2] -like 'release edit *--notes-file *' -or
            $global:dxxReleaseTestCalls[$global:dxxReleaseTestCalls.Count - 2] -like 'release delete-asset *') 'Cleanup must follow successful APK and notes replacement'
        if ($global:dxxReleaseTestScenario -eq 'cleanup-failure') { $global:LASTEXITCODE = 1; return }
        $global:dxxReleaseTestAssets.Remove($args[3])
        return
    }
    if ($args[0] -eq 'release' -and $args[1] -eq 'edit') {
        if ($args -contains '--draft=false') {
            $global:dxxReleaseTestIsDraft = $false
            return
        }
        if ($args -contains '--notes-file') {
            if ($global:dxxReleaseTestScenario -eq 'notes-update-failure') { $global:LASTEXITCODE = 1; return }
            $global:dxxReleaseTestBody = [IO.File]::ReadAllText($args[[Array]::IndexOf($args, '--notes-file') + 1])
            return
        }
        Assert-Test ($args -contains '--target' -and $args -contains $global:dxxReleaseTestCommit) 'Unexpected release edit'
        $title = $args[[Array]::IndexOf($args, '--title') + 1]
        Assert-Test ($title -eq "DXX-Revival $global:dxxReleaseTestVersion for Android") 'Existing release title was not refreshed'
        if ($global:dxxReleaseTestScenario -eq 'retarget-failure') { $global:LASTEXITCODE = 1; return }
        $global:dxxReleaseTestTarget = $global:dxxReleaseTestCommit
        return
    }
    if ($args[0] -eq 'release' -and $args[1] -eq 'delete') {
        Assert-Test ($args[2] -eq "android-legacy-v$global:dxxReleaseTestVersion" -and $args -contains '--cleanup-tag') 'Only the obsolete separate legacy release/tag may be removed'
        Assert-Test ($global:dxxReleaseTestAssets.ContainsKey($global:dxxReleaseTestCombinedLegacyName)) 'Separate legacy release deleted before its APK was uploaded'
        Assert-Test ($global:dxxReleaseTestBody.Contains('special apk with support for older android 6.0 - use the recommended apk for newer devices')) 'Separate legacy release deleted before combined notes were published'
        if ($global:dxxReleaseTestScenario -eq 'legacy-retirement-failure') { $global:LASTEXITCODE = 1; return }
        $global:dxxReleaseTestSeparateLegacy = $false
        return
    }
    if ($args[0] -eq 'release' -and $args[1] -eq 'view') { return }
    throw "Unexpected gh request: $args"
}

$helperFixture = @'
function Initialize-RegressionJavaEnvironment { param($RepoRoot) }
function Get-RegressionDependencyBase { param($RepoRoot) return $RepoRoot }
function Test-RegressionWindowsHost { return $false }
function Resolve-RegressionAndroidSdkTool {
    param($DepBase, $Subdir, $ToolName)
    return (Join-Path $DepBase "android/helpers/$ToolName.ps1")
}
function Resolve-RegressionGradleWrapper {
    param($AndroidDir)
    return (Join-Path $AndroidDir 'helpers/gradle.ps1')
}
'@
$gradleFixture = @'
$global:LASTEXITCODE = 0
$global:dxxReleaseTestCalls.Add('gradle ' + ($args -join ' '))
if ($args -contains ':app:clean') {
    Assert-Test (-not ($args -contains ':app:assembleRelease')) 'Clean must finish in its own Gradle invocation'
    if ($global:dxxReleaseTestScenario -eq 'clean-failure') { $global:LASTEXITCODE = 1 }
    return
}
$global:dxxReleaseTestBuildNumber++
$global:dxxReleaseTestBuildingLegacy = $args -contains '-PlegacyRelease=true'
if ($global:dxxReleaseTestScenario -eq 'legacy-build-failure' -and $global:dxxReleaseTestBuildingLegacy) { $global:LASTEXITCODE = 1; return }
Assert-Test ($global:dxxReleaseTestCalls[$global:dxxReleaseTestCalls.Count - 2] -like 'gradle *:app:clean *') 'Assemble must follow a successful clean'
Assert-Test ($args -contains ':app:assembleRelease' -and $args -contains '--no-build-cache') 'Release must assemble without build-cache reuse'
Assert-Test ($args -contains '-Pkotlin.incremental=false') 'Clean releases must not reuse Kotlin incremental state'
if ($global:dxxReleaseTestScenario -eq 'build-failure' -or ($global:dxxReleaseTestScenario -eq 'rerun-build-failure' -and $global:dxxReleaseTestBuildNumber -gt 2)) {
    $global:LASTEXITCODE = 1; return
}
if ($global:dxxReleaseTestScenario -eq 'became-immutable') { $global:dxxReleaseTestImmutable = $true }
if ($global:dxxReleaseTestScenario -eq 'native-warning') {
    if ([IO.Path]::DirectorySeparatorChar -eq '\') {
        & $env:ComSpec /c 'echo fixture tool warning 1>&2'
    } else {
        & /bin/sh -c 'echo fixture tool warning >&2'
    }
}
$root = Split-Path $PSScriptRoot
$output = Join-Path $root 'app/build/outputs/apk/release'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Set-Content (Join-Path $output 'app-release.apk') "fixture APK build $global:dxxReleaseTestBuildNumber legacy=$global:dxxReleaseTestBuildingLegacy"
if ($global:dxxReleaseTestScenario -eq 'legacy-source-changed' -and $global:dxxReleaseTestBuildingLegacy) { Set-Content (Join-Path (Split-Path $root) 'source.txt') 'changed during legacy build' }
if ($global:dxxReleaseTestScenario -eq 'source-changed') { Set-Content (Join-Path (Split-Path $root) 'source.txt') 'changed during build' }
if ($global:dxxReleaseTestScenario -eq 'source-staged') {
    Set-Content (Join-Path (Split-Path $root) 'source.txt') 'changed during build'
    & git -C (Split-Path $root) add source.txt
}
if ($global:dxxReleaseTestScenario -eq 'helper-changed') { Add-Content (Join-Path $root 'release-github.ps1') '# Edited while Gradle runs' }
if ($global:dxxReleaseTestScenario -eq 'head-changed') {
    Set-Content (Join-Path (Split-Path $root) 'source.txt') 'changed during build'
    & git -C (Split-Path $root) add source.txt
    & git -C (Split-Path $root) -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m 'change during build'
}
'@
$signerFixture = @'
$global:LASTEXITCODE = 0
if ($global:dxxReleaseTestScenario -eq 'unsigned' -or ($global:dxxReleaseTestScenario -eq 'rerun-unsigned' -and $global:dxxReleaseTestBuildNumber -gt 2)) {
    $global:LASTEXITCODE = 1; return
}
if ($global:dxxReleaseTestScenario -eq 'native-warning') {
    if ([IO.Path]::DirectorySeparatorChar -eq '\') {
        & $env:ComSpec /c 'echo fixture signing warning 1>&2'
    } else {
        & /bin/sh -c 'echo fixture signing warning >&2'
    }
}
$prefix = if ($global:dxxReleaseTestScenario -eq 'draft') { 'Signer #1' } else { 'V2 Signer:' }
$certificate = if ($global:dxxReleaseTestScenario -eq 'upload-only-pair-certificate' -and $args[-1] -like '*-legacy-universal.apk') { 'b' * 64 } else { 'a' * 64 }
"$prefix certificate SHA-256 digest: $certificate"
'@
$aaptFixture = @'
$global:LASTEXITCODE = 0
$legacyApk = $args[-1] -like '*-legacy-universal.apk' -or $args[-1] -like '*-only-for-android-*'
if ($args[-1] -like '*app-release.apk') { $legacyApk = $global:dxxReleaseTestBuildingLegacy }
$package = if ($global:dxxReleaseTestScenario -eq 'wrong-package') { 'com.dxxredux.app' } else { 'com.dxxredux.app.github' }
if ($legacyApk) { $package = 'com.dxxredux.app.github.legacy' }
$code = if ($global:dxxReleaseTestScenario -eq 'wrong-version') { 999 } else { $global:dxxReleaseTestVersionCode }
"package: name='$package' versionCode='$code' versionName='$global:dxxReleaseTestVersion'"
if ($global:dxxReleaseTestScenario -ne 'sdk-missing') {
    $minimum = if ($global:dxxReleaseTestScenario -eq 'wrong-min-sdk') { 26 } elseif ($legacyApk) { 23 } else { 24 }
    $target = if ($global:dxxReleaseTestScenario -eq 'wrong-target-sdk') { 35 } else { 36 }
    $minimumField = if ($global:dxxReleaseTestScenario -eq 'native-warning') { 'sdkVersion' } else { 'minSdkVersion' }
    "$minimumField`:'$minimum'"
    "targetSdkVersion:'$target'"
}
if ($global:dxxReleaseTestScenario -eq 'debuggable') { 'application-debuggable' }
if ($global:dxxReleaseTestScenario -eq 'missing-abi') { "native-code: 'arm64-v8a'" } else { "native-code: 'armeabi-v7a' 'arm64-v8a' 'x86_64'" }
'@

$cases = @('native-warning', 'publish', 'draft', 'build-only', 'dirty', 'unpushed', 'orphan-tag', 'existing-release',
    'existing-draft', 'immutable-release', 'older-draft-target', 'release-api-failure', 'replace-failure', 'notes-update-failure', 'cleanup-failure', 'became-immutable',
    'older-tag', 'older-annotated-tag', 'current-annotated-tag', 'prefix-tag', 'tag-update-failure', 'tag-create-failure', 'tag-verification-failure', 'retarget-failure',
    'rerun', 'rerun-new-commit', 'rerun-draft', 'rerun-build-only', 'rerun-build-failure', 'rerun-unsigned', 'rerun-upload-failure',
    'api-failure', 'clean-failure', 'build-failure', 'unsigned', 'wrong-package', 'wrong-version', 'debuggable', 'missing-abi', 'source-changed', 'upload-failure',
    'helper-dirty', 'helper-changed', 'source-staged', 'head-changed', 'commit-api-failure',
    'upload-only', 'upload-only-new-head', 'upload-only-dirty', 'upload-only-tampered', 'upload-only-checksum',
    'upload-only-signature', 'upload-only-version', 'upload-only-unverified', 'upload-only-legacy', 'upload-only-legacy-changed',
    'upload-only-missing', 'upload-only-unpushed', 'upload-only-conflict', 'upload-only-notes', 'upload-only-replace', 'upload-only-dirty-build',
    'legacy-edition-build-only', 'legacy-build-failure', 'legacy-source-changed', 'combined-migration', 'draft-migration', 'legacy-retirement-failure', 'uploaded-hash-mismatch',
    'upload-only-pair-commit', 'upload-only-pair-certificate', 'upload-only-legacy-apk-tampered',
    'sdk-missing', 'wrong-min-sdk', 'wrong-target-sdk', 'upload-only-sdk', 'upload-only-distribution')
foreach ($global:dxxReleaseTestScenario in $cases) {
    $global:dxxReleaseTestLegacy = $global:dxxReleaseTestScenario -like '*legacy-edition*'
    $global:dxxReleaseTestMinSdk = if ($global:dxxReleaseTestLegacy) { 23 } else { 24 }
    $androidVersion = if ($global:dxxReleaseTestLegacy) { '6.0' } else { '7.0' }
    $global:dxxReleaseTestSdkLabel = "minsdk: api $global:dxxReleaseTestMinSdk (android $androidVersion), targetsdk: api 36 (android 16)"
    $global:dxxReleaseTestCalls = [Collections.Generic.List[string]]::new()
    $global:dxxReleaseTestBuildNumber = 0
    $global:dxxReleaseTestBuildingLegacy = $false
    $global:dxxReleaseTestSeparateLegacy = $global:dxxReleaseTestScenario -in @('combined-migration', 'draft-migration', 'legacy-retirement-failure', 'uploaded-hash-mismatch')
    $global:dxxReleaseTestHasRelease = $global:dxxReleaseTestScenario -in @('existing-release', 'existing-draft', 'immutable-release',
        'older-draft-target', 'release-api-failure', 'replace-failure', 'notes-update-failure', 'cleanup-failure', 'became-immutable', 'older-tag', 'older-annotated-tag',
        'current-annotated-tag', 'tag-update-failure', 'tag-verification-failure', 'retarget-failure', 'combined-migration', 'draft-migration', 'legacy-retirement-failure', 'uploaded-hash-mismatch')
    $global:dxxReleaseTestIsDraft = $global:dxxReleaseTestScenario -in @('existing-draft', 'older-draft-target', 'draft-migration')
    $global:dxxReleaseTestImmutable = $global:dxxReleaseTestScenario -eq 'immutable-release'
    $global:dxxReleaseTestAssets = @{ 'unrelated.txt' = 'keep this asset' }
    $customNotes = 'Handwritten release notes: keep these'
    $global:dxxReleaseTestBody = "DXX-Redux 1.2.0 for Android (Android 7.0 or newer).`n`n- universal APK: includes ARM32, ARM64 and x86_64`n`nSource commit: $('0' * 40)`nAndroid versionCode: 1`nSigning certificate SHA-256: $('a' * 64)`n`n$customNotes"
    if ($global:dxxReleaseTestScenario -eq 'existing-draft') {
        $global:dxxReleaseTestBody = "<!-- dxx-redux-build:start -->`nOld generated details`n<!-- dxx-redux-build:end -->`n`n$customNotes"
    }
    if ($global:dxxReleaseTestScenario -eq 'older-tag') { $global:dxxReleaseTestBody = $customNotes }
    $fixture = Join-Path $testRoot $global:dxxReleaseTestScenario
    $androidDir = Join-Path $fixture 'android'
    foreach ($dir in @('helpers', 'get_deps', 'app/src/main/java/com/dxxredux/app')) {
        New-Item -ItemType Directory -Path (Join-Path $androidDir $dir) -Force | Out-Null
    }
    Copy-Item (Join-Path $repoRoot 'android/release-github.ps1') $androidDir
    Set-Content (Join-Path $androidDir 'helpers/test_host_platform.ps1') $helperFixture
    Set-Content (Join-Path $androidDir 'helpers/retain-recent-artifacts.ps1') 'param($Artifacts)'
    Set-Content (Join-Path $androidDir 'helpers/gradle.ps1') $gradleFixture
    Set-Content (Join-Path $androidDir 'helpers/apksigner.ps1') $signerFixture
    Set-Content (Join-Path $androidDir 'helpers/aapt2.ps1') $aaptFixture
    Set-Content (Join-Path $androidDir 'get_deps/tool_versions.conf') 'BUILD_TOOLS_VERSION=37.0.0'
    Set-Content (Join-Path $androidDir 'distribution_versions.conf') "CURRENT_MIN_SDK=24`nCURRENT_TARGET_SDK=36`nLEGACY_MIN_SDK=23`nLEGACY_TARGET_SDK=36"
    Set-Content (Join-Path $androidDir 'keystore.properties') '# No real credentials'
    Set-Content (Join-Path $fixture 'source.txt') 'original source'
    $notesText = 'Release notes: caf' + [char]0xE9 + ' ' + [char]0x65E5
    [IO.File]::WriteAllText((Join-Path $fixture 'notes.md'), $notesText, [Text.UTF8Encoding]::new($false))
    Set-Content (Join-Path $fixture '.gitignore') "android/build-outputs/`nandroid/app/build/`nBuildInfo.kt"
    & git -C $fixture init --quiet
    & git -C $fixture config core.autocrlf false
    & git -C $fixture add .
    & git -C $fixture -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m fixture
    Assert-Test ($LASTEXITCODE -eq 0) 'Cannot create Git fixture'
    & git -C $fixture remote add origin https://github.com/fixture/test.git
    $global:dxxReleaseTestCommit = (& git -C $fixture rev-parse HEAD).Trim()
    $oldCommit = $global:dxxReleaseTestCommit
    if ($global:dxxReleaseTestScenario -in @('orphan-tag', 'older-tag', 'older-annotated-tag', 'older-draft-target', 'tag-update-failure', 'tag-verification-failure', 'retarget-failure')) {
        Set-Content (Join-Path $fixture 'source.txt') 'new release source'
        & git -C $fixture add source.txt
        & git -C $fixture -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m 'new source'
        Assert-Test ($LASTEXITCODE -eq 0) 'Cannot create updated source commit'
        $global:dxxReleaseTestCommit = (& git -C $fixture rev-parse HEAD).Trim()
    }
    $global:dxxReleaseTestVersionCode = [int](& git -C $fixture rev-list --count HEAD) * 10
    $global:dxxReleaseTestTarget = $oldCommit
    $global:dxxReleaseTestRemote = Join-Path $testRoot ($global:dxxReleaseTestScenario + '.git')
    & git clone --bare --quiet $fixture $global:dxxReleaseTestRemote
    Assert-Test ($LASTEXITCODE -eq 0) 'Cannot create fake GitHub Git repository'
    if ($global:dxxReleaseTestScenario -eq 'dirty') { Set-Content (Join-Path $fixture 'untracked.txt') 'new source' }
    if ($global:dxxReleaseTestScenario -eq 'helper-dirty') { Add-Content (Join-Path $androidDir 'release-github.ps1') '# Local release helper edit' }
    $global:dxxReleaseTestVersion = if ($global:dxxReleaseTestScenario -eq 'draft') { '1.2.0-rc.1' } else { '1.2.0' }
    $global:dxxReleaseTestTag = "android-v$global:dxxReleaseTestVersion"
    $global:dxxReleaseTestRecommendedName = "dxx-revival-$global:dxxReleaseTestVersion-android-1-recommended-universal.apk"
    $global:dxxReleaseTestCombinedLegacyName = "dxx-revival-$global:dxxReleaseTestVersion-android-2-only-for-android-6.0-universal.apk"
    $global:dxxReleaseTestOldCombinedLegacyName = "dxx-redux-$global:dxxReleaseTestVersion-android-only-for-android-6.0-universal.apk"
    $global:dxxReleaseTestApkName = if ($global:dxxReleaseTestLegacy) { "dxx-redux-$global:dxxReleaseTestVersion-android-legacy-universal.apk" } else { "dxx-redux-$global:dxxReleaseTestVersion-android-universal.apk" }
    $remoteTag = "refs/tags/$global:dxxReleaseTestTag"
    if (($global:dxxReleaseTestHasRelease -and -not $global:dxxReleaseTestIsDraft) -or $global:dxxReleaseTestScenario -eq 'orphan-tag') {
        & git -C $global:dxxReleaseTestRemote update-ref $remoteTag $oldCommit
    }
    if ($global:dxxReleaseTestScenario -in @('older-annotated-tag', 'current-annotated-tag')) {
        & git -C $global:dxxReleaseTestRemote -c user.name=ReleaseTest -c user.email=release@example.invalid -c tag.gpgsign=false tag -a -f $global:dxxReleaseTestTag $oldCommit -m 'annotated release'
    }
    if ($global:dxxReleaseTestScenario -eq 'prefix-tag') {
        & git -C $global:dxxReleaseTestRemote update-ref ($remoteTag + '-other') $oldCommit
    }
    $parameters = @{ Version = $global:dxxReleaseTestVersion; NotesFile = (Join-Path $fixture 'notes.md') }
    if ($global:dxxReleaseTestLegacy) { $parameters.Legacy = $true }
    if ($global:dxxReleaseTestScenario -in @('build-only', 'rerun-build-only', 'legacy-edition-build-only')) { $parameters.BuildOnly = $true }
    if ($global:dxxReleaseTestScenario -in @('draft', 'rerun-draft')) { $parameters.Draft = $true }
    # Preseed old local and remote assets to verify replacement without deleting unrelated files
    $outputTag = if ($global:dxxReleaseTestLegacy) { "android-legacy-v$global:dxxReleaseTestVersion" } else { $global:dxxReleaseTestTag }
    $out = Join-Path $androidDir "build-outputs/github/$outputTag"
    $global:dxxReleaseTestOutput = $out
    if ($global:dxxReleaseTestHasRelease) {
        New-Item -ItemType Directory -Path $out -Force | Out-Null
        foreach ($name in @($global:dxxReleaseTestApkName, $global:dxxReleaseTestOldCombinedLegacyName,
                "dxx-redux-$global:dxxReleaseTestVersion-android-1-recommended-universal.apk", "dxx-redux-$global:dxxReleaseTestVersion-android-2-only-for-android-6.0-universal.apk",
                'SHA256SUMS.txt', 'build-info.json', 'build.json', 'unrelated.txt')) {
            [IO.File]::WriteAllText((Join-Path $out $name), 'old ' + $name)
            $global:dxxReleaseTestAssets[$name] = 'old ' + $name
        }
    }
    $unrelatedAsset = $global:dxxReleaseTestAssets['unrelated.txt']
    if ($global:dxxReleaseTestScenario -like 'rerun*') {
        $firstFailure = $null
        try { & (Join-Path $androidDir 'release-github.ps1') @parameters } catch { $firstFailure = $_ }
        Assert-Test (($null -ne $firstFailure) -eq ($global:dxxReleaseTestScenario -eq 'rerun-upload-failure')) "Unexpected first-run result: $firstFailure"
        $previousHash = (Get-FileHash (Join-Path $out $global:dxxReleaseTestApkName)).Hash
        # Omitting Draft on a rerun must not publish the existing draft
        $parameters.Remove('Draft')
        if ($global:dxxReleaseTestScenario -eq 'rerun-new-commit') {
            Set-Content (Join-Path $fixture 'source.txt') 'updated between release runs'
            & git -C $fixture add source.txt
            & git -C $fixture -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m 'update release'
            Assert-Test ($LASTEXITCODE -eq 0) 'Cannot commit between release runs'
            $global:dxxReleaseTestCommit = (& git -C $fixture rev-parse HEAD).Trim()
            $global:dxxReleaseTestVersionCode = [int](& git -C $fixture rev-list --count HEAD) * 10
            & git -C $fixture push --quiet $global:dxxReleaseTestRemote HEAD:refs/heads/main
            Assert-Test ($LASTEXITCODE -eq 0) 'Cannot push updated fixture source'
        }
    }
    if ($global:dxxReleaseTestScenario -like 'upload-only*') {
        # Prepare saved output through the real script, then attempt an upload with no Gradle call
        if ($global:dxxReleaseTestScenario -eq 'upload-only-dirty-build') {
            Set-Content (Join-Path $fixture 'source.txt') 'dirty source used for BuildOnly'
        }
        $setupParameters = @{ Version = $global:dxxReleaseTestVersion; NotesFile = (Join-Path $fixture 'notes.md'); BuildOnly = $true }
        if ($global:dxxReleaseTestLegacy) { $setupParameters.Legacy = $true }
        & (Join-Path $androidDir 'release-github.ps1') @setupParameters
        $parameters.Remove('NotesFile')
        $parameters.UploadOnly = $true
        $previousHash = (Get-FileHash (Join-Path $out $global:dxxReleaseTestApkName)).Hash
        $global:dxxReleaseTestCalls.Clear()
        $savedPath = Join-Path $out 'build-info.json'
        $saved = Get-Content $savedPath -Raw | ConvertFrom-Json
        switch ($global:dxxReleaseTestScenario) {
            'upload-only-new-head' {
                Set-Content (Join-Path $fixture 'source.txt') 'different source after completed build'
                & git -C $fixture add source.txt
                & git -C $fixture -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m 'later work'
            }
            'upload-only-dirty' { Set-Content (Join-Path $fixture 'source.txt') 'uncommitted work after completed build' }
            'upload-only-tampered' { Add-Content (Join-Path $out $global:dxxReleaseTestApkName) 'changed' }
            'upload-only-checksum' { Set-Content (Join-Path $out 'SHA256SUMS.txt') 'wrong checksum' }
            'upload-only-signature' { $saved.certificateSha256 = 'b' * 64 }
            'upload-only-sdk' { $saved.minSdk = 26 }
            'upload-only-distribution' { $saved.distribution = 'play' }
            'upload-only-version' { $parameters.VersionCode = 999 }
            'upload-only-unverified' { $saved.sourceVerified = $false }
            'upload-only-dirty-build' { & git -C $fixture restore source.txt }
            'upload-only-legacy' {
                $saved.PSObject.Properties.Remove('sourceVerified')
                $saved.sourceClean = $false
                Add-Content (Join-Path $androidDir 'release-github.ps1') '# Comment change that previously blocked publication'
                & git -C $fixture add android/release-github.ps1
                & git -C $fixture -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m 'helper comment'
                Add-Content (Join-Path $androidDir 'release-github.ps1') '# Further helper-only change'
            }
            'upload-only-legacy-changed' {
                $saved.PSObject.Properties.Remove('sourceVerified')
                Set-Content (Join-Path $fixture 'source.txt') 'different source'
                & git -C $fixture add source.txt
                & git -C $fixture -c user.name=ReleaseTest -c user.email=release@example.invalid -c commit.gpgsign=false commit --quiet -m 'app changes'
            }
            'upload-only-missing' { Remove-Item -LiteralPath (Join-Path $out 'release-notes.md') }
            'upload-only-conflict' { $parameters.BuildOnly = $true }
            'upload-only-notes' { $parameters.NotesFile = (Join-Path $fixture 'notes.md') }
            'upload-only-replace' { $global:dxxReleaseTestHasRelease = $true }
        }
        [IO.File]::WriteAllText($savedPath, ($saved | ConvertTo-Json) + "`n")
        $legacyOutput = Join-Path $androidDir "build-outputs/github/android-legacy-v$global:dxxReleaseTestVersion"
        if ($global:dxxReleaseTestScenario -in @('upload-only-pair-commit', 'upload-only-pair-certificate')) {
            $legacySavedPath = Join-Path $legacyOutput 'build-info.json'
            $legacySaved = Get-Content $legacySavedPath -Raw | ConvertFrom-Json
            if ($global:dxxReleaseTestScenario -eq 'upload-only-pair-commit') { $legacySaved.commit = '0' * 40 }
            if ($global:dxxReleaseTestScenario -eq 'upload-only-pair-certificate') { $legacySaved.certificateSha256 = 'b' * 64 }
            [IO.File]::WriteAllText($legacySavedPath, ($legacySaved | ConvertTo-Json) + "`n")
        }
        if ($global:dxxReleaseTestScenario -eq 'upload-only-legacy-apk-tampered') {
            Add-Content (Join-Path $legacyOutput "dxx-redux-$global:dxxReleaseTestVersion-android-legacy-universal.apk") 'changed'
        }
    }
    $failure = $null
    try { & (Join-Path $androidDir 'release-github.ps1') @parameters } catch { $failure = $_ }
    $successExpected = $global:dxxReleaseTestScenario -in @('native-warning', 'publish', 'draft', 'build-only',
        'existing-release', 'existing-draft', 'orphan-tag', 'older-tag', 'older-annotated-tag', 'current-annotated-tag', 'prefix-tag',
        'older-draft-target', 'rerun', 'rerun-new-commit', 'rerun-draft', 'rerun-build-only', 'rerun-upload-failure',
        'helper-dirty', 'helper-changed', 'upload-only', 'upload-only-new-head', 'upload-only-dirty', 'upload-only-legacy', 'upload-only-replace',
        'legacy-edition-build-only', 'combined-migration', 'draft-migration')
    Assert-Test (($null -eq $failure) -eq $successExpected) "Unexpected result for $global:dxxReleaseTestScenario`: $failure"
    if (-not $successExpected) {
        $expectedFailure = switch ($global:dxxReleaseTestScenario) {
            { $_ -in @('dirty', 'source-changed', 'source-staged', 'legacy-source-changed') } { 'clean working tree' }
            { $_ -in @('unpushed', 'upload-only-unpushed') } { 'Push the branch' }
            'commit-api-failure' { 'check gh auth status' }
            { $_ -in @('api-failure', 'release-api-failure') } { 'gh failed' }
            'head-changed' { 'HEAD changed during the build' }
            { $_ -in @('upload-only-tampered', 'upload-only-checksum', 'upload-only-signature', 'upload-only-legacy-apk-tampered') } { 'Saved APK/hash/signing metadata does not match' }
            { $_ -in @('upload-only-pair-commit', 'upload-only-pair-certificate') } { 'must have the same source commit' }
            'uploaded-hash-mismatch' { 'Uploaded APK verification failed' }
            'legacy-retirement-failure' { 'retiring android-legacy-v' }
            'upload-only-version' { '-VersionCode differs' }
            { $_ -in @('upload-only-unverified', 'upload-only-dirty-build') } { 'source validation did not pass' }
            'upload-only-legacy-changed' { 'Cannot validate app source' }
            'upload-only-missing' { 'Saved release file missing' }
            'upload-only-conflict' { 'cannot be combined' }
            'upload-only-notes' { 'uses saved release notes' }
            { $_ -in @('immutable-release', 'became-immutable') } { 'is immutable' }
            { $_ -in @('clean-failure', 'build-failure', 'rerun-build-failure', 'legacy-build-failure') } { 'gradle.ps1 failed' }
            { $_ -in @('unsigned', 'rerun-unsigned') } { 'apksigner.ps1 failed' }
            { $_ -in @('wrong-package', 'wrong-version') } { 'package or version' }
            'debuggable' { 'debuggable APK' }
            'missing-abi' { 'missing armeabi-v7a' }
            'sdk-missing' { 'minimum SDK is missing' }
            { $_ -in @('wrong-min-sdk', 'wrong-target-sdk') } { 'SDK versions do not match' }
            { $_ -in @('upload-only-sdk', 'upload-only-distribution') } { 'SDK/distribution metadata does not match' }
            { $_ -in @('upload-failure', 'replace-failure', 'notes-update-failure', 'cleanup-failure', 'tag-update-failure', 'tag-create-failure', 'retarget-failure') } { 'Release tag/upload/publish failed' }
            'tag-verification-failure' { 'does not resolve to the built commit' }
        }
        Assert-Test ($failure.ToString().Contains($expectedFailure)) "Wrong failure for $global:dxxReleaseTestScenario`: $failure"
    }
    $creates = @($global:dxxReleaseTestCalls | Where-Object { $_ -like 'release create *' })
    $publishes = @($global:dxxReleaseTestCalls | Where-Object { $_ -like 'release edit *--draft=false*' })
    $replaces = @($global:dxxReleaseTestCalls | Where-Object { $_ -like 'release upload *' })
    if (-not $successExpected -and $global:dxxReleaseTestScenario -notlike 'rerun*' -and
        $global:dxxReleaseTestScenario -notin @('upload-failure', 'replace-failure', 'notes-update-failure', 'cleanup-failure', 'tag-update-failure', 'tag-create-failure', 'tag-verification-failure', 'retarget-failure', 'uploaded-hash-mismatch', 'legacy-retirement-failure')) {
        Assert-Test (@($global:dxxReleaseTestCalls | Where-Object { $_ -like 'api *--method *' }).Count -eq 0) 'Failed build or validation mutated a remote tag'
    }
    if ($global:dxxReleaseTestScenario -eq 'clean-failure') {
        Assert-Test ($global:dxxReleaseTestBuildNumber -eq 0) 'Assemble ran after clean failed'
    }
    Assert-Test ($publishes.Count -eq [int]($global:dxxReleaseTestScenario -in @('native-warning', 'publish', 'orphan-tag', 'prefix-tag', 'rerun', 'rerun-new-commit', 'rerun-build-failure', 'rerun-unsigned',
                'helper-dirty', 'helper-changed', 'upload-only', 'upload-only-new-head', 'upload-only-dirty', 'upload-only-legacy'))) "Incorrect publication for $global:dxxReleaseTestScenario"
    Assert-Test ($creates.Count -eq [int]($global:dxxReleaseTestScenario -in @('native-warning', 'publish', 'draft', 'upload-failure', 'orphan-tag', 'prefix-tag', 'rerun', 'rerun-new-commit', 'rerun-draft', 'rerun-build-failure', 'rerun-unsigned', 'rerun-upload-failure',
                'helper-dirty', 'helper-changed', 'upload-only', 'upload-only-new-head', 'upload-only-dirty', 'upload-only-legacy'))) "Incorrect upload for $global:dxxReleaseTestScenario"
    Assert-Test ($replaces.Count -eq [int]($global:dxxReleaseTestScenario -in @('existing-release', 'existing-draft', 'older-tag', 'older-annotated-tag', 'current-annotated-tag', 'older-draft-target', 'replace-failure', 'notes-update-failure', 'cleanup-failure', 'rerun', 'rerun-new-commit', 'rerun-draft', 'rerun-upload-failure', 'upload-only-replace', 'combined-migration', 'draft-migration', 'legacy-retirement-failure', 'uploaded-hash-mismatch'))) "Incorrect replacement for $global:dxxReleaseTestScenario"
    if ($global:dxxReleaseTestScenario -in @('replace-failure', 'notes-update-failure')) {
        Assert-Test (@($global:dxxReleaseTestCalls | Where-Object { $_ -like 'release delete-asset *' }).Count -eq 0) 'Failed APK/notes replacement deleted old attachments'
    }
    if ($global:dxxReleaseTestScenario -like 'upload-only*') {
        Assert-Test ($global:dxxReleaseTestBuildNumber -eq 2 -and @($global:dxxReleaseTestCalls | Where-Object { $_ -like 'gradle *' }).Count -eq 0) 'UploadOnly rebuilt an APK'
        if ($successExpected) {
            Assert-Test ((Get-FileHash (Join-Path $out $global:dxxReleaseTestApkName)).Hash -eq $previousHash) 'UploadOnly changed the APK'
        }
    }
    if ($global:dxxReleaseTestScenario -in @('dirty', 'unpushed', 'commit-api-failure')) {
        Assert-Test ($global:dxxReleaseTestBuildNumber -eq 0) 'Preflight failure started a build'
    }
    if ($global:dxxReleaseTestScenario -in @('dirty', 'source-changed', 'source-staged')) {
        Assert-Test ($failure.ToString() -match 'source.txt|untracked.txt') 'Dirty-source error omitted the offending file'
    }
    Assert-Test ($global:dxxReleaseTestAssets['unrelated.txt'] -eq $unrelatedAsset) 'Unrelated release asset changed'
    if ($global:dxxReleaseTestScenario -in @('existing-release', 'older-tag', 'older-annotated-tag', 'current-annotated-tag', 'replace-failure', 'rerun', 'rerun-new-commit', 'rerun-build-failure', 'rerun-unsigned')) {
        Assert-Test (-not $global:dxxReleaseTestIsDraft) 'Published release was changed to a draft'
    }
    if ($global:dxxReleaseTestScenario -in @('existing-draft', 'older-draft-target', 'rerun-draft', 'rerun-upload-failure')) {
        Assert-Test $global:dxxReleaseTestIsDraft 'Existing draft was published on a rerun'
    }
    if ($global:dxxReleaseTestScenario -in @('rerun-build-failure', 'rerun-unsigned')) {
        Assert-Test ((Get-FileHash (Join-Path $out $global:dxxReleaseTestApkName)).Hash -eq $previousHash) 'Failed build or verification overwrote the previous verified APK'
        Assert-Test ($global:dxxReleaseTestAssets[$global:dxxReleaseTestRecommendedName] -eq [IO.File]::ReadAllText((Join-Path $out $global:dxxReleaseTestApkName))) 'Failed build or verification changed the remote APK'
    }
    if ($global:dxxReleaseTestScenario -in @('build-only', 'rerun-build-only', 'legacy-edition-build-only')) {
        Assert-Test (@($global:dxxReleaseTestCalls | Where-Object { $_ -notlike 'gradle *' }).Count -eq 0) 'BuildOnly contacted GitHub'
    }
    if ($global:dxxReleaseTestScenario -in @('tag-update-failure', 'tag-verification-failure', 'immutable-release', 'became-immutable', 'rerun-build-failure', 'rerun-unsigned')) {
        Assert-Test ((& git -C $global:dxxReleaseTestRemote rev-parse "$remoteTag^{commit}") -eq $oldCommit) 'Tag changed despite a failed build, validation or tag update'
    }
    if ($global:dxxReleaseTestScenario -eq 'current-annotated-tag') {
        Assert-Test ((& git -C $global:dxxReleaseTestRemote cat-file -t $remoteTag) -eq 'tag') 'Unchanged annotated tag was replaced'
    }
    if ($global:dxxReleaseTestScenario -eq 'prefix-tag') {
        Assert-Test ((& git -C $global:dxxReleaseTestRemote rev-parse "$remoteTag-other^{commit}") -eq $oldCommit) 'Unrelated prefix tag was moved'
    }
    if ($successExpected) {
        $info = Get-Content (Join-Path $out 'build-info.json') -Raw | ConvertFrom-Json
        $apk = Get-Item (Join-Path $out $global:dxxReleaseTestApkName)
        Assert-Test ($info.apkSha256 -eq (Get-FileHash $apk.FullName).Hash.ToLowerInvariant()) 'Incorrect checksum'
        Assert-Test ($info.commit -eq $global:dxxReleaseTestCommit) 'Incorrect source metadata'
        Assert-Test ($info.versionCode -eq $global:dxxReleaseTestVersionCode) 'Incorrect versionCode after source update'
        Assert-Test ($info.minSdk -eq $global:dxxReleaseTestMinSdk -and $info.targetSdk -eq 36) 'Incorrect APK SDK metadata'
        Assert-Test ($info.distribution -eq $(if ($global:dxxReleaseTestLegacy) { 'legacy' } else { 'github' })) 'Incorrect distribution metadata'
        Assert-Test ([IO.File]::ReadAllText((Join-Path $out 'release-notes.md')).Contains($global:dxxReleaseTestSdkLabel)) 'Release notes lack the inspected SDK label'
        if (-not $parameters.BuildOnly) {
            Assert-Test ((& git -C $global:dxxReleaseTestRemote rev-parse "$remoteTag^{commit}") -eq $info.commit) 'Release tag and APK source metadata disagree'
        }
        Assert-Test ([IO.File]::ReadAllText((Join-Path $out 'release-notes.md')).Contains($notesText)) 'Release notes lost UTF-8 characters'
        if ($global:dxxReleaseTestScenario -like 'rerun*') {
            Assert-Test ($global:dxxReleaseTestBuildNumber -eq 4 -and (Get-FileHash $apk.FullName).Hash -ne $previousHash) 'Same-version rerun did not produce a fresh APK pair'
        }
        if ($replaces.Count -gt 0) {
            Assert-Test ($global:dxxReleaseTestAssets[$global:dxxReleaseTestRecommendedName] -eq [IO.File]::ReadAllText($apk.FullName)) 'Release APK was not replaced'
            foreach ($name in @('SHA256SUMS.txt', 'build-info.json', 'build.json')) {
                Assert-Test (-not $global:dxxReleaseTestAssets.ContainsKey($name)) "Obsolete release asset remains: $name"
            }
            Assert-Test ($global:dxxReleaseTestBody.Contains($customNotes) -or $global:dxxReleaseTestScenario -like 'rerun*') 'Custom notes were lost'
        }
        if (-not $parameters.BuildOnly) {
            Assert-Test ($global:dxxReleaseTestAssets.ContainsKey($global:dxxReleaseTestRecommendedName)) 'Combined release lacks the recommended APK'
            Assert-Test ($global:dxxReleaseTestAssets.ContainsKey($global:dxxReleaseTestCombinedLegacyName)) 'Combined release lacks the Android 6.0 APK'
            $sortedApks = @($global:dxxReleaseTestAssets.Keys | Where-Object { $_ -like '*.apk' } | Sort-Object)
            Assert-Test ($sortedApks.Count -eq 2 -and $sortedApks[0] -eq $global:dxxReleaseTestRecommendedName -and
                $sortedApks[1] -eq $global:dxxReleaseTestCombinedLegacyName) 'Recommended APK must sort first with no obsolete APK names'
            Assert-Test ($global:dxxReleaseTestBody.Contains("Download: $global:dxxReleaseTestRecommendedName") -and
                $global:dxxReleaseTestBody.Contains("Download: $global:dxxReleaseTestCombinedLegacyName")) 'Release notes lack the public filenames'
            Assert-Test ($global:dxxReleaseTestBody.Contains('this is a special build to go one step lower on minimum android version (to 6.0). On Android 7.0 or newer, use the recommended APK instead')) 'Legacy usage guidance is missing'
            Assert-Test (([regex]::Matches($global:dxxReleaseTestBody, 'APK SHA-256:')).Count -eq 2) 'Expected one hash for each APK'
            Assert-Test ($global:dxxReleaseTestBody.Contains("Source commit: $($info.commit)")) 'Remote notes have a stale commit'
            Assert-Test ($global:dxxReleaseTestBody.Contains("Android versionCode: $($info.versionCode)")) 'Remote notes have a stale versionCode'
            Assert-Test ($global:dxxReleaseTestBody.Contains("APK SHA-256: $($info.apkSha256)")) 'Remote notes lack the verified APK hash'
            Assert-Test (([regex]::Matches($global:dxxReleaseTestBody, 'Source commit:')).Count -eq 1) 'Remote notes duplicate build details'
            foreach ($name in @('SHA256SUMS.txt', 'build-info.json', 'build.json')) {
                Assert-Test (-not $global:dxxReleaseTestAssets.ContainsKey($name)) "Metadata/checksums were uploaded: $name"
            }
        }
        if ($global:dxxReleaseTestScenario -eq 'draft') { Assert-Test ($creates[0].Contains('--prerelease')) 'Missing prerelease flag' }
    }
    if ($global:dxxReleaseTestScenario -eq 'combined-migration') { Assert-Test (-not $global:dxxReleaseTestSeparateLegacy) 'Separate legacy release was not retired' }
    if ($global:dxxReleaseTestScenario -eq 'draft-migration') { Assert-Test $global:dxxReleaseTestSeparateLegacy 'Draft migration deleted a published legacy release' }
    if ($global:dxxReleaseTestScenario -eq 'uploaded-hash-mismatch') { Assert-Test $global:dxxReleaseTestSeparateLegacy 'Unverified migration retired the old legacy release' }
    if ($global:dxxReleaseTestScenario -in @('notes-update-failure', 'cleanup-failure', 'legacy-retirement-failure')) {
        $failedScenario = $global:dxxReleaseTestScenario
        $global:dxxReleaseTestScenario = 'upload-only-replace'
        $parameters.Remove('NotesFile')
        $parameters.UploadOnly = $true
        $global:dxxReleaseTestCalls.Clear()
        & (Join-Path $androidDir 'release-github.ps1') @parameters
        Assert-Test (@($global:dxxReleaseTestCalls | Where-Object { $_ -like 'gradle *' }).Count -eq 0) 'Partial failure recovery rebuilt the APK'
        Assert-Test ($global:dxxReleaseTestBody.Contains($customNotes)) 'Partial failure recovery lost custom notes'
        foreach ($name in @('SHA256SUMS.txt', 'build-info.json', 'build.json')) {
            Assert-Test (-not $global:dxxReleaseTestAssets.ContainsKey($name)) "Partial failure recovery left obsolete asset: $name"
        }
        $global:dxxReleaseTestScenario = $failedScenario
    }
    Write-Host "PASS $global:dxxReleaseTestScenario"
}
Write-Host "Passed $($cases.Count) release integration scenarios on PowerShell $($PSVersionTable.PSVersion)"
# The final failure scenario deliberately leaves a nonzero native exit code
exit 0
