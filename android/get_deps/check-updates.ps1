#!/usr/bin/env pwsh
# check-updates.ps1 -- Check for newer versions of Android build dependencies
# and offer to upgrade them. Updates tool_versions.conf and related metadata in place.
#
# Checks: AGP, Gradle, Kotlin, Compose Compiler, Compose BOM, AndroidX libs,
#          Android SDK cmdline-tools/build-tools/CMake, Android NDK, JDK,
#          DOSBox-X, code quality tools, networking libs, archive tools,
#          GM soundfont, Chromaprint, and related helper packages
#
# Usage:  .\check-updates.ps1
#         .\check-updates.ps1 -TargetSelection a
#         .\check-updates.ps1 -TargetSelection 1,2,5
#         .\check-updates.ps1 -InstallSelection 1,2

param(
    [Alias("Selection")]
    [string]$TargetSelection,
    [string]$InstallSelection,
    [switch]$NoPrompt,
    [string]$RunNotePath
)

$ErrorActionPreference = "Stop"
# Suppress per-request progress UI, which can overwhelm the VS Code terminal
# when this script fans out across many web requests.
$ProgressPreference = "SilentlyContinue"

$scriptDir = $PSScriptRoot
$androidDir = Split-Path $scriptDir -Parent
$repoRoot = Split-Path $androidDir -Parent
$confFile = Join-Path $scriptDir "tool_versions.conf"
$platformHelper = Join-Path (Join-Path $scriptDir "helpers") "Get-DepPlatform.ps1"
. $platformHelper
$script:hostPlatform = Get-HostPlatform
$script:checkUpdatesInvocation = Get-CheckUpdatesInvocation
$runningInVsCodeTerminal = $env:TERM_PROGRAM -eq "vscode"
if (-not $RunNotePath) {
    $RunNotePath = Join-Path $androidDir "temp/check-updates.last-run.txt"
}

if ($TargetSelection -or $InstallSelection) {
    $NoPrompt = $true
} elseif ($runningInVsCodeTerminal) {
    # Avoid Read-Host inside the VS Code integrated terminal, which has been
    # prone to host lockups during this script's long-running network checks.
    $NoPrompt = $true
}

function Write-RunNote($status, $extraLines = @()) {
    $runMethod = if ($runningInVsCodeTerminal) {
        "VS Code terminal safe mode"
    } elseif ($NoPrompt) {
        "Noninteractive"
    } else {
        "Interactive prompt"
    }
    $commandParts = @("pwsh -File $script:checkUpdatesInvocation")
    if ($TargetSelection) {
        $commandParts += "-TargetSelection $TargetSelection"
    }
    if ($InstallSelection) {
        $commandParts += "-InstallSelection $InstallSelection"
    }
    if ($NoPrompt -and -not $TargetSelection -and -not $InstallSelection) {
        $commandParts += "-NoPrompt"
    }
    $noteLines = @(
        "timestamp=$([DateTimeOffset]::Now.ToString('o'))",
        "status=$status",
        "script=$PSCommandPath",
        "cwd=$((Get-Location).Path)",
        "ps_edition=$($PSVersionTable.PSEdition)",
        "ps_version=$($PSVersionTable.PSVersion)",
        "target_selection=$TargetSelection",
        "install_selection=$InstallSelection",
        "no_prompt=$NoPrompt",
        "term_program=$env:TERM_PROGRAM",
        "method=$runMethod",
        "command=$($commandParts -join ' ')"
    )
    if ($extraLines) {
        $noteLines += $extraLines
    }

    $noteDir = Split-Path $RunNotePath -Parent
    if ($noteDir -and -not (Test-Path $noteDir)) {
        New-Item -ItemType Directory -Path $noteDir -Force | Out-Null
    }
    ($noteLines -join "`r`n") + "`r`n" | Set-Content -Path $RunNotePath -NoNewline
}

Write-RunNote "started" @(
    "reason=record the exact invocation before network work begins so crashes leave a durable breadcrumb",
    "recommended_safe_command=pwsh -File $script:checkUpdatesInvocation -NoPrompt",
    "recommended_target_command=pwsh -File $script:checkUpdatesInvocation -TargetSelection 1,2,3",
    "recommended_install_command=pwsh -File $script:checkUpdatesInvocation -InstallSelection 1,2,3"
)

# -- Load tool_versions.conf --------------------------------------------------

. (Join-Path $scriptDir "helpers/safe_conf_value.ps1")

function Normalize-ConfValue($rawValue) {
    if ($null -eq $rawValue) { return $null }

    $builder = New-Object System.Text.StringBuilder
    $inSingle = $false
    $inDouble = $false

    foreach ($char in $rawValue.ToCharArray()) {
        if ($char -eq "'" -and -not $inDouble) {
            $inSingle = -not $inSingle
            [void]$builder.Append($char)
            continue
        }
        if ($char -eq '"' -and -not $inSingle) {
            $inDouble = -not $inDouble
            [void]$builder.Append($char)
            continue
        }
        if ($char -eq '#' -and -not $inSingle -and -not $inDouble) {
            break
        }
        [void]$builder.Append($char)
    }

    $value = $builder.ToString().Trim()
    if ($value.Length -ge 2) {
        $first = $value[0]
        $last = $value[$value.Length - 1]
        if (($first -eq "'" -and $last -eq "'") -or ($first -eq '"' -and $last -eq '"')) {
            $value = $value.Substring(1, $value.Length - 2)
        }
    }

    return $value.Trim()
}

function Load-Conf {
    $cfg = @{}
    foreach ($line in Get-Content $confFile) {
        $line = $line.Trim()
        if ($line -match '^([A-Z0-9_]+)=(.+)$') {
            $cfg[$Matches[1]] = Normalize-ConfValue $Matches[2]
        }
    }
    return $cfg
}

function Load-DependencyBase {
    return Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing
}

function Refresh-ConfContext {
    $script:conf = Load-Conf
    foreach ($key in $script:conf.Keys) {
        Set-Variable -Scope Script -Name $key -Value $script:conf[$key]
    }
    Set-Variable -Scope Script -Name dependency_base -Value $script:dependency_base
    Set-Variable -Scope Script -Name repo_root -Value $repoRoot
    Set-Variable -Scope Script -Name script_dir -Value $scriptDir
    Set-Variable -Scope Script -Name android_dir -Value $androidDir
}

$script:dependency_base = Load-DependencyBase
Refresh-ConfContext

# -- Helpers ------------------------------------------------------------------

function Test-IsPrereleaseVersion($version) {
    if (-not $version) { return $false }
    return $version -match '(?i)(?:^|[-._])(alpha|beta|preview|rc)\d*(?:$|[-._])'
}

function Get-PrereleaseRank($label) {
    switch ($label) {
        "alpha" { return 0 }
        "beta" { return 1 }
        "preview" { return 2 }
        "rc" { return 3 }
        default { return 4 }
    }
}

function Get-VersionInfo($version) {
    if (-not $version) { return $null }

    $normalized = $version.Trim()
    if ($normalized -match '^[0-9a-f]{12,}$') {
        return $null
    }

    if ($normalized -match '^[vV](.+)$') {
        $normalized = $Matches[1]
    }

    if ($normalized -match '^[rR](\d+)([a-z]?)$') {
        return @{
            Kind = "ndk"
            Core = @([int]$Matches[1])
            Suffix = $Matches[2].ToLower()
            PreRank = 3
            PreNumber = 0
        }
    }

    $core = $normalized
    $preLabel = $null
    $preNumber = 0
    if ($normalized -match '^(.*?)(?:[-._](alpha|beta|preview|rc)(?:[-._]?(\d+))?)$') {
        $core = $Matches[1]
        $preLabel = $Matches[2].ToLower()
        if ($Matches[3]) {
            $preNumber = [int]$Matches[3]
        }
    }

    $coreNumbers = [regex]::Matches($core, '\d+') | ForEach-Object { [int]$_.Value }
    if (-not $coreNumbers) { return $null }

    return @{
        Kind = "numeric"
        Core = $coreNumbers
        Suffix = ""
        PreRank = Get-PrereleaseRank $preLabel
        PreNumber = $preNumber
    }
}

function Compare-VersionStrings($left, $right) {
    if ($left -eq $right) { return 0 }
    if (-not $left -or -not $right) { return $null }

    $leftInfo = Get-VersionInfo $left
    $rightInfo = Get-VersionInfo $right
    if (-not $leftInfo -or -not $rightInfo) { return $null }
    if ($leftInfo.Kind -ne $rightInfo.Kind) { return $null }

    $maxCoreCount = [Math]::Max($leftInfo.Core.Count, $rightInfo.Core.Count)
    for ($index = 0; $index -lt $maxCoreCount; $index++) {
        $leftPart = if ($index -lt $leftInfo.Core.Count) { $leftInfo.Core[$index] } else { 0 }
        $rightPart = if ($index -lt $rightInfo.Core.Count) { $rightInfo.Core[$index] } else { 0 }
        if ($leftPart -lt $rightPart) { return -1 }
        if ($leftPart -gt $rightPart) { return 1 }
    }

    if ($leftInfo.Kind -eq "ndk") {
        if ($leftInfo.Suffix -lt $rightInfo.Suffix) { return -1 }
        if ($leftInfo.Suffix -gt $rightInfo.Suffix) { return 1 }
    }

    if ($leftInfo.PreRank -lt $rightInfo.PreRank) { return -1 }
    if ($leftInfo.PreRank -gt $rightInfo.PreRank) { return 1 }

    if ($leftInfo.PreNumber -lt $rightInfo.PreNumber) { return -1 }
    if ($leftInfo.PreNumber -gt $rightInfo.PreNumber) { return 1 }

    return 0
}

