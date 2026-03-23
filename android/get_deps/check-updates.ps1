# check-updates.ps1 -- Check for newer versions of ALL Android build dependencies
# and offer to upgrade them.  Updates tool_versions.conf + build files in place.
#
# Checks: AGP, Gradle, Kotlin, Compose Compiler, Compose BOM, AndroidX libs,
#          Android NDK, JDK (OpenJDK), Android SDK build-tools
#
# Usage:  .\check-updates.ps1   (from android/get_deps/)

$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot
$androidDir = Split-Path $scriptDir -Parent
$confFile = Join-Path $scriptDir "tool_versions.conf"

# -- Load tool_versions.conf --------------------------------------------------

function Load-Conf {
    $cfg = @{}
    foreach ($line in Get-Content $confFile) {
        $line = $line.Trim()
        if ($line -match '^([A-Z_]+)=(.+)$') {
            $cfg[$Matches[1]] = $Matches[2]
        }
    }
    return $cfg
}

$conf = Load-Conf

# -- Helpers ------------------------------------------------------------------

function Get-LatestMavenVersion($group, $artifact) {
    $groupPath = $group -replace '\.', '/'
    foreach ($repo in @("https://dl.google.com/dl/android/maven2",
            "https://repo1.maven.org/maven2")) {
        $url = "$repo/$groupPath/$artifact/maven-metadata.xml"
        try {
            $xml = [xml](Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 10).Content
            $ver = $xml.metadata.versioning.release
            if ($ver) { return $ver }
        } catch {}
    }
    return $null
}

function Get-LatestGradleVersion {
    try {
        $json = (Invoke-WebRequest -Uri "https://services.gradle.org/versions/current" `
                -UseBasicParsing -TimeoutSec 10).Content | ConvertFrom-Json
        return $json.version
    } catch { return $null }
}

function Get-LatestNDKVersion {
    # Scrape the NDK download page for the latest version tag
    try {
        $page = (Invoke-WebRequest -Uri "https://developer.android.com/ndk/downloads" `
                -UseBasicParsing -TimeoutSec 15).Content
        # Look for "android-ndk-r<VER>-windows.zip" pattern
        if ($page -match 'android-ndk-(r\d+[a-z]?)-windows\.zip') {
            return $Matches[1]
        }
    } catch {}
    return $null
}

