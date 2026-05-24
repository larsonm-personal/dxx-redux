#!/usr/bin/env pwsh
# generate-keystore.ps1 -- Generate a release keystore for signing AABs.
# Usage: .\generate-keystore.ps1
$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot
try {
    # Find keytool via JAVA_HOME or auto-detect
    $javaHome = $env:JAVA_HOME
    if (-not $javaHome) {
        $_depBaseFile = Join-Path (Split-Path $PSScriptRoot) "dependency_base.txt"
        if (-not (Test-Path $_depBaseFile)) {
            Write-Error "dependency_base.txt not found at $_depBaseFile. Create it with a single line containing the path to your dependency directory (e.g. C:\local)."
            exit 1
        }
        $DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
        $jdk = Get-ChildItem "$DEP_BASE\jdk-*" -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($jdk) {
            $javaHome = $jdk.FullName
        } else {
            Write-Error "No JDK found in $DEP_BASE\jdk-*. Set JAVA_HOME manually"
        }
    }

    $keytool = Join-Path $javaHome "bin\keytool.exe"
    if (-not (Test-Path $keytool)) {
        Write-Error "keytool not found at $keytool"
    }

    $keystoreFile = "release.keystore"
    if (Test-Path $keystoreFile) {
        Write-Host "Keystore already exists: $keystoreFile"
        Write-Host "Delete it first if you want to regenerate"
        exit 1
    }

    Write-Host "Using keytool: $keytool"
    Write-Host ""
    & $keytool -genkeypair -v -keystore $keystoreFile -alias dxxredux `
        -keyalg RSA -keysize 2048 -validity 10000

    if ($LASTEXITCODE -ne 0) { throw "keytool failed with exit code $LASTEXITCODE" }

    Write-Host ""
    Write-Host "Keystore created: $keystoreFile"
    Write-Host ""
    Write-Host "Now copy keystore.properties.example to keystore.properties"
    Write-Host "and fill in the passwords you just entered"
} finally {
    Pop-Location
}