function Compare-VersionValues($left, $right) {
    $comparison = Compare-VersionStrings $left $right
    if ($null -ne $comparison) { return $comparison }
    if ($left -eq $right) { return 0 }
    return $null
}

function Test-InstallSyncNeeded($installedValue, $targetValue) {
    if (-not $targetValue -or $targetValue -eq "???") {
        return $false
    }
    if (-not $installedValue) {
        return $true
    }

    $installedComparison = Compare-VersionValues $installedValue $targetValue
    return (($null -eq $installedComparison -and $installedValue -ne $targetValue) -or
        ($null -ne $installedComparison -and $installedComparison -ne 0))
}

function Select-LatestVersion($versions, [switch]$IncludePrerelease) {
    $uniqueVersions = $versions | Where-Object { $_ } | Sort-Object -Unique
    if (-not $IncludePrerelease) {
        $uniqueVersions = $uniqueVersions | Where-Object { -not (Test-IsPrereleaseVersion $_) }
    }
    if (-not $uniqueVersions) { return $null }

    $best = $null
    foreach ($candidate in $uniqueVersions) {
        if (-not $best) {
            $best = $candidate
            continue
        }

        $comparison = Compare-VersionStrings $candidate $best
        if ($comparison -gt 0) {
            $best = $candidate
        }
    }

    return $best
}

function Get-ConfValue($key) {
    if ($script:conf.ContainsKey($key)) {
        return $script:conf[$key]
    }
    return $null
}

function Normalize-CommandOutput($output) {
    foreach ($item in @($output)) {
        if ($null -eq $item) { continue }
        $text = ([string]$item).Trim()
        if ($text) {
            return $text
        }
    }
    return $null
}

function Invoke-ConfiguredValueCommand($commandKey) {
    $command = Get-ConfValue $commandKey
    if (-not $command) { return $null }

    try {
        $output = & ([ScriptBlock]::Create($command))
        return Normalize-CommandOutput $output
    } catch {
        return $null
    }
}

function Invoke-ConfiguredActionCommand($commandKey) {
    $command = Get-ConfValue $commandKey
    if (-not $command) {
        throw "Configured action command '$commandKey' was not found"
    }

    & ([ScriptBlock]::Create($command))
}

function Write-RequestLoadLine($uri) {
    if (-not $uri) { return }
    Write-Host ("loading {0}..." -f $uri)
}

function Invoke-LoggedWebRequest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Uri,

        [int]$TimeoutSec = 15,

        [hashtable]$Headers
    )

    Write-RequestLoadLine $Uri

    $requestParams = @{
        Uri = $Uri
        UseBasicParsing = $true
        TimeoutSec = $TimeoutSec
    }
    if ($Headers) {
        $requestParams["Headers"] = $Headers
    }

    Invoke-WebRequest @requestParams
}

function Get-InstalledIfPathExists($path, $version) {
    if ($path -and (Test-Path -LiteralPath $path)) {
        return $version
    }
    return $null
}

function Get-VerifiedFileVersion($path, $expectedSha, $version) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    try {
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower()
        if ($expectedSha -and $actual -eq $expectedSha.ToLower()) {
            return $version
        }
    } catch {}
    return $null
}

function Get-PropertyValueFromFile($path, $propertyName) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    foreach ($line in Get-Content -LiteralPath $path) {
        if ($line -match ('^' + [regex]::Escape($propertyName) + '=(.+)$')) {
            return $Matches[1].Trim()
        }
    }
    return $null
}

function Get-ExecutableVersionFromRegex($path, [string[]]$Arguments, $pattern, $prefix = "") {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    try {
        $output = & $path @Arguments 2>&1
        foreach ($line in @($output)) {
            $text = [string]$line
            if ($text -match $pattern) {
                return "$prefix$($Matches[1])"
            }
        }
    } catch {}
    return $null
}

function Get-JavaVersionFromExecutable($path) {
    return Get-ExecutableVersionFromRegex $path @("-version") 'version "([^"]+)"'
}

function Get-PowerShell7InstalledVersion {
    $currentPwsh = (Get-Process -Id $PID).Path
    $currentVersion = Get-PowerShellVersionFromCommand $currentPwsh
    if ($currentVersion -match '^7\.') { return $currentVersion }

    $command = @(
        Get-Command pwsh -ErrorAction SilentlyContinue | Select-Object -First 1
        Get-Command pwsh-preview -ErrorAction SilentlyContinue | Select-Object -First 1
    ) | Where-Object { $_ } | Select-Object -First 1
    if ($command) {
        $pathVersion = Get-PowerShellVersionFromCommand $command.Source
        if ($pathVersion -match '^7\.') { return $pathVersion }
    }

    $localPwsh = Join-Path (Join-Path $dependency_base "powershell-$POWERSHELL_VERSION") (Get-PlatformExecutableName -ToolName "pwsh")
    $localVersion = Get-PowerShellVersionFromCommand $localPwsh
    if ($localVersion -match '^7\.') { return $localVersion }

    return $null
}

function Get-PowerShellVersionFromCommand($path) {
    if (-not $path) { return $null }
    try {
        return Normalize-CommandOutput (& $path -NoProfile -Command '$PSVersionTable.PSVersion.ToString()')
    } catch {
        return $null
    }
}

function Get-JdkInstalledVersion {
    $releaseFile = Join-Path (Join-Path $dependency_base "jdk-$JDK_MAJOR") "release"
    $value = Get-PropertyValueFromFile $releaseFile "JAVA_VERSION"
    if ($value) {
        return $value.Trim([char]34)
    }
    return $null
}

function Get-BuildToolsInstalledVersion {
    return Get-LatestInstalledDirectoryVersion (Join-Path (Join-Path $dependency_base "android-sdk") "build-tools") "*" "^(.+)$"
}

function Get-CompileSdkInstalledVersion {
    return Get-LatestInstalledDirectoryVersion (Join-Path (Join-Path $dependency_base "android-sdk") "platforms") "android-*" "^android-(\d+)(?:\.0)?$"
}

function Get-SdkCmdlineToolsInstalledBuildId {
    $latestDir = Join-Path (Join-Path (Join-Path $dependency_base "android-sdk") "cmdline-tools") "latest"
    $binDir = Join-Path $latestDir "bin"
    $sdkManagerPath = Get-PlatformToolPath -BaseDir $binDir -ToolName "sdkmanager" -UseBatch
    if ($sdkManagerPath) {
        $markerPath = Join-Path $latestDir ".dxx-cmdline-tools-build-id"
        if (Test-Path -LiteralPath $markerPath) {
            return (Get-Content -LiteralPath $markerPath -First 1).Trim()
        }
    }
    return $null
}

function Get-CmakeInstalledVersion {
    return Get-LatestInstalledDirectoryVersion (Join-Path (Join-Path $dependency_base "android-sdk") "cmake") "*" "^(.+)$"
}

function Get-SoundfontInstalledVersion {
    $soundfontPath = Join-Path (Join-Path $android_dir "app/src/main/assets") "gm.sf2"
    return Get-VerifiedFileVersion $soundfontPath $SOUNDFONT_SHA256 $SOUNDFONT_VERSION
}

function Get-ChromaprintInstalledVersion {
    $version = Get-LatestInstalledDirectoryVersion $dependency_base "fpcalc-*" "^fpcalc-(.+)$"
    if ($version) {
        return "v$version"
    }
    return $null
}

function Get-UnarInstalledVersion {
    $unarPath = Get-PlatformToolPath -BaseDir (Join-Path $dependency_base $UNAR_DIR_NAME) -ToolName "unar"
    return Get-ExecutableVersionFromRegex $unarPath @("-v") "v?([0-9]+\.[0-9]+\.[0-9]+)" "v"
}

function Get-ImageMagickInstalledVersion {
    $localVersion = Get-LatestInstalledDirectoryVersion $dependency_base "imagemagick-*" "^imagemagick-(.+)$"
    if ($localVersion) {
        return $localVersion
    }

    $magickPath = Get-PlatformToolPath -ToolName "magick"
    if (-not $magickPath) {
        return $null
    }

    try {
        $firstLine = Normalize-CommandOutput (& $magickPath --version 2>&1)
        if ($firstLine -match 'ImageMagick\s+([0-9]+\.[0-9]+\.[0-9]+-\d+)') {
            return $Matches[1]
        }
    } catch {}

    return $null
}

function Get-SevenZipInstalledVersion {
    $localVersion = Get-LatestInstalledDirectoryVersion $dependency_base "7z-*" "^7z-(.+)$"
    if ($localVersion) {
        return $localVersion
    }

    $sevenZipPath = Get-ToolPathFromPath -CommandNames @("7zz", "7z", "7za")
    return Get-SevenZipVersionFromExecutable $sevenZipPath
}

function Get-SdkCommandLineToolsUrlForPlatform($buildId) {
    $token = Get-SdkCmdlineToolsOsToken
    if (-not $token) {
        $token = "win"
    }
    return "https://dl.google.com/android/repository/commandlinetools-$token-$buildId`_latest.zip"
}

function Get-NdkUrlForPlatform($version) {
    $token = Get-NdkArchiveOsToken
    if (-not $token) {
        $token = "windows"
    }
    return "https://dl.google.com/android/repository/android-ndk-$version-$token.zip"
}

