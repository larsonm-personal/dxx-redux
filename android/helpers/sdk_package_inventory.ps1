# Read-only SDK package/reference inventory for planning managed package retirement
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'verified_dependencies.ps1')

function Assert-DxxPlainSdkPath([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Cannot assess linked SDK/reference path: $cursor"
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}

function Get-DxxAvdSearchRoots {
    param(
        [string]$UserProfile = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile),
        [System.Collections.IDictionary]$Variables
    )
    if ($null -eq $Variables) { $Variables = [Environment]::GetEnvironmentVariables() }
    $roots = @((Join-Path $UserProfile '.android/avd'))
    if ($Variables['ANDROID_AVD_HOME']) { $roots += [string]$Variables['ANDROID_AVD_HOME'] }
    foreach ($key in @('ANDROID_USER_HOME', 'ANDROID_EMULATOR_HOME')) {
        if ($Variables[$key]) { $roots += Join-Path ([string]$Variables[$key]) 'avd' }
    }
    if ($Variables['ANDROID_SDK_HOME']) { $roots += Join-Path ([string]$Variables['ANDROID_SDK_HOME']) '.android/avd' }
    @($roots | ForEach-Object { [IO.Path]::GetFullPath($_) } | Select-Object -Unique)
}

function Read-DxxSdkIni([string]$Path) {
    Assert-DxxPlainSdkPath $Path
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path -ErrorAction Stop) {
        if ($line -match '^\s*([^#;=]+?)\s*=\s*(.*?)\s*$') {
            $key = $Matches[1]
            if ($values.ContainsKey($key)) { throw "Duplicate configuration key $key in $Path" }
            $values[$key] = $Matches[2]
        }
    }
    $values
}

