#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/sdk_package_inventory.ps1')
$root = Join-Path $repoRoot ('android/temp/sdk_package_inventory_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix sdk_package_inventory_ -MinimumFreeSpaceGB 0.01
$fixtureLock = $null
function Set-FixturePins([string]$Checkout, [int]$Api, [int]$Compile) {
    Set-Content -LiteralPath (Join-Path $Checkout 'android/get_deps/tool_versions.conf') -Value "COMPILE_SDK=$Compile`nEMULATOR_API_LEVEL=$Api`nBUILD_TOOLS_VERSION=$Compile.0.0`nCMAKE_VERSION=3.31.6"
}
function New-FixturePackage([string]$Id, [string]$DirectoryId = $Id) {
    $path = Join-Path $sdk $DirectoryId.Replace(';', '/')
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $path 'package.xml') -Value "<r:repository xmlns:r='urn:fixture'><localPackage path='$Id'/></r:repository>"
    return $path
}
function Get-FixtureInventory { Get-DxxSdkInventory -RepoRoot $checkouts[0] -AvdRoots @($avdRoot) }
function Assert-Package([object]$Inventory, [string]$Id, [string]$State, [string]$Kind = '') {
    $matches = @($Inventory.Packages | Where-Object PackageId -CEQ $Id)
    if ($matches.Count -ne 1 -or $matches[0].ReferenceState -ne $State) { throw "Unexpected inventory for $Id" }
    if ($Kind -and -not @($matches[0].References | Where-Object Kind -eq $Kind).Count) { throw "Missing $Kind reference for $Id" }
}
function Assert-Rejected([scriptblock]$Operation) {
    $failed = $false
    try { & $Operation | Out-Null } catch { $failed = $true }
    if (-not $failed) { throw 'Incomplete reference discovery was accepted' }
}
try {
    New-Item -ItemType Directory -Path $root -Force | Out-Null
    $fixtureLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $deps = Join-Path $root 'shared tools'
    $sdk = Join-Path $deps 'android-sdk'
    $avdRoot = Join-Path $root 'AVD registry'
    New-Item -ItemType Directory -Path $sdk, $avdRoot -Force | Out-Null
    $checkouts = @((Join-Path $root 'checkout one'), (Join-Path $root 'checkout two'))
    foreach ($checkout in $checkouts) {
        New-Item -ItemType Directory -Path (Join-Path $checkout 'android/get_deps') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $checkout 'dependency_base.txt') -Value $deps
    }
    Set-FixturePins $checkouts[0] 34 37
    Set-FixturePins $checkouts[1] 23 36
    $registry = Join-Path $deps '.dxx-dependency-ownership.json'
    @{ Schema = 1; Repositories = $checkouts } | ConvertTo-Json | Set-Content -LiteralPath $registry
    foreach ($id in @('platforms;android-37.0', 'platforms;android-36', 'system-images;android-34;google_apis;x86_64', 'system-images;android-23;google_apis;x86_64')) {
        New-FixturePackage $id | Out-Null
    }
    New-FixturePackage 'cmdline-tools;19.0' 'cmdline-tools;latest' | Out-Null
    $externalAvd = Join-Path $root 'moved device.avd'
    New-Item -ItemType Directory -Path $externalAvd | Out-Null
    Set-Content -LiteralPath (Join-Path $externalAvd 'config.ini') -Value 'image.sysdir.1=system-images/android-23/google_apis/x86_64/'
    $descriptor = Join-Path $avdRoot 'device.ini'
    Set-Content -LiteralPath $descriptor -Value "path=$externalAvd"
    $inventory = Get-FixtureInventory
    Assert-Package $inventory 'platforms;android-37.0' Referenced Checkout
    Assert-Package $inventory 'platforms;android-36' Referenced Checkout
    Assert-Package $inventory 'cmdline-tools;19.0' Referenced Checkout
    Assert-Package $inventory 'system-images;android-23;google_apis;x86_64' Referenced AVD
    Set-FixturePins $checkouts[1] 34 37
    $inventory = Get-FixtureInventory
    Assert-Package $inventory 'platforms;android-36' Unreferenced
    Assert-Package $inventory 'system-images;android-23;google_apis;x86_64' Referenced AVD
    # A relocated descriptor can also carry an Android-user-home-relative path
    Set-Content -LiteralPath $descriptor -Value 'path.rel=moved device.avd'
    Assert-Package (Get-FixtureInventory) 'system-images;android-23;google_apis;x86_64' Referenced AVD
    Remove-Item -LiteralPath $descriptor
    Assert-Package (Get-FixtureInventory) 'system-images;android-23;google_apis;x86_64' Unreferenced
    if ($inventory.RetirementReady) { throw 'Inventory alone authorized retirement' }
    # Malformed or inconsistent metadata never looks like an ordinary unused package
    $malformed = New-FixturePackage 'build-tools;99.0.0'
    Set-Content -LiteralPath (Join-Path $malformed 'package.xml') -Value '<!DOCTYPE repository [<!ENTITY payload SYSTEM "file:///missing">]><repository>&payload;</repository>'
    Assert-Package (Get-FixtureInventory) 'build-tools;99.0.0' Unknown
    New-FixturePackage 'platforms;android-98' 'platforms;android-99' | Out-Null
    Assert-Package (Get-FixtureInventory) 'platforms;android-98' Unknown
    # Missing registered checkouts or moved AVD data cannot prove absence of use
    Remove-Item -LiteralPath (Join-Path $checkouts[1] 'android/get_deps/tool_versions.conf')
    Assert-Rejected { Get-FixtureInventory }
    Set-FixturePins $checkouts[1] 34 37
    Set-Content -LiteralPath $descriptor -Value "path=$(Join-Path $root 'missing.avd')"
    Assert-Rejected { Get-FixtureInventory }
    Remove-Item -LiteralPath $descriptor
    $link = Join-Path $sdk 'ndk'
    $linkType = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { 'Junction' } else { 'SymbolicLink' }
    New-Item -ItemType $linkType -Path $link -Target $externalAvd | Out-Null
    try { Assert-Rejected { Get-FixtureInventory } } finally { Remove-Item -LiteralPath $link -Force }
    $roots = @(Get-DxxAvdSearchRoots -UserProfile (Join-Path $root 'profile') -Variables @{
            ANDROID_AVD_HOME = (Join-Path $root 'explicit')
            ANDROID_USER_HOME = (Join-Path $root 'user')
            ANDROID_EMULATOR_HOME = (Join-Path $root 'emulator')
            ANDROID_SDK_HOME = (Join-Path $root 'legacy')
        })
    foreach ($expected in @('profile/.android/avd', 'explicit', 'user/avd', 'emulator/avd', 'legacy/.android/avd')) {
        if ((Join-Path $root $expected) -notin $roots) { throw "Missing AVD search root $expected" }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $externalAvd 'config.ini'))) { throw 'Inventory modified external AVD data' }
    Write-Host 'PASS: SDK package metadata, shared pins, API aliases, relocated AVD references, environment roots and incomplete-discovery rejection'
} finally {
    if ($fixtureLock) { $fixtureLock.Dispose() }
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