function Get-JdkUrlForPlatform($majorVersion) {
    $token = Get-AdoptiumOsToken
    if (-not $token) {
        $token = "windows"
    }
    return "https://api.adoptium.net/v3/binary/latest/$majorVersion/ga/$token/x64/jdk/hotspot/normal/eclipse?project=jdk"
}

function Get-CmakeUrlForPlatform($version) {
    switch ($script:hostPlatform) {
        "Linux" { return "https://github.com/Kitware/CMake/releases/download/v$version/cmake-$version-linux-x86_64.tar.gz" }
        "MacOS" { return "https://github.com/Kitware/CMake/releases/download/v$version/cmake-$version-macos-universal.tar.gz" }
        default { return "https://github.com/Kitware/CMake/releases/download/v$version/cmake-$version-windows-x86_64.zip" }
    }
}

function Get-ShellcheckUrlForPlatform($version) {
    switch ($script:hostPlatform) {
        "Linux" { return "https://github.com/koalaman/shellcheck/releases/download/v$version/shellcheck-v$version.linux.x86_64.tar.xz" }
        "MacOS" { return "https://github.com/koalaman/shellcheck/releases/download/v$version/shellcheck-v$version.darwin.x86_64.tar.xz" }
        default { return "https://github.com/koalaman/shellcheck/releases/download/v$version/shellcheck-v$version.zip" }
    }
}

function Get-ShfmtUrlForPlatform($version) {
    switch ($script:hostPlatform) {
        "Linux" { return "https://github.com/mvdan/sh/releases/download/v$version/shfmt_v${version}_linux_amd64" }
        "MacOS" { return "https://github.com/mvdan/sh/releases/download/v$version/shfmt_v${version}_darwin_amd64" }
        default { return "https://github.com/mvdan/sh/releases/download/v$version/shfmt_v${version}_windows_amd64.exe" }
    }
}

function Get-ChromaprintFpcalcUrlForPlatform($tag) {
    $plainVersion = $tag -replace '^v', ''
    switch ($script:hostPlatform) {
        "Linux" { return "https://github.com/acoustid/chromaprint/releases/download/$tag/chromaprint-fpcalc-$plainVersion-linux-x86_64.tar.gz" }
        "MacOS" { return "https://github.com/acoustid/chromaprint/releases/download/$tag/chromaprint-fpcalc-$plainVersion-macos-x86_64.tar.gz" }
        default { return "https://github.com/acoustid/chromaprint/releases/download/$tag/chromaprint-fpcalc-$plainVersion-windows-x86_64.zip" }
    }
}

function Get-PowerShellUrlForPlatform($version) {
    switch ($script:hostPlatform) {
        "Linux" { return "https://github.com/PowerShell/PowerShell/releases/download/v$version/powershell-$version-linux-x64.tar.gz" }
        "MacOS" { return "https://github.com/PowerShell/PowerShell/releases/download/v$version/powershell-$version-osx-x64.tar.gz" }
        default { return "https://github.com/PowerShell/PowerShell/releases/download/v$version/PowerShell-$version-win-x64.zip" }
    }
}

function Get-GradleVerificationCommand {
    if ($script:hostPlatform -eq "Windows") {
        return "cd ..; .\\gradlew.bat assembleDebug"
    }

    return "cd ..; ./gradlew assembleDebug"
}

function Get-SevenZipVersionFromExecutable($path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    try {
        $output = & $path 2>&1
        foreach ($line in @($output)) {
            $text = [string]$line
            if ($text -match '7-Zip(?: \(.\))?\s+(\d+)\.(\d+)') {
                return ('{0}{1:D2}' -f [int]$Matches[1], [int]$Matches[2])
            }
        }
    } catch {}
    return $null
}

function Get-LatestInstalledDirectoryVersion($basePath, $filter, $namePattern) {
    if (-not $basePath -or -not (Test-Path -LiteralPath $basePath)) { return $null }

    $versions = @(
        Get-ChildItem -LiteralPath $basePath -Directory -Filter $filter -ErrorAction SilentlyContinue |
            ForEach-Object {
                if ($_.Name -match $namePattern) {
                    $Matches[1]
                }
            } |
            Where-Object { $_ }
    )
    return Select-LatestVersion $versions -IncludePrerelease
}

function Get-DependencyBaseKey($dep) {
    if ($dep.ContainsKey("BaseKey")) {
        return $dep.BaseKey
    }
    if ($dep.ConfKey -match '^(.*)_VERSION$') {
        return $Matches[1]
    }
    if ($dep.ConfKey -match '^(.*)_COMMIT$') {
        return $Matches[1]
    }
    if ($dep.ConfKey -match '^(.*)_URL$') {
        return $Matches[1]
    }
    return $null
}

function Resolve-Selection($inputString, $items) {
    $resolved = @()
    if ([string]::IsNullOrWhiteSpace($inputString)) {
        return $resolved
    }
    if ($inputString.Trim().ToLower() -in @('a', 'all')) {
        return @($items)
    }

    $nums = $inputString -split '[,\s]+' |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^\d+$' }
    foreach ($n in $nums) {
        $match = $items | Where-Object { $_.Index -eq [int]$n }
        if ($match) { $resolved += $match }
    }

    return $resolved
}

function Get-BashCommandPath {
    foreach ($name in @("bash", "sh")) {
        $command = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) {
            return $command.Source
        }
    }
    return $null
}

function Invoke-InstallerScript($path) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Installer script not found: $path"
    }

    $savedGetAllRunning = $env:GET_ALL_RUNNING
    $env:GET_ALL_RUNNING = "1"
    try {
        switch ([System.IO.Path]::GetExtension($path).ToLowerInvariant()) {
            ".ps1" {
                & $path
            }
            ".sh" {
                $bash = Get-BashCommandPath
                if (-not $bash) {
                    throw "bash or sh was not found on PATH"
                }
                & $bash $path
                if ($LASTEXITCODE -ne 0) {
                    throw "Installer script failed with exit code $LASTEXITCODE`: $path"
                }
            }
            default {
                & $path
            }
        }
    } finally {
        if ($null -eq $savedGetAllRunning) {
            Remove-Item Env:GET_ALL_RUNNING -ErrorAction SilentlyContinue
        } else {
            $env:GET_ALL_RUNNING = $savedGetAllRunning
        }
    }
}

function Get-LatestMavenVersion($group, $artifact) {
    $groupPath = $group -replace '\.', '/'
    foreach ($repo in @("https://dl.google.com/dl/android/maven2",
            "https://repo1.maven.org/maven2")) {
        $url = "$repo/$groupPath/$artifact/maven-metadata.xml"
        try {
            $xml = [xml](Invoke-LoggedWebRequest -Uri $url -TimeoutSec 10).Content
            $versions = @($xml.metadata.versioning.versions.version | ForEach-Object { [string]$_ })
            $ver = Select-LatestVersion $versions
            if ($ver) { return $ver }

            $release = [string]$xml.metadata.versioning.release
            if ($release -and -not (Test-IsPrereleaseVersion $release)) {
                return $release
            }
        } catch {}
    }
    return $null
}

function Get-AndroidRepositoryContent {
    if ($script:AndroidRepositoryContent) { return $script:AndroidRepositoryContent }
    try {
        $script:AndroidRepositoryContent = (Invoke-LoggedWebRequest -Uri "https://dl.google.com/android/repository/repository2-1.xml" `
                -TimeoutSec 15).Content
        return $script:AndroidRepositoryContent
    } catch { return $null }
}

function Get-LatestAndroidPackageVersion($packagePrefix) {
    $content = Get-AndroidRepositoryContent
    if (-not $content) { return $null }

    $pattern = 'path="' + [regex]::Escape($packagePrefix) + ';([0-9.]+)"'
    $versions = [regex]::Matches($content, $pattern) |
        ForEach-Object { $_.Groups[1].Value }
    return Select-LatestVersion $versions
}

function Get-SdkCommandLineToolsBuildId($url) {
    if ($url -match 'commandlinetools-(?:win|linux|mac)-(\d+)_latest\.zip') {
        return $Matches[1]
    }
    return $null
}

function Get-LatestSdkCommandLineToolsInfo {
    $content = Get-AndroidRepositoryContent
    if (-not $content) { return $null }

    $buildIds = [regex]::Matches($content, 'commandlinetools-(?:win|linux|mac)-(\d+)_latest\.zip') |
        ForEach-Object { $_.Groups[1].Value }
    $latestBuild = Select-LatestVersion $buildIds -IncludePrerelease
    if (-not $latestBuild) { return $null }

    return @{
        Build = $latestBuild
        Url = Get-SdkCommandLineToolsUrlForPlatform $latestBuild
    }
}

function Get-GitHubLatestRelease($repo) {
    if (-not $script:GitHubReleaseCache) { $script:GitHubReleaseCache = @{} }
    if ($script:GitHubReleaseCache.ContainsKey($repo)) {
        return $script:GitHubReleaseCache[$repo]
    }

    try {
        $headers = @{
            "Accept" = "application/vnd.github+json"
            "User-Agent" = "dxx-redux-check-updates"
        }
        $json = (Invoke-LoggedWebRequest -Uri "https://api.github.com/repos/$repo/releases/latest" `
                -TimeoutSec 15 -Headers $headers).Content | ConvertFrom-Json
        $script:GitHubReleaseCache[$repo] = $json
        return $json
    } catch { return $null }
}