function Get-LatestJDKVersion {
    # Check Adoptium (Eclipse Temurin) for latest JDK 17 and 21
    $versions = @()
    foreach ($major in @(17, 21)) {
        try {
            $url = "https://api.adoptium.net/v3/info/release_versions?architecture=x64&heap_size=normal&image_type=jdk&os=windows&page=0&page_size=1&project=jdk&release_type=ga&sort_method=DEFAULT&sort_order=DESC&vendor=eclipse&version=%5B${major}%2C$($major+1)%29"
            $json = (Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 10).Content | ConvertFrom-Json
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
    # Use sdkmanager --list to find latest build-tools (if available)
    $sdkDir = $null
    $_depBaseFile = Join-Path (Split-Path (Split-Path $PSScriptRoot)) "dependency_base.txt"
    if (Test-Path $_depBaseFile) {
        $DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
        $candidate = Join-Path $DEP_BASE "android-sdk"
        if (Test-Path $candidate) { $sdkDir = $candidate }
    }
    if (-not $sdkDir) { return $null }

    $sdkmanager = Join-Path $sdkDir "cmdline-tools\latest\bin\sdkmanager.bat"
    if (-not (Test-Path $sdkmanager)) { return $null }

    try {
        $output = & $sdkmanager --list 2>$null | Select-String "build-tools;" |
            ForEach-Object { if ($_ -match 'build-tools;(\d+\.\d+\.\d+)\s') { $Matches[1] } } |
            Sort-Object { [version]$_ } -Descending | Select-Object -First 1
        return $output
    } catch { return $null }
}

function Get-LatestSoundfontVersion {
    # Check the GitHub releases API for arbruijn/TimGM6mb
    try {
        $headers = @{ "Accept" = "application/vnd.github.v3+json" }
        $url = "https://api.github.com/repos/arbruijn/TimGM6mb/releases/latest"
        $json = (Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 10 -Headers $headers).Content |
            ConvertFrom-Json
        $tag = $json.tag_name -replace '^v', ''
        return $tag
    } catch { return $null }
}

# -- Fetch all versions -------------------------------------------------------

Write-Host ""
Write-Host "Checking for updates..."
Write-Host ""

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
        Latest = Get-LatestNDKVersion
    },

    @{ Name = "Build Tools"; ConfKey = "BUILD_TOOLS_VERSION";
        Current = $conf["BUILD_TOOLS_VERSION"];
        Latest = Get-LatestBuildToolsVersion
    },

    @{ Name = "GM Soundfont"; ConfKey = "SOUNDFONT_VERSION";
        Current = $conf["SOUNDFONT_VERSION"];
        Latest = Get-LatestSoundfontVersion
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

# -- Display table ------------------------------------------------------------

$upgradeable = @()
$i = 1

Write-Host ("{0,-25} {1,-18} {2,-18} {3}" -f "Dependency", "Current", "Latest", "Status")
Write-Host ("{0,-25} {1,-18} {2,-18} {3}" -f ("-" * 25), ("-" * 18), ("-" * 18), ("-" * 20))

foreach ($dep in $deps) {
    $latest = $dep.Latest
    if (-not $latest) { $latest = "???" }

    if ($latest -eq "???" -or $dep.Current -eq "unknown") {
        $status = "?"
    } elseif ($dep.Current -eq $latest) {
        $status = "up-to-date"
    } else {
        $status = "[$i] upgrade available"
        $upgradeable += @{ Index = $i; Dep = $dep }
        $i++
    }

    Write-Host ("{0,-25} {1,-18} {2,-18} {3}" -f $dep.Name, $dep.Current, $latest, $status)
}

if ($upgradeable.Count -eq 0) {
    Write-Host ""
    Write-Host "Everything is up to date!"
    return
}

# -- Prompt -------------------------------------------------------------------

Write-Host ""
Write-Host "Enter numbers to upgrade (comma-separated), 'a' for all, or Enter to skip:"
$input_str = Read-Host "Upgrade"

if ([string]::IsNullOrWhiteSpace($input_str)) {
    Write-Host "No changes made."
    return
}

$selected = @()
if ($input_str.Trim().ToLower() -eq 'a') {
    $selected = $upgradeable
} else {
    $nums = $input_str -split '[,\s]+' | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^\d+$' }
    foreach ($n in $nums) {
        $match = $upgradeable | Where-Object { $_.Index -eq [int]$n }
        if ($match) { $selected += $match }
    }
}

if ($selected.Count -eq 0) {
    Write-Host "No valid selections."
    return
}

# -- Apply upgrades -----------------------------------------------------------

function Update-Conf($key, $value) {
    $lines = Get-Content $confFile
    $found = $false
    $lines = $lines | ForEach-Object {
        if ($_ -match "^$key=") {
            $found = $true
            "$key=$value"
        } else { $_ }
    }
    if (-not $found) { $lines += "$key=$value" }
    Set-Content $confFile $lines
}

function Update-BuildGradle($pattern, $replacement) {
    $path = "$androidDir\build.gradle"
    $content = Get-Content $path -Raw
    $content = $content -replace $pattern, $replacement
    Set-Content $path $content -NoNewline
}

function Update-AppBuildGradle($pattern, $replacement) {
    $path = "$androidDir\app\build.gradle"
    $content = Get-Content $path -Raw
    $content = $content -replace $pattern, $replacement
    Set-Content $path $content -NoNewline
}

function Update-GradleWrapper($version) {
    $path = "$androidDir\gradle\wrapper\gradle-wrapper.properties"
    $content = Get-Content $path -Raw
    $content = $content -replace "gradle-[0-9.]+-bin\.zip", "gradle-$version-bin.zip"
    Set-Content $path $content -NoNewline
}

foreach ($item in $selected) {
    $dep = $item.Dep
    $name = $dep.Name
    $old = $dep.Current
    $new = $dep.Latest
    $key = $dep.ConfKey

    Write-Host "  Upgrading $name $old -> $new ..."

    # Always update the conf file
    Update-Conf $key $new

    switch -Wildcard ($name) {
        "Android Gradle Plugin" {
            Update-BuildGradle "com\.android\.application.*version\s+'[^']+'" `
                "com.android.application' version '$new'"
        }
        "Gradle" {
            Update-GradleWrapper $new
        }
        "Kotlin" {
            Update-BuildGradle "org\.jetbrains\.kotlin\.android.*version\s+'[^']+'" `
                "org.jetbrains.kotlin.android' version '$new'"
        }
        "Compose Compiler" {
            Update-AppBuildGradle "kotlinCompilerExtensionVersion\s+'[^']+'" `
                "kotlinCompilerExtensionVersion '$new'"
        }
        "Compose BOM" {
            Update-AppBuildGradle "compose-bom:[^']+" "compose-bom:$new"
        }
        "core-ktx" {
            Update-AppBuildGradle "core-ktx:[^']+" "core-ktx:$new"
        }
        "appcompat" {
            Update-AppBuildGradle "appcompat:[^']+" "appcompat:$new"
        }
        "activity-compose" {
            Update-AppBuildGradle "activity-compose:[^']+" "activity-compose:$new"
        }
        "Android NDK" {
            # Update NDK_VERSION and NDK_URL in conf
            Update-Conf "NDK_VERSION" $new
            $ndkUrl = "https://dl.google.com/android/repository/android-ndk-$new-windows.zip"
            Update-Conf "NDK_URL" $ndkUrl
            # Note: NDK_FULL_VERSION must be set manually after install
            # (it's the numeric version from source.properties)
            Write-Host "    NOTE: After installing the new NDK, update NDK_FULL_VERSION in tool_versions.conf"
            Write-Host "    Run: get_ndk.sh to download, then check source.properties for the numeric version"
        }
        "Build Tools" {
            Update-Conf "BUILD_TOOLS_VERSION" $new
            Write-Host "    Run finalize.sh to install the new build tools via sdkmanager"
        }
        "JDK*" {
            if ($dep.JDKMajor) {
                # Upgrading major version (e.g. 17 -> 21)
                Update-Conf "JDK_MAJOR" $dep.JDKMajor
                Update-Conf "JDK_VERSION" $new
                $url = "https://api.adoptium.net/v3/binary/latest/$($dep.JDKMajor)/ga/windows/x64/jdk/hotspot/normal/eclipse?project=jdk"
                Update-Conf "JDK_URL" $url
                Write-Host "    NOTE: Run get_jdk.sh to download JDK $($dep.JDKMajor)"
            } else {
                Update-Conf "JDK_VERSION" $new
                Write-Host "    NOTE: Run get_jdk.sh to download the updated JDK"
            }
        }
        "GM Soundfont" {
            Update-Conf "SOUNDFONT_VERSION" $new
            $sfUrl = "https://github.com/arbruijn/TimGM6mb/releases/download/v$new/TimGM6mb.sf2"
            Update-Conf "SOUNDFONT_URL" $sfUrl
            Write-Host "    NOTE: Update SOUNDFONT_SHA256 in tool_versions.conf after downloading."
            Write-Host "    Run: get_soundfont.sh (it will fail on hash mismatch until you update the hash)"
        }
    }
}

Write-Host ""
Write-Host "tool_versions.conf and build files updated."
Write-Host ""
Write-Host "IMPORTANT NOTES:"
Write-Host "  - Kotlin and Compose Compiler must be compatible."
Write-Host "    See https://developer.android.com/jetpack/androidx/releases/compose-kotlin"
Write-Host "  - For NDK/JDK/SDK changes, re-run the get_deps install scripts."
Write-Host "  - Run a test build:  cd ..; .\gradlew.bat assembleDebug"
Write-Host ""