function Get-DxxSdkInventory {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [string[]]$AvdRoots
    )
    $RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
    $comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    $baseFile = Join-Path $RepoRoot 'dependency_base.txt'
    Assert-DxxPlainSdkPath $baseFile
    $dependencyRoot = [IO.Path]::GetFullPath((Get-Content -LiteralPath $baseFile -First 1 -ErrorAction Stop).Trim())
    $sdkRoot = Join-Path $dependencyRoot 'android-sdk'
    Assert-DxxPlainSdkPath $sdkRoot
    if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) { throw "SDK not installed: $sdkRoot" }
    $checkouts = @($RepoRoot)
    $ownership = Join-Path $dependencyRoot '.dxx-dependency-ownership.json'
    Assert-DxxPlainSdkPath $ownership
    if (Test-Path -LiteralPath $ownership) {
        $state = Get-Content -LiteralPath $ownership -Raw -ErrorAction Stop | ConvertFrom-Json
        if ($state.Schema -ne 1) { throw 'Unsupported dependency ownership schema' }
        $checkouts += @($state.Repositories)
    }
    $references = [Collections.Generic.List[object]]::new()
    $relevantCheckouts = @()
    foreach ($checkout in @($checkouts | Select-Object -Unique)) {
        $otherBaseFile = Join-Path $checkout 'dependency_base.txt'
        Assert-DxxPlainSdkPath $otherBaseFile
        $otherBase = [IO.Path]::GetFullPath((Get-Content -LiteralPath $otherBaseFile -First 1 -ErrorAction Stop).Trim())
        if (-not $otherBase.Equals($dependencyRoot, $comparison)) { continue }
        $relevantCheckouts += $checkout
        Assert-DxxPlainSdkPath (Join-Path $checkout 'android/get_deps/tool_versions.conf')
        $config = Read-DxxDependencyConfig -RepoRoot $checkout
        foreach ($key in @('COMPILE_SDK', 'EMULATOR_API_LEVEL', 'BUILD_TOOLS_VERSION', 'CMAKE_VERSION')) {
            if (-not $config.ContainsKey($key) -or $config[$key] -notmatch '^\d+(\.\d+)*$') {
                throw "Missing or invalid SDK pin $key in $checkout"
            }
        }
        $ids = @('emulator', 'platform-tools', 'cmdline-tools;latest', "build-tools;$($config.BUILD_TOOLS_VERSION)", "cmake;$($config.CMAKE_VERSION)",
            "system-images;android-$($config.EMULATOR_API_LEVEL);google_apis;x86_64")
        foreach ($api in @($config.COMPILE_SDK, $config.EMULATOR_API_LEVEL) | Select-Object -Unique) {
            $ids += "platforms;android-$api"
            # SDK repositories have used both integer and .0 platform paths
            if ($api -notmatch '\.') { $ids += "platforms;android-$api.0" }
        }
        if ($config.ContainsKey('NDK_FULL_VERSION')) {
            if ($config.NDK_FULL_VERSION -notmatch '^\d+(\.\d+)*$') { throw "Invalid NDK pin in $checkout" }
            $ids += "ndk;$($config.NDK_FULL_VERSION)"
        }
        foreach ($id in $ids) { $references.Add([pscustomobject]@{ PackageId = $id; Kind = 'Checkout'; Source = $checkout }) }
    }
    if (-not $AvdRoots) { $AvdRoots = @(Get-DxxAvdSearchRoots) }
    $avdDirectories = @()
    foreach ($root in $AvdRoots) {
        Assert-DxxPlainSdkPath $root
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $avdDirectories += @(Get-ChildItem -LiteralPath $root -Directory -Filter '*.avd' -ErrorAction Stop | ForEach-Object FullName)
        foreach ($descriptor in Get-ChildItem -LiteralPath $root -File -Filter '*.ini' -ErrorAction Stop) {
            $values = Read-DxxSdkIni $descriptor.FullName
            if ($values['path']) {
                if (-not [IO.Path]::IsPathRooted($values['path'])) { throw "AVD path must be absolute: $($descriptor.FullName)" }
                $avdDirectories += $values['path']
            } elseif ($values['path.rel']) {
                $avdDirectories += [IO.Path]::GetFullPath((Join-Path (Split-Path $root) $values['path.rel']))
            } else { throw "Missing AVD path in $($descriptor.FullName)" }
        }
    }
    foreach ($directory in @($avdDirectories | Select-Object -Unique)) {
        $configPath = Join-Path $directory 'config.ini'
        $config = Read-DxxSdkIni $configPath
        $keys = @($config.Keys | Where-Object { $_ -match '^image\.sysdir\.\d+$' })
        if (-not $keys.Count) { throw "Missing system image reference in $configPath" }
        foreach ($key in $keys) {
            $raw = $config[$key].Replace('\', '/')
            if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -and $raw -match '^[A-Za-z]:') { throw "Foreign-host AVD image path in $configPath" }
            $path = if ([IO.Path]::IsPathRooted($raw)) { [IO.Path]::GetFullPath($raw) } else { [IO.Path]::GetFullPath((Join-Path $sdkRoot $raw)) }
            if (-not $path.StartsWith($sdkRoot + [IO.Path]::DirectorySeparatorChar, $comparison)) { continue }
            $id = $path.Substring($sdkRoot.Length + 1).TrimEnd('/', '\').Replace('\', ';').Replace('/', ';')
            if ($id -notmatch '^system-images;android-[A-Za-z0-9._+-]+;[A-Za-z0-9._+-]+;[A-Za-z0-9._+-]+$') { throw "Unrecognized AVD image path in $configPath" }
            $references.Add([pscustomobject]@{ PackageId = $id; Kind = 'AVD'; Source = $configPath })
        }
    }
    $directories = @()
    foreach ($family in @('build-tools', 'platforms', 'cmake', 'ndk', 'cmdline-tools')) {
        $parent = Join-Path $sdkRoot $family
        Assert-DxxPlainSdkPath $parent
        if (Test-Path -LiteralPath $parent) {
            $directories += @(Get-ChildItem -LiteralPath $parent -Directory -ErrorAction Stop | Where-Object Name -NotLike '.*' | ForEach-Object FullName)
        }
    }
    foreach ($name in @('platform-tools', 'emulator')) {
        $path = Join-Path $sdkRoot $name
        if (Test-Path -LiteralPath $path) { $directories += $path }
    }
    $imageRoot = Join-Path $sdkRoot 'system-images'
    Assert-DxxPlainSdkPath $imageRoot
    if (Test-Path -LiteralPath $imageRoot) {
        foreach ($api in Get-ChildItem -LiteralPath $imageRoot -Directory -ErrorAction Stop) {
            Assert-DxxPlainSdkPath $api.FullName
            foreach ($tag in Get-ChildItem -LiteralPath $api.FullName -Directory -ErrorAction Stop) {
                Assert-DxxPlainSdkPath $tag.FullName
                $directories += @(Get-ChildItem -LiteralPath $tag.FullName -Directory -ErrorAction Stop | ForEach-Object FullName)
            }
        }
    }
    $packages = foreach ($directory in $directories) {
        Assert-DxxPlainSdkPath $directory
        $relative = $directory.Substring($sdkRoot.Length + 1).Replace('\', '/').Replace('/', ';')
        $metadata = Join-Path $directory 'package.xml'
        Assert-DxxPlainSdkPath $metadata
        $problem = $null
        $id = $relative
        if (-not (Test-Path -LiteralPath $metadata -PathType Leaf)) { $problem = 'Missing package.xml' }
        else {
            $settings = [Xml.XmlReaderSettings]::new()
            $settings.DtdProcessing = [Xml.DtdProcessing]::Prohibit
            $settings.XmlResolver = $null
            $settings.MaxCharactersInDocument = 8MB
            $reader = $null
            try {
                $reader = [Xml.XmlReader]::Create($metadata, $settings)
                $document = [Xml.XmlDocument]::new()
                $document.XmlResolver = $null
                $document.Load($reader)
                $nodes = $document.SelectNodes('/*[local-name()="repository"]/*[local-name()="localPackage"]')
                if ($nodes.Count -ne 1) { throw 'Expected one localPackage record' }
                $id = $nodes[0].GetAttribute('path')
                # The SDK's latest alias often contains a numerically versioned package
                if ($id -cne $relative -and -not ($relative -eq 'cmdline-tools;latest' -and $id -match '^cmdline-tools;\d+(\.\d+)*$')) {
                    throw "Package identity $id does not match directory $relative"
                }
            } catch { $problem = $_.Exception.Message }
            finally { if ($reader) { $reader.Dispose() } }
        }
        $uses = @($references | Where-Object { $_.PackageId -ceq $id -or $_.PackageId -ceq $relative })
        [pscustomobject]@{
            PackageId = $id; Path = $directory; MetadataProblem = $problem
            ReferenceState = if ($problem) { 'Unknown' } elseif ($uses.Count) { 'Referenced' } else { 'Unreferenced' }
            References = $uses
        }
    }
    [pscustomobject]@{
        SdkRoot = $sdkRoot; AvdRoots = $AvdRoots; Checkouts = $relevantCheckouts; Packages = @($packages | Sort-Object PackageId)
        References = @($references.ToArray())
        Coverage = @('registered checkout configuration', 'visible AVD configuration')
        RetirementReady = $false
    }
}