function Get-GitHubReleases($repo) {
    if (-not $script:GitHubReleasesCache) { $script:GitHubReleasesCache = @{} }
    if ($script:GitHubReleasesCache.ContainsKey($repo)) {
        return $script:GitHubReleasesCache[$repo]
    }

    try {
        $headers = @{
            "Accept" = "application/vnd.github+json"
            "User-Agent" = "dxx-redux-check-updates"
        }
        $json = (Invoke-LoggedWebRequest -Uri "https://api.github.com/repos/$repo/releases?per_page=30" `
                -TimeoutSec 15 -Headers $headers).Content | ConvertFrom-Json
        $script:GitHubReleasesCache[$repo] = @($json)
        return $script:GitHubReleasesCache[$repo]
    } catch { return @() }
}

function Get-GitHubRepoInfo($repo) {
    if (-not $script:GitHubRepoCache) { $script:GitHubRepoCache = @{} }
    if ($script:GitHubRepoCache.ContainsKey($repo)) {
        return $script:GitHubRepoCache[$repo]
    }

    try {
        $headers = @{
            "Accept" = "application/vnd.github+json"
            "User-Agent" = "dxx-redux-check-updates"
        }
        $json = (Invoke-LoggedWebRequest -Uri "https://api.github.com/repos/$repo" `
                -TimeoutSec 15 -Headers $headers).Content | ConvertFrom-Json
        $script:GitHubRepoCache[$repo] = $json
        return $json
    } catch { return $null }
}

function Get-GitCommandPath {
    if ($script:GitCommandPath) { return $script:GitCommandPath }

    $command = Get-Command git -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) {
        $script:GitCommandPath = $command.Source
        return $script:GitCommandPath
    }

    return $null
}

function Get-GitRemoteTags($repo) {
    if (-not $script:GitTagCache) { $script:GitTagCache = @{} }
    if ($script:GitTagCache.ContainsKey($repo)) {
        return $script:GitTagCache[$repo]
    }

    $git = Get-GitCommandPath
    if (-not $git) { return @() }

    try {
        $lines = & $git ls-remote --tags --refs "https://github.com/$repo.git" 2>$null
        $tags = @($lines | ForEach-Object {
                if ($_ -match '^[0-9a-f]{40}\s+refs/tags/(.+)$') {
                    $Matches[1]
                }
            } | Where-Object { $_ })
        $script:GitTagCache[$repo] = $tags
        return $tags
    } catch {
        return @()
    }
}

function Get-GitRemoteHeadCommit($repo) {
    if (-not $script:GitHeadCache) { $script:GitHeadCache = @{} }
    if ($script:GitHeadCache.ContainsKey($repo)) {
        return $script:GitHeadCache[$repo]
    }

    $git = Get-GitCommandPath
    if (-not $git) { return $null }

    try {
        $line = & $git ls-remote "https://github.com/$repo.git" HEAD 2>$null | Select-Object -First 1
        if ($line -match '^([0-9a-f]{40})\s+HEAD$') {
            $script:GitHeadCache[$repo] = $Matches[1]
            return $script:GitHeadCache[$repo]
        }
    } catch {}

    return $null
}

function Get-WebResponseUri($response) {
    if ($response -and $response.BaseResponse) {
        if ($response.BaseResponse.ResponseUri) {
            return [string]$response.BaseResponse.ResponseUri
        }
        if ($response.BaseResponse.RequestMessage -and $response.BaseResponse.RequestMessage.RequestUri) {
            return [string]$response.BaseResponse.RequestMessage.RequestUri
        }
    }
    return $null
}

function Get-GitHubLatestReleaseInfoFromWeb($repo, $trimPrefix = "") {
    try {
        $response = Invoke-LoggedWebRequest -Uri "https://github.com/$repo/releases/latest" `
            -TimeoutSec 15
        $finalUri = Get-WebResponseUri $response
        if (-not $finalUri -or $finalUri -notmatch '/releases/tag/([^/?#]+)') {
            return $null
        }

        $tag = [uri]::UnescapeDataString($Matches[1])
        $version = $tag
        if ($trimPrefix) {
            if (-not $tag.StartsWith($trimPrefix)) {
                return $null
            }
            $version = $tag.Substring($trimPrefix.Length)
        }
        if (Test-IsPrereleaseVersion $version) {
            return $null
        }

        return @{
            Tag = $tag
            Version = $version
        }
    } catch {
        return $null
    }
}

function Get-LatestGitTagInfo($repo, $trimPrefix = "", $requiredVersionPrefix = "", [switch]$IncludePrerelease) {
    $best = $null

    foreach ($tag in Get-GitRemoteTags $repo) {
        $version = $tag
        if ($trimPrefix) {
            if (-not $tag.StartsWith($trimPrefix)) {
                continue
            }
            $version = $tag.Substring($trimPrefix.Length)
        }
        if ($requiredVersionPrefix -and -not $version.StartsWith($requiredVersionPrefix)) {
            continue
        }
        if (-not $IncludePrerelease -and (Test-IsPrereleaseVersion $version)) {
            continue
        }

        if (-not $best) {
            $best = @{ Tag = $tag; Version = $version }
            continue
        }

        $comparison = Compare-VersionStrings $version $best.Version
        if ($comparison -gt 0) {
            $best = @{ Tag = $tag; Version = $version }
        }
    }

    return $best
}

function Get-GitHubExpandedAssetsHtml($repo, $tag) {
    if (-not $script:GitHubAssetsCache) { $script:GitHubAssetsCache = @{} }
    $cacheKey = "$repo::$tag"
    if ($script:GitHubAssetsCache.ContainsKey($cacheKey)) {
        return $script:GitHubAssetsCache[$cacheKey]
    }

    try {
        $html = (Invoke-LoggedWebRequest -Uri "https://github.com/$repo/releases/expanded_assets/$tag" `
                -TimeoutSec 15).Content
        $script:GitHubAssetsCache[$cacheKey] = $html
        return $html
    } catch {
        return $null
    }
}

function Get-LatestGitHubDefaultBranchCommit($repo) {
    return Get-GitRemoteHeadCommit $repo
}

function Get-ShortCommit($commit) {
    if (-not $commit) { return $null }
    if ($commit.Length -le 12) { return $commit }
    return $commit.Substring(0, 12)
}

function Get-LatestGradleVersion {
    try {
        $json = (Invoke-LoggedWebRequest -Uri "https://services.gradle.org/versions/current" `
                -TimeoutSec 10).Content | ConvertFrom-Json
        return $json.version
    } catch { return $null }
}

function Get-LatestNDKVersion {
    # Scrape the NDK download page for the latest version tag
    try {
        $page = (Invoke-LoggedWebRequest -Uri "https://developer.android.com/ndk/downloads" `
                -TimeoutSec 15).Content
        # Look for "android-ndk-r<VER>-<platform>.zip" pattern
        if ($page -match 'android-ndk-(r\d+[a-z]?)-(?:windows|linux|darwin)\.zip') {
            return $Matches[1]
        }
    } catch {}
    return $null
}

function Get-LatestJDKVersion {
    # Check Adoptium (Eclipse Temurin) for latest JDK 17 and 21
    $versions = @()
    $osToken = Get-AdoptiumOsToken
    if (-not $osToken) {
        $osToken = "windows"
    }
    foreach ($major in @(17, 21)) {
        try {
            $url = "https://api.adoptium.net/v3/info/release_versions?architecture=x64&heap_size=normal&image_type=jdk&os=$osToken&page=0&page_size=1&project=jdk&release_type=ga&sort_method=DEFAULT&sort_order=DESC&vendor=eclipse&version=%5B${major}%2C$($major+1)%29"
            $json = (Invoke-LoggedWebRequest -Uri $url -TimeoutSec 10).Content | ConvertFrom-Json
            if ($json.versions.Count -gt 0) {
                $v = $json.versions[0]
                $semver = "$($v.major).$($v.minor).$($v.security)"
                $versions += @{ Major = $major; Version = $semver }
            }
        } catch {}
    }
    return $versions
}

function Get-LatestBuildToolsVersion {
    return Get-LatestAndroidPackageVersion "build-tools"
}

function Get-LatestCompileSdkVersion {
    $sdkManagerPath = Get-PlatformToolPath -BaseDir (Join-Path (Join-Path (Join-Path (Join-Path $dependency_base "android-sdk") "cmdline-tools") "latest") "bin") -ToolName "sdkmanager" -UseBatch
    if ($sdkManagerPath) {
        $savedJavaHome = $env:JAVA_HOME
        $savedPath = $env:Path
        try {
            $localJdk = Join-Path $dependency_base "jdk-$JDK_MAJOR"
            $localJava = Get-PlatformToolPath -BaseDir (Join-Path $localJdk "bin") -ToolName "java"
            if ($localJava) {
                $env:JAVA_HOME = $localJdk
                $env:Path = "$(Join-Path $localJdk "bin");$env:Path"
            }
            $output = & $sdkManagerPath --list 2>&1
            $versions = [regex]::Matches(($output -join "`n"), 'platforms;android-(\d+(?:\.0)?)\b') |
                ForEach-Object {
                    $version = $_.Groups[1].Value
                    if ($version -match '^(\d+)(?:\.0)?$') {
                        $Matches[1]
                    } else {
                        $version
                    }
                }
            $latest = Select-LatestVersion $versions
            if ($latest) { return $latest }
        } catch {} finally {
            $env:JAVA_HOME = $savedJavaHome
            $env:Path = $savedPath
        }
    }

    $content = Get-AndroidRepositoryContent
    if (-not $content) { return $null }

    $versions = [regex]::Matches($content, 'path="platforms;android-(\d+(?:\.\d+)?)"') |
        ForEach-Object {
            $version = $_.Groups[1].Value
            if ($version -match '^(\d+)(?:\.0)?$') {
                $Matches[1]
            } else {
                $version
            }
        }
    return Select-LatestVersion $versions
}

