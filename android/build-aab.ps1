# build-aab.ps1 — Build an AAB with all ABIs and copy to build-outputs/
# Usage: .\build-aab.ps1

$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot
try {
    # Set JAVA_HOME if not already set
    if (-not $env:JAVA_HOME) {
        $jdk = Get-ChildItem "C:\local\jdk-*" -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($jdk) {
            $env:JAVA_HOME = $jdk.FullName
            Write-Host "JAVA_HOME = $env:JAVA_HOME"
        } else {
            Write-Error "No JDK found in C:\local\jdk-*. Set JAVA_HOME manually."
        }
    }

    # Prompt for build type
    Write-Host ""
    Write-Host "Select build type:"
    Write-Host "  1) Debug"
    Write-Host "  2) Release (signed, for Play Console)"
    Write-Host ""
    $choice = Read-Host "Enter choice [2]"
    if ($choice -eq '1') {
        $variant = "Debug"
    } else {
        $variant = "Release"
    }
    $task = "bundle$variant"

    $versionCode = (git rev-list --count HEAD).Trim()
    Write-Host ""
    Write-Host "versionCode: $versionCode (git commits)"
    Write-Host "Building AAB ($variant) for armeabi-v7a, arm64-v8a, x86_64..."
    Write-Host ""
    & .\gradlew.bat $task
    if ($LASTEXITCODE -ne 0) { throw "Gradle build failed with exit code $LASTEXITCODE" }

    # Find the AAB
    $variantLower = $variant.ToLower()
    $aabDir = "app\build\outputs\bundle\$variantLower"
    $aab = Get-ChildItem "$aabDir\*.aab" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $aab) { throw "AAB not found in $aabDir" }

    # Copy to build-outputs/ with timestamp
    $outDir = "build-outputs"
    if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $outName = "dxx-redux-$variantLower-$timestamp-v$versionCode.aab"
    $outPath = Join-Path $outDir $outName

    Copy-Item $aab.FullName $outPath
    $sizeMB = [math]::Round((Get-Item $outPath).Length / 1MB, 1)

    Write-Host ""
    Write-Host "AAB built successfully: $outPath ($sizeMB MB)"
    Write-Host ""
} finally {
    Pop-Location
}