function Get-LatestCMakeVersion {
    return Get-LatestAndroidPackageVersion "cmake"
}

function Get-LatestNdkFullVersion {
    return Get-LatestAndroidPackageVersion "ndk"
}

function Get-LatestSoundfontVersion {
    $latest = Get-LatestGitTagInfo "arbruijn/TimGM6mb" "v"
    if ($latest) {
        return $latest.Version
    }
    return $null
}

function Get-LatestClangFormatInfo {
    $releaseInfo = Get-GitHubLatestReleaseInfoFromWeb "muttleyxd/clang-tools-static-binaries"
    $tag = if ($releaseInfo) { $releaseInfo.Tag } else { $conf["CLANG_FORMAT_RELEASE_TAG"] }
    if (-not $tag) { return $null }

    $html = Get-GitHubExpandedAssetsHtml "muttleyxd/clang-tools-static-binaries" $tag
    if (-not $html) { return $null }

    $assetPattern = 'clang-format-(\d+)_windows-amd64\.exe'
    $assetTemplate = 'clang-format-{0}_windows-amd64.exe'
    switch ($script:hostPlatform) {
        "Linux" {
            $assetPattern = 'clang-format-(\d+)_linux-amd64'
            $assetTemplate = 'clang-format-{0}_linux-amd64'
        }
        "MacOS" {
            $assetPattern = 'clang-format-(\d+)_macosx-amd64'
            $assetTemplate = 'clang-format-{0}_macosx-amd64'
        }
    }

    $bestAsset = $null
    foreach ($match in [regex]::Matches($html, $assetPattern)) {
        $assetVersion = $match.Groups[1].Value
        $assetName = [string]::Format($assetTemplate, $assetVersion)
        $candidate = @{
            Version = $assetVersion
            Tag = $tag
            Url = "https://github.com/muttleyxd/clang-tools-static-binaries/releases/download/$tag/$assetName"
        }

        if (-not $bestAsset) {
            $bestAsset = $candidate
            continue
        }

        if ((Compare-VersionStrings $candidate.Version $bestAsset.Version) -gt 0) {
            $bestAsset = $candidate
        }
    }

    return $bestAsset
}

function Get-LatestImageMagickVersion {
    try {
        $page = (Invoke-LoggedWebRequest -Uri "https://imagemagick.org/archive/binaries/" `
                -TimeoutSec 15).Content
        $versions = [regex]::Matches($page, 'ImageMagick-((\d+\.\d+\.\d+-\d+))-portable-Q16-HDRI-x64\.7z') |
            ForEach-Object { $_.Groups[1].Value }
        return Select-LatestVersion $versions -IncludePrerelease
    } catch { return $null }
}

function Get-LatestSevenZipVersion {
    try {
        $page = (Invoke-LoggedWebRequest -Uri "https://www.7-zip.org/download.html" `
                -TimeoutSec 15).Content
        $versions = [regex]::Matches($page, '7z(\d+)-extra\.7z') |
            ForEach-Object { $_.Groups[1].Value }
        return Select-LatestVersion $versions -IncludePrerelease
    } catch { return $null }
}

function Get-LatestPyPIVersion($package) {
    try {
        $json = (Invoke-LoggedWebRequest -Uri "https://pypi.org/pypi/$package/json" `
                -TimeoutSec 15).Content | ConvertFrom-Json
        return [string]$json.info.version
    } catch { return $null }
}

# -- Fetch all versions -------------------------------------------------------

Write-Host ""
Write-Host "Checking for updates..."
Write-Host ""

$latestSdkCommandLineTools = Get-LatestSdkCommandLineToolsInfo
$latestShellcheck = Get-LatestGitTagInfo "koalaman/shellcheck" "v"
$latestShfmt = Get-LatestGitTagInfo "mvdan/sh" "v"
$latestKtlint = Get-LatestGitTagInfo "pinterest/ktlint"
$latestCmakelang = Get-LatestPyPIVersion "cmakelang"
$latestChromaprint = Get-LatestGitTagInfo "acoustid/chromaprint" "v"
$latestClangFormat = Get-LatestClangFormatInfo
$latestNdkFullVersion = Get-LatestNdkFullVersion
$latestMinimp3Commit = Get-LatestGitHubDefaultBranchCommit "lieff/minimp3"
$latestStbVorbisCommit = Get-LatestGitHubDefaultBranchCommit "nothings/stb"
$latestDrFlacCommit = Get-LatestGitHubDefaultBranchCommit "mackron/dr_libs"
$latestPowerShell = Get-LatestGitTagInfo "PowerShell/PowerShell" "v" "7."

# Build the dependency list: Name, ConfKey, Current, Latest, Extra conf keys to update
$deps = @(
    @{ Name = "Android Gradle Plugin"; ConfKey = "AGP_VERSION";
        Current = $conf["AGP_VERSION"];
        Latest = Get-LatestMavenVersion "com.android.tools.build" "gradle"
    },

    @{ Name = "Gradle"; ConfKey = "GRADLE_VERSION";
        Current = $conf["GRADLE_VERSION"];
        Latest = Get-LatestGradleVersion
    },

    @{ Name = "Kotlin"; ConfKey = "KOTLIN_VERSION";
        Current = $conf["KOTLIN_VERSION"];
        Latest = Get-LatestMavenVersion "org.jetbrains.kotlin" "kotlin-stdlib"
    },

    @{ Name = "Compose Compiler"; ConfKey = "COMPOSE_COMPILER_VERSION";
        Current = $conf["COMPOSE_COMPILER_VERSION"];
        Latest = Get-LatestMavenVersion "androidx.compose.compiler" "compiler"
    },

    @{ Name = "Compose BOM"; ConfKey = "COMPOSE_BOM_VERSION";
        Current = $conf["COMPOSE_BOM_VERSION"];
        Latest = Get-LatestMavenVersion "androidx.compose" "compose-bom"
    },

    @{ Name = "core-ktx"; ConfKey = "CORE_KTX_VERSION";
        Current = $conf["CORE_KTX_VERSION"];
        Latest = Get-LatestMavenVersion "androidx.core" "core-ktx"
    },

    @{ Name = "appcompat"; ConfKey = "APPCOMPAT_VERSION";
        Current = $conf["APPCOMPAT_VERSION"];
        Latest = Get-LatestMavenVersion "androidx.appcompat" "appcompat"
    },

    @{ Name = "activity-compose"; ConfKey = "ACTIVITY_COMPOSE_VERSION";
        Current = $conf["ACTIVITY_COMPOSE_VERSION"];
        Latest = Get-LatestMavenVersion "androidx.activity" "activity-compose"
    },

    @{ Name = "Android NDK"; ConfKey = "NDK_VERSION";
        Current = $conf["NDK_VERSION"];
        Latest = Get-LatestNDKVersion;
        LatestFullVersion = $latestNdkFullVersion
    },

    @{ Name = "Build Tools"; ConfKey = "BUILD_TOOLS_VERSION";
        Current = $conf["BUILD_TOOLS_VERSION"];
        Latest = Get-LatestBuildToolsVersion
    },

    @{ Name = "Compile SDK"; ConfKey = "COMPILE_SDK";
        BaseKey = "COMPILE_SDK";
        Current = $conf["COMPILE_SDK"];
        Latest = Get-LatestCompileSdkVersion
    },

    @{ Name = "CMake"; ConfKey = "CMAKE_VERSION";
        Current = $conf["CMAKE_VERSION"];
        Latest = Get-LatestCMakeVersion;
        ManagedBy = "Android SDK";
        SuppressTargetUpdate = $true;
        SuppressInstallSync = $true
    },

    @{ Name = "Android SDK cmdline-tools"; ConfKey = "SDK_CMDLINE_TOOLS_URL";
        Current = Get-SdkCommandLineToolsBuildId $conf["SDK_CMDLINE_TOOLS_URL"];
        Latest = if ($latestSdkCommandLineTools) { $latestSdkCommandLineTools["Build"] } else { $null };
        NewValue = if ($latestSdkCommandLineTools) { $latestSdkCommandLineTools["Url"] } else { $null }
    },

    @{ Name = "GM Soundfont"; ConfKey = "SOUNDFONT_VERSION";
        Current = $conf["SOUNDFONT_VERSION"];
        Latest = Get-LatestSoundfontVersion
    },

    @{ Name = "clang-format"; ConfKey = "CLANG_FORMAT_VERSION";
        Current = $conf["CLANG_FORMAT_VERSION"];
        Latest = if ($latestClangFormat) { $latestClangFormat["Version"] } else { $null };
        ReleaseTag = if ($latestClangFormat) { $latestClangFormat["Tag"] } else { $null };
        LatestUrl = if ($latestClangFormat) { $latestClangFormat["Url"] } else { $null }
    },

    @{ Name = "shellcheck"; ConfKey = "SHELLCHECK_VERSION";
        Current = $conf["SHELLCHECK_VERSION"];
        Latest = if ($latestShellcheck) { $latestShellcheck["Version"] } else { $null }
    },

    @{ Name = "shfmt"; ConfKey = "SHFMT_VERSION";
        Current = $conf["SHFMT_VERSION"];
        Latest = if ($latestShfmt) { $latestShfmt["Version"] } else { $null }
    },

    @{ Name = "OkHttp"; ConfKey = "OKHTTP_VERSION";
        Current = $conf["OKHTTP_VERSION"];
        Latest = Get-LatestMavenVersion "com.squareup.okhttp3" "okhttp"
    },

    @{ Name = "unar"; ConfKey = "UNAR_VERSION";
        Current = $conf["UNAR_VERSION"];
        Latest = $conf["UNAR_VERSION"];
        SuppressTargetUpdate = $true;
        LinuxManualOnly = $script:hostPlatform -eq "Linux";
        DriftLabel = "manual-linux";
        ManualInstallHint = "On Ubuntu install the host package with sudo apt install unar"
    },

    @{ Name = "kotlinx-serialization-json"; ConfKey = "KOTLINX_SERIALIZATION_VERSION";
        Current = $conf["KOTLINX_SERIALIZATION_VERSION"];
        Latest = Get-LatestMavenVersion "org.jetbrains.kotlinx" "kotlinx-serialization-json"
    },

    @{ Name = "play-services-games-v2"; ConfKey = "PLAY_SERVICES_GAMES_VERSION";
        Current = $conf["PLAY_SERVICES_GAMES_VERSION"];
        Latest = Get-LatestMavenVersion "com.google.android.gms" "play-services-games-v2";
        SuppressTargetUpdate = $true;
        ManualTargetUpdateHint = "22.0.0 requires minSdk 24; retain 21.x while MIN_SDK is 23"
    },

    @{ Name = "commons-compress"; ConfKey = "COMMONS_COMPRESS_VERSION";
        Current = $conf["COMMONS_COMPRESS_VERSION"];
        Latest = Get-LatestMavenVersion "org.apache.commons" "commons-compress"
    },

    @{ Name = "JUnit"; ConfKey = "JUNIT_VERSION";
        Current = $conf["JUNIT_VERSION"];
        Latest = Get-LatestMavenVersion "junit" "junit"
    },

    @{ Name = "Play App Update"; ConfKey = "APP_UPDATE_VERSION";
        Current = $conf["APP_UPDATE_VERSION"];
        Latest = Get-LatestMavenVersion "com.google.android.play" "app-update-ktx"
    },

    @{ Name = "xcrash"; ConfKey = "XCRASH_VERSION";
        Current = $conf["XCRASH_VERSION"];
        Latest = Get-LatestMavenVersion "com.iqiyi.xcrash" "xcrash-android-lib"
    },

    @{ Name = "cmakelang"; ConfKey = "CMAKELANG_VERSION";
        Current = $conf["CMAKELANG_VERSION"];
        Latest = $latestCmakelang
    },

    @{ Name = "ktlint"; ConfKey = "KTLINT_VERSION";
        Current = $conf["KTLINT_VERSION"];
        Latest = if ($latestKtlint) { $latestKtlint["Version"] } else { $null };
        ReleaseTag = if ($latestKtlint) { $latestKtlint["Tag"] } else { $null }
    },

    @{ Name = "Chromaprint"; ConfKey = "CHROMAPRINT_VERSION";
        Current = $conf["CHROMAPRINT_VERSION"];
        Latest = if ($latestChromaprint) { $latestChromaprint["Tag"] } else { $null };
        SuppressTargetUpdate = $true;
        ManualTargetUpdateHint = "Update CHROMAPRINT_URL and CHROMAPRINT_SHA256 together after reviewing the release"
    },

    @{ Name = "minimp3"; ConfKey = "MINIMP3_COMMIT";
        Current = $conf["MINIMP3_COMMIT"];
        CurrentDisplay = Get-ShortCommit $conf["MINIMP3_COMMIT"];
        Latest = $latestMinimp3Commit;
        LatestDisplay = Get-ShortCommit $latestMinimp3Commit;
        SuppressTargetUpdate = $true;
        ManualTargetUpdateHint = "Update MINIMP3_COMMIT, both URLs, and both SHA-256 values together"
    },

    @{ Name = "stb_vorbis"; ConfKey = "STB_VORBIS_COMMIT";
        Current = $conf["STB_VORBIS_COMMIT"];
        CurrentDisplay = Get-ShortCommit $conf["STB_VORBIS_COMMIT"];
        Latest = $latestStbVorbisCommit;
        LatestDisplay = Get-ShortCommit $latestStbVorbisCommit;
        SuppressTargetUpdate = $true;
        ManualTargetUpdateHint = "Update STB_VORBIS_COMMIT, URL, and SHA-256 together"
    },

    @{ Name = "dr_flac"; ConfKey = "DR_FLAC_COMMIT";
        Current = $conf["DR_FLAC_COMMIT"];
        CurrentDisplay = Get-ShortCommit $conf["DR_FLAC_COMMIT"];
        Latest = $latestDrFlacCommit;
        LatestDisplay = Get-ShortCommit $latestDrFlacCommit;
        SuppressTargetUpdate = $true;
        ManualTargetUpdateHint = "Update DR_FLAC_COMMIT, URL, and SHA-256 together"
    },

    @{ Name = "ImageMagick"; ConfKey = "IMAGEMAGICK_VERSION";
        Current = $conf["IMAGEMAGICK_VERSION"];
        Latest = Get-LatestImageMagickVersion;
        SuppressTargetUpdate = $script:hostPlatform -eq "Linux";
        LinuxManualOnly = $script:hostPlatform -eq "Linux";
        DriftLabel = "manual-linux";
        ManualInstallHint = "Install ImageMagick from your distro so magick is on PATH"
    },

    @{ Name = "7-Zip"; ConfKey = "SEVENZIP_VERSION";
        Current = $conf["SEVENZIP_VERSION"];
        Latest = Get-LatestSevenZipVersion;
        SuppressTargetUpdate = $true;
        ManualTargetUpdateHint = "Update the 7-Zip URLs and reviewed archive, bootstrap, and executable SHA-256 values together";
        LinuxManualOnly = $script:hostPlatform -eq "Linux";
        DriftLabel = "manual-linux";
        ManualInstallHint = "Install a host 7z binary such as p7zip-full or 7zip"
    },

    @{ Name = "PowerShell 7"; ConfKey = "POWERSHELL_VERSION";
        Current = $conf["POWERSHELL_VERSION"];
        Latest = if ($latestPowerShell) { $latestPowerShell["Version"] } else { $null };
        ReleaseTag = if ($latestPowerShell) { $latestPowerShell["Tag"] } else { $null };
        DriftLabel = "host-managed";
        PreferStable = $true;
        ManualInstallHint = "Run android/get_deps/update-powershell.ps1 to update the pinned repo runtime"
    }
)

# JDK: special -- only show update for current major, and upgrade option for the other
$jdkVersions = Get-LatestJDKVersion
$currentJDKMajor = $conf["JDK_MAJOR"]
$currentJDKVersion = $conf["JDK_VERSION"]
$latestJDK17 = ($jdkVersions | Where-Object { $_.Major -eq 17 }).Version
$latestJDK21 = ($jdkVersions | Where-Object { $_.Major -eq 21 }).Version

if ($currentJDKMajor -eq "17" -and $latestJDK17) {
    $deps += @{ Name = "JDK 17"; ConfKey = "JDK_VERSION";
        Current = $currentJDKVersion; Latest = $latestJDK17
    }
    if ($latestJDK21) {
        $deps += @{ Name = "JDK 21 (upgrade)"; ConfKey = "JDK_VERSION";
            Current = "$currentJDKMajor ($currentJDKVersion)"; Latest = $latestJDK21;
            JDKMajor = 21
        }
    }
} elseif ($currentJDKMajor -eq "21" -and $latestJDK21) {
    $deps += @{ Name = "JDK 21"; ConfKey = "JDK_VERSION";
        Current = $currentJDKVersion; Latest = $latestJDK21
    }
}

$deps = @($deps | Sort-Object { [string]$_["Name"] })

# -- Display table ------------------------------------------------------------

$targetUpgradeable = @()
$installOutOfSync = @()
$manualInstallDrift = @()
$manualTargetReviews = @()
$targetIndex = 1
$installIndex = 1

foreach ($dep in $deps) {
    $baseKey = Get-DependencyBaseKey $dep
    if ($baseKey) {
        $dep["BaseKey"] = $baseKey

        $installedCmdKey = "${baseKey}_INSTALLED_VERSION_CMD"
        if ($conf.ContainsKey($installedCmdKey)) {
            $dep["InstalledVersionCmdKey"] = $installedCmdKey
            $dep["Installed"] = Invoke-ConfiguredValueCommand $installedCmdKey
        }

        $installCmdKey = "${baseKey}_INSTALL_CMD"
        if ($conf.ContainsKey($installCmdKey) -and -not ($script:hostPlatform -eq "Linux" -and $dep.ContainsKey("LinuxManualOnly") -and $dep.LinuxManualOnly)) {
            $dep["InstallCmdKey"] = $installCmdKey
        }
    }
}

Write-Host ("{0,-25} {1,-18} {2,-18} {3,-18} {4}" -f "Dependency", "Installed", "Target", "Latest", "Status")
Write-Host ("{0,-25} {1,-18} {2,-18} {3,-18} {4}" -f ("-" * 25), ("-" * 18), ("-" * 18), ("-" * 18), ("-" * 24))
Write-Host "Status legend: T[n] updates tool_versions.conf target, I[n] runs an install helper to sync actual tools"
Write-Host "SDK-managed rows are detected but not offered as standalone updates"

foreach ($dep in $deps) {
    $currentValue = $dep.Current
    $currentDisplay = if ($dep.ContainsKey("CurrentDisplay")) { $dep.CurrentDisplay } else { $dep.Current }
    if (-not $currentDisplay) { $currentDisplay = $currentValue }

    $latestValue = $dep.Latest
    $latestDisplay = if ($dep.ContainsKey("LatestDisplay")) { $dep.LatestDisplay } else { $dep.Latest }
    if (-not $latestDisplay) { $latestDisplay = "???" }
    if (-not $latestValue) { $latestValue = "???" }

    $installedValue = if ($dep.ContainsKey("Installed")) { $dep.Installed } else { $null }
    $installedDisplay = if ($dep.ContainsKey("InstalledDisplay")) { $dep.InstalledDisplay } else { $installedValue }
    if (-not $installedDisplay) {
        if ($dep.ContainsKey("InstalledVersionCmdKey")) {
            $installedDisplay = "???"
        } else {
            $installedDisplay = "n/a"
        }
    }

    $statusParts = @()
    $targetComparison = Compare-VersionValues $currentValue $latestValue
    $targetNeedsUpdate = $false
    if ($latestValue -ne "???" -and $currentValue -ne "unknown") {
        if (($dep.ContainsKey("PreferStable") -and $dep.PreferStable -and
                (Test-IsPrereleaseVersion $currentValue) -and -not (Test-IsPrereleaseVersion $latestValue) -and
                $currentValue -ne $latestValue) -or
            ($null -eq $targetComparison -and $currentValue -ne $latestValue) -or
            ($null -ne $targetComparison -and $targetComparison -lt 0)) {
            $targetNeedsUpdate = $true
        }
    }
    if ($targetNeedsUpdate -and -not $dep.SuppressTargetUpdate) {
        $statusParts += "T[$targetIndex]"
        $targetUpgradeable += @{ Index = $targetIndex; Dep = $dep }
        $targetIndex++
    } elseif ($targetNeedsUpdate -and $dep.ContainsKey("ManualTargetUpdateHint")) {
        $statusParts += "review-hash"
        $manualTargetReviews += $dep
    }

    $installNeedsSync = $false
    if ($dep.ContainsKey("InstalledVersionCmdKey")) {
        $installNeedsSync = Test-InstallSyncNeeded $installedValue $currentValue
    }
    if ($installNeedsSync -and -not $dep.SuppressInstallSync) {
        if ($dep.ContainsKey("InstallCmdKey")) {
            $statusParts += "I[$installIndex]"
            $installOutOfSync += @{ Index = $installIndex; Dep = $dep }
            $installIndex++
        } else {
            $statusParts += if ($dep.ContainsKey("DriftLabel") -and $dep.DriftLabel) { $dep.DriftLabel } else { "drift" }
            $manualInstallDrift += $dep
        }
    }

    if ($dep.ContainsKey("ManagedBy") -and $dep.ManagedBy) {
        $statusParts += "sdk-managed"
    }

    if ($latestValue -eq "???") {
        $statusParts += "latest ?"
    }

    $status = if ($statusParts.Count -gt 0) {
        ($statusParts | Select-Object -Unique) -join ' '
    } else {
        "up-to-date"
    }

    Write-Host ("{0,-25} {1,-18} {2,-18} {3,-18} {4}" -f $dep.Name, $installedDisplay, $currentDisplay, $latestDisplay, $status)
}

if ($manualTargetReviews.Count -gt 0) {
    Write-Host ""
    Write-Host "Manual target review needed for:"
    foreach ($dep in $manualTargetReviews) {
        Write-Host ("  - {0}: {1}" -f $dep.Name, $dep.ManualTargetUpdateHint)
    }
}

if ($targetUpgradeable.Count -eq 0 -and $installOutOfSync.Count -eq 0 -and
    $manualInstallDrift.Count -eq 0 -and $manualTargetReviews.Count -eq 0) {
    Write-Host ""
    Write-Host "Everything is up to date and installed tools match the configured targets"
    Write-RunNote "finished" @("result=everything_up_to_date")
    return
}

if ($targetUpgradeable.Count -eq 0 -and $installOutOfSync.Count -eq 0 -and
    ($manualInstallDrift.Count -gt 0 -or $manualTargetReviews.Count -gt 0)) {
    Write-Host ""
    Write-Host "No target updates or scripted install sync actions remain"
    if ($manualInstallDrift.Count -gt 0) {
        Write-Host "Manual tool alignment still needed for:"
        foreach ($dep in $manualInstallDrift) {
            $hint = if ($dep.ContainsKey("ManualInstallHint") -and $dep.ManualInstallHint) { $dep.ManualInstallHint } else { "Update the installed tool manually to match the configured target" }
            Write-Host ("  - {0}: {1}" -f $dep.Name, $hint)
        }
    }
    Write-RunNote "finished" @(
        "result=manual_review_needed",
        "manual_drift_count=$($manualInstallDrift.Count)",
        "manual_target_review_count=$($manualTargetReviews.Count)"
    )
    return
}

# -- Prompt -------------------------------------------------------------------

Write-Host ""
$targetInput = $TargetSelection
$installInput = $InstallSelection

if ($NoPrompt -and -not $TargetSelection -and -not $InstallSelection) {
    if ($runningInVsCodeTerminal) {
        Write-Host "Interactive prompt skipped in the VS Code terminal to avoid host lockups"
    }
    Write-Host "Re-run with -TargetSelection a or -TargetSelection 1,2,3 to update tool_versions.conf"
    Write-Host "Re-run with -InstallSelection a or -InstallSelection 1,2,3 to sync installed tools to the configured targets"
    Write-RunNote "finished" @(
        "result=listed_only",
        "target_upgradeable_count=$($targetUpgradeable.Count)",
        "install_out_of_sync_count=$($installOutOfSync.Count)"
    )
    return
}

if (-not $NoPrompt -and -not $TargetSelection -and -not $InstallSelection) {
    Write-Host "Enter target numbers to update tool_versions.conf (comma-separated), 'a' for all, or Enter to skip:"
    $targetInput = Read-Host "Target update"
    Write-Host "Enter install numbers to sync actual tools to the configured target versions (comma-separated), 'a' for all, or Enter to skip:"
    $installInput = Read-Host "Install sync"
}

if ([string]::IsNullOrWhiteSpace($targetInput) -and [string]::IsNullOrWhiteSpace($installInput)) {
    Write-Host "No changes made"
    Write-RunNote "finished" @("result=no_changes")
    return
}

$selectedTarget = @(Resolve-Selection $targetInput $targetUpgradeable)
$selectedInstall = @(Resolve-Selection $installInput $installOutOfSync)

function Get-InstallPriority($depName) {
    switch -Wildcard ($depName) {
        "JDK*" { return 0 }
        "Android SDK cmdline-tools" { return 1 }
        "Build Tools" { return 2 }
        "Android NDK" { return 3 }
        default { return 100 }
    }
}

$targetInstallSelections = @()
$installAllSelected = (-not [string]::IsNullOrWhiteSpace($installInput)) -and (
    $installInput.Trim().ToLowerInvariant() -in @("a", "all")
)
if ($installAllSelected -and $selectedTarget.Count -gt 0) {
    foreach ($item in $selectedTarget) {
        $dep = $item.Dep
        if ($dep.SuppressInstallSync -or
            -not $dep.ContainsKey("InstallCmdKey") -or
            -not $dep.ContainsKey("InstalledVersionCmdKey")) {
            continue
        }

        $installedValue = if ($dep.ContainsKey("Installed")) { $dep.Installed } else { $null }
        if (-not (Test-InstallSyncNeeded $installedValue $dep.Latest)) {
            continue
        }

        $alreadySelected = $selectedInstall | Where-Object { $_.Dep.Name -eq $dep.Name } | Select-Object -First 1
        if ($alreadySelected) {
            continue
        }

        $targetInstallSelections += @{ Index = 100000 + [int]$item.Index; Dep = $dep; FromTargetUpdate = $true }
    }
}
if ($targetInstallSelections.Count -gt 0) {
    $selectedInstall = @(@($selectedInstall) + @($targetInstallSelections))
    $targetInstallNames = ($targetInstallSelections | ForEach-Object { $_.Dep.Name }) -join ', '
    Write-Host ("Install sync 'a' also includes selected target updates: {0}" -f $targetInstallNames)
}

if ($selectedInstall.Count -gt 0) {
    $selectedInstall = @(
        $selectedInstall |
            Sort-Object @{ Expression = { Get-InstallPriority $_.Dep.Name } }, @{ Expression = { $_.Index } }
    )
}

if ($selectedTarget.Count -eq 0 -and $selectedInstall.Count -eq 0) {
    Write-Host "No valid selections"
    Write-RunNote "finished" @(
        "result=no_valid_selections",
        "target_selection_input=$targetInput",
        "install_selection_input=$installInput"
    )
    return
}

# -- Apply upgrades -----------------------------------------------------------

function Update-Conf($key, $value) {
    $assignment = Format-SafeToolConfAssignment -Key ([string]$key) -Value ([string]$value)
    $lines = Get-Content $confFile
    $found = [bool]($lines | Where-Object { $_ -match "^$key=" } | Select-Object -First 1)
    $lines = $lines | ForEach-Object {
        if ($_ -match "^$key=") {
            $assignment
        } else { $_ }
    }
    if (-not $found) { $lines += $assignment }
    ($lines -join "`n") + "`n" | Set-Content $confFile -NoNewline
}

function Update-GradleWrapper($version) {
    $path = Join-Path (Join-Path (Join-Path $androidDir "gradle") "wrapper") "gradle-wrapper.properties"
    $content = Get-Content $path -Raw
    $content = $content -replace "gradle-[0-9.]+-bin\.zip", "gradle-$version-bin.zip"
    Set-Content $path $content -NoNewline
}

$script:executedInstallCommands = @{}

function Invoke-InstallSyncForDependency($dep, $target) {
    $installCmdKey = $dep.InstallCmdKey
    if (-not $installCmdKey) {
        Write-Host "  Skipping $($dep.Name) install sync: no install command configured"
        return
    }

    $installCmd = Get-ConfValue $installCmdKey
    if (-not $installCmd) {
        Write-Host "  Skipping $($dep.Name) install sync: no install command configured"
        return
    }
    if ($script:executedInstallCommands.ContainsKey($installCmd)) {
        Write-Host "  Skipping duplicate install command for $($dep.Name)"
        return
    }

    Write-Host "  Syncing installed $($dep.Name) to target $target ..."
    Invoke-ConfiguredActionCommand $installCmdKey
    $script:executedInstallCommands[$installCmd] = $true
}

foreach ($item in $selectedTarget) {
    $dep = $item.Dep
    $name = $dep.Name
    $old = if ($dep.ContainsKey("CurrentDisplay")) { $dep.CurrentDisplay } else { $dep.Current }
    $new = $dep.Latest
    $newDisplay = if ($dep.ContainsKey("LatestDisplay")) { $dep.LatestDisplay } else { $dep.Latest }
    $key = $dep.ConfKey

    Write-Host "  Upgrading $name $old -> $newDisplay ..."

    # Always update the conf file
    $confValue = if ($dep.ContainsKey("NewValue")) { $dep.NewValue } else { $new }
    Update-Conf $key $confValue

    switch -Wildcard ($name) {
        "Gradle" {
            Update-GradleWrapper $new
        }
        "Android NDK" {
            # Update NDK_VERSION and NDK_URL in conf
            Update-Conf "NDK_VERSION" $new
            $ndkUrl = Get-NdkUrlForPlatform $new
            Update-Conf "NDK_URL" $ndkUrl
            if ($dep.ContainsKey("LatestFullVersion") -and $dep.LatestFullVersion) {
                Update-Conf "NDK_FULL_VERSION" $dep.LatestFullVersion
            } else {
                Write-Host "    NOTE: Update NDK_FULL_VERSION in tool_versions.conf after install if this lookup fails"
            }
            Write-Host "    Run: helpers/get_ndk.sh to download the updated NDK"
        }
        "Build Tools" {
            Update-Conf "BUILD_TOOLS_VERSION" $new
            Write-Host "    Run helpers/finalize.sh to install the new build tools via sdkmanager"
        }
        "Compile SDK" {
            Update-Conf "COMPILE_SDK" $new
            if ($dep.ContainsKey("InstallCmdKey")) {
                Invoke-InstallSyncForDependency $dep $new
            } else {
                Write-Host "    Run helpers/finalize.sh to install the new Android SDK platform"
            }
        }
        "CMake" {
            $cmakeUrl = Get-CmakeUrlForPlatform $new
            Update-Conf "CMAKE_URL" $cmakeUrl
            Write-Host "    Run helpers/get_cmake.sh and helpers/finalize.sh to install the updated CMake packages"
        }
        "Android SDK cmdline-tools" {
            if ($dep.ContainsKey("InstallCmdKey")) {
                Invoke-InstallSyncForDependency $dep $newDisplay
            } else {
                Write-Host "    Run helpers/get_sdk.sh to download the updated Android SDK command-line tools"
            }
        }
        "JDK*" {
            if ($dep.JDKMajor) {
                # Upgrading major version (e.g. 17 -> 21)
                Update-Conf "JDK_MAJOR" $dep.JDKMajor
                Update-Conf "JDK_VERSION" $new
                $url = Get-JdkUrlForPlatform $dep.JDKMajor
                Update-Conf "JDK_URL" $url
            } else {
                Update-Conf "JDK_VERSION" $new
                Update-Conf "JDK_URL" (Get-JdkUrlForPlatform $currentJDKMajor)
            }
            if ($dep.ContainsKey("InstallCmdKey")) {
                Invoke-InstallSyncForDependency $dep $new
            } else {
                Write-Host "    Run helpers/get_jdk.sh to download the updated JDK"
            }
        }
        "GM Soundfont" {
            Update-Conf "SOUNDFONT_VERSION" $new
            $sfUrl = "https://github.com/arbruijn/TimGM6mb/releases/download/v$new/TimGM6mb.sf2"
            Update-Conf "SOUNDFONT_URL" $sfUrl
            Write-Host "    NOTE: Update SOUNDFONT_SHA256 in tool_versions.conf after downloading"
            Write-Host "    Run: helpers/get_soundfont.sh (it will fail on hash mismatch until you update the hash)"
        }
        "clang-format" {
            if ($dep.ReleaseTag) {
                Update-Conf "CLANG_FORMAT_RELEASE_TAG" $dep.ReleaseTag
            }
            if ($dep.LatestUrl) {
                Update-Conf "CLANG_FORMAT_URL" $dep.LatestUrl
            }
        }
        "shellcheck" {
            $shellcheckUrl = Get-ShellcheckUrlForPlatform $new
            Update-Conf "SHELLCHECK_URL" $shellcheckUrl
        }
        "shfmt" {
            $shfmtUrl = Get-ShfmtUrlForPlatform $new
            Update-Conf "SHFMT_URL" $shfmtUrl
        }
        "ktlint" {
            $ktlintTag = if ($dep.ContainsKey("ReleaseTag")) { $dep.ReleaseTag } else { $new }
            $ktlintUrl = "https://github.com/pinterest/ktlint/releases/download/$ktlintTag/ktlint"
            Update-Conf "KTLINT_URL" $ktlintUrl
        }
        "Chromaprint" {
            $plainVersion = $new -replace '^v', ''
            Update-Conf "CHROMAPRINT_URL" "https://github.com/acoustid/chromaprint/archive/refs/tags/$new.tar.gz"
            Update-Conf "FPCALC_URL" (Get-ChromaprintFpcalcUrlForPlatform $new)
            Update-Conf "FPCALC_DIR_NAME" "fpcalc-$plainVersion"
        }
        "minimp3" {
            Update-Conf "MINIMP3_URL" "https://raw.githubusercontent.com/lieff/minimp3/$new/minimp3.h"
            Update-Conf "MINIMP3_EX_URL" "https://raw.githubusercontent.com/lieff/minimp3/$new/minimp3_ex.h"
        }
        "stb_vorbis" {
            Update-Conf "STB_VORBIS_URL" "https://raw.githubusercontent.com/nothings/stb/$new/stb_vorbis.c"
        }
        "dr_flac" {
            Update-Conf "DR_FLAC_URL" "https://raw.githubusercontent.com/mackron/dr_libs/$new/dr_flac.h"
        }
        "ImageMagick" {
            Update-Conf "IMAGEMAGICK_URL" "https://imagemagick.org/archive/binaries/ImageMagick-$new-portable-Q16-HDRI-x64.7z"
            Update-Conf "IMAGEMAGICK_DIR_NAME" "imagemagick-$new"
        }
        "7-Zip" {
            Update-Conf "SEVENZIP_URL" "https://www.7-zip.org/a/7z$new-extra.7z"
            Update-Conf "SEVENZIP_DIR_NAME" "7z-$new"
        }
        "PowerShell 7" {
            Update-Conf "POWERSHELL_URL" (Get-PowerShellUrlForPlatform $new)
            if ($dep.ContainsKey("InstallCmdKey")) {
                Invoke-InstallSyncForDependency $dep $new
            } else {
                Write-Host "    NOTE: PowerShell is a host tool; run update-powershell.ps1 or update your pwsh package"
            }
        }
    }
}

if ($selectedTarget.Count -gt 0) {
    Refresh-ConfContext
}

foreach ($item in $selectedInstall) {
    $dep = $item.Dep
    $target = if ($conf.ContainsKey($dep.ConfKey)) { $conf[$dep.ConfKey] } else { $dep.Current }

    Invoke-InstallSyncForDependency $dep $target
}

& (Join-Path (Join-Path $scriptDir "helpers") "sync-vscode-java-settings.ps1")

Write-Host ""
if ($selectedTarget.Count -gt 0) {
    Write-Host "tool_versions.conf and related metadata updated"
}
if ($selectedInstall.Count -gt 0) {
    Write-Host "requested install-sync commands completed"
}
Write-Host ""
Write-Host "IMPORTANT NOTES:"
Write-Host "  - Kotlin and Compose Compiler must be compatible"
Write-Host "    See https://developer.android.com/jetpack/androidx/releases/compose-kotlin"
Write-Host "  - For manual NDK/SDK changes, re-run the get_deps install scripts"
Write-Host "  - Run a test build:  $(Get-GradleVerificationCommand)"
Write-Host ""
Write-RunNote "finished" @(
    "result=applied_changes",
    "target_selected_count=$($selectedTarget.Count)",
    "install_selected_count=$($selectedInstall.Count)"
)
