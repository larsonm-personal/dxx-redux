#!/usr/bin/env pwsh
# extract_dos_demos.ps1 -- Extract game files from DOS shareware demo installers using DOSBox-X.
#
# For each ZIP-compatible archive in game_data\demo installers\, this script:
#   1. Extracts the archive to a temp directory
#   2. Runs the DOS INSTALL.EXE inside DOSBox-X with automated keystrokes
#   3. Copies the resulting game files (.HOG, .PIG, etc.) to an _extracted\ folder
#
# Requires: DOSBox-X installed via android\get_deps\get_dosbox.sh
# Usage:    .\extract_dos_demos.ps1 [-Force]
param(
    [switch]$Force  # Re-extract even if output already has game files
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression.FileSystem

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir
$DemoDir = Join-Path $ScriptDir "demo installers"
. (Join-Path $RepoRoot 'android\helpers\bounded_extraction.ps1')
. (Join-Path $RepoRoot 'android\helpers\verified_dependencies.ps1')

$DosboxExe = Resolve-DxxVerifiedDependencyExecutable -RepoRoot $RepoRoot `
    -DirectoryKey 'DOSBOX_DIR_NAME' -RelativePath 'dosbox-x.exe' `
    -Sha256Key 'DOSBOX_EXE_SHA256' -Label 'DOSBox-X'
$DosboxDir = Split-Path $DosboxExe

# --- Game file extensions to extract ---
$GameExtensions = @("*.hog", "*.pig", "*.ham", "*.mvl", "*.s11", "*.s22",
    "*.dem", "*.256", "*.clr", "*.sng", "*.bnk", "*.txt")

# --- Per-installer configurations ---
# Each entry: zip name, installer exe, expected game files to poll for, and stdin bytes.
# The installer is run via batch file with stdin redirection (INSTALL.EXE < INPUT.TXT).
# Once the expected game files appear on the target drive, DOSBox is killed immediately
# (the installers launch post-install sound card config that we don't need).
$Installers = @(
    @{
        Zip       = "desc14sw.exe"
        Sha256    = "3dadb7fbc01efce2904d0908c55d9a9cf1f402e83bf771970552efaca15efcb0"
        Exe       = "INSTALL.EXE"
        StdinKeys = @([char]13, [char]10, [char]67, [char]13, [char]10) +
        (1..50 | ForEach-Object { [char]13; [char]10 })
        ExpectFiles = @("DESCENT.HOG", "DESCENT.PIG")
    },
    @{
        Zip       = "descent 1 demo 1-4.zip"
        Sha256    = "64741386ad88d7a60a9529383affb4d2415e11d907ea6dbab8a8a66e1c20b745"
        Exe       = "INSTALL.EXE"
        # Stdin sequence: Enter (welcome), C (drive letter), Enter, then many Enters
        # for remaining prompts including post-install sound card config
        StdinKeys = @([char]13, [char]10, [char]67, [char]13, [char]10) +
        (1..50 | ForEach-Object { [char]13; [char]10 })
        ExpectFiles = @("DESCENT.HOG", "DESCENT.PIG")
    },
    @{
        Zip       = "descent 2 demo 1-0.zip"
        Sha256    = "a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d"
        Exe       = "INSTALL.EXE"
        # Stdin sequence: Enter (welcome), C (drive letter), Enter, then many Enters
        # for remaining prompts including post-install sound card detection/config
        StdinKeys = @([char]13, [char]10, [char]67, [char]13, [char]10) +
        (1..50 | ForEach-Object { [char]13; [char]10 })
        ExpectFiles = @("D2DEMO.HOG", "D2DEMO.PIG", "D2DEMO.HAM", "D2DEMO.DEM")
    },
    @{
        Zip       = "d2demo10.zip"
        Sha256    = "f8d005670fe5cd17e07ca9bf4022f1045aed436639c37f1e83dd647e14fcec1f"
        Exe       = "INSTALL.EXE"
        StdinKeys = @([char]13, [char]10, [char]67, [char]13, [char]10) +
        (1..50 | ForEach-Object { [char]13; [char]10 })
        ExpectFiles = @("D2DEMO.HOG", "D2DEMO.PIG", "D2DEMO.HAM", "D2DEMO.DEM")
    }
)

$TimeoutSec = 120
$PollIntervalMs = 2000
$failures = 0

function Expand-ZipCompatibleArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    Expand-BoundedZipArchive -ArchivePath $ArchivePath -DestinationPath $DestinationPath
}

foreach ($inst in $Installers) {
    $zipName = $inst.Zip
    $zipPath = Join-Path $DemoDir $zipName
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($zipName)
    $outputDir = Join-Path $DemoDir "${baseName}_extracted"

    if (-not (Test-Path $zipPath)) {
        Write-Warning "Zip not found, skipping: $zipPath"
        continue
    }
    Assert-DxxFileSha256 -Path $zipPath -ExpectedSha256 $inst.Sha256 `
        -Label "$zipName demo package" | Out-Null

    # Skip if already extracted (unless -Force)
    if ((Test-ExtractionCompletionManifest -Directory $outputDir) -and -not $Force) {
        Write-Host "$baseName already extracted. Use -Force to redo"
        continue
    }

    Write-Host "`n=== $baseName ===" -ForegroundColor Cyan

    # Create temp dirs (no spaces in path for DOSBox compatibility)
    $safeName = $baseName -replace '[^a-zA-Z0-9_-]', '_'
    $tempBase = Join-Path $env:TEMP "dxx_dosbox_${safeName}_$([Guid]::NewGuid().ToString('N'))"
    $sourceDir = Join-Path $tempBase "src"    # extracted zip (DOSBox D:)
    $targetDir = Join-Path $tempBase "dst"    # DOSBox C: -- installer writes here

    # Create one uniquely owned workspace for this installer
    New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null

    $proc = $null
    try {
    # Extract zip
    Write-Host "  Extracting $zipName"
    Expand-ZipCompatibleArchive -ArchivePath $zipPath -DestinationPath $sourceDir

    # Find INSTALL.EXE (might be in root or subdirectory)
    $installerExe = Get-ChildItem $sourceDir -Recurse -Filter $inst.Exe -File | Select-Object -First 1
    if (-not $installerExe) {
        Write-Warning "  $($inst.Exe) not found in $zipName. Skipping"
        $failures++
        continue
    }
    $installerDir = $installerExe.DirectoryName

    # Build INPUT.TXT with stdin bytes for the installer
    $inputBytes = New-Object System.Collections.Generic.List[byte]
    foreach ($ch in $inst.StdinKeys) { $inputBytes.Add([byte]$ch) }
    $inputPath = Join-Path $installerDir "INPUT.TXT"
    [System.IO.File]::WriteAllBytes($inputPath, $inputBytes.ToArray())

    # Build a batch file that redirects stdin into the installer
    $batchContent = "@echo off`r`nINSTALL.EXE < INPUT.TXT`r`n"
    $batchPath = Join-Path $installerDir "RUNINST.BAT"
    [System.IO.File]::WriteAllText($batchPath, $batchContent, [System.Text.Encoding]::ASCII)

    # Build DOSBox-X config (stdin redirect via batch file)
    $targetFwd = $targetDir -replace '\\', '/'
    $installerFwd = $installerDir -replace '\\', '/'

    $confLines = @(
        "[sdl]"
        "output=surface"
        "windowresolution=640x400"
        ""
        "[dosbox]"
        "machine=svga_s3"
        ""
        "[cpu]"
        "cycles=max"
        ""
        "[autoexec]"
        "MOUNT C `"$targetFwd`""
        "MOUNT D `"$installerFwd`""
        "D:"
        "RUNINST.BAT"
        "EXIT"
    )
    $confContent = ($confLines -join "`r`n") + "`r`n"

    $confPath = Join-Path $tempBase "dosbox.conf"
    [System.IO.File]::WriteAllText($confPath, $confContent, [System.Text.Encoding]::ASCII)

    # Run DOSBox-X and poll for game files
    Write-Host ("  Running {0} in DOSBox-X (polling for game files, max {1}s)..." -f $inst.Exe, $TimeoutSec)
    $proc = Start-Process -FilePath $DosboxExe `
        -ArgumentList "-conf", "`"$confPath`"", "-exit", "-fastlaunch", "-nopromptfolder" `
        -WorkingDirectory $DosboxDir `
        -PassThru -WindowStyle Minimized

    # Poll: wait for expected game files to appear and stabilize on the target drive
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $filesFound = $false
    $lastSizes = @{}
    $stableCount = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds $PollIntervalMs
        if ($proc.HasExited) { break }
        $allPresent = $true
        $currentSizes = @{}
        foreach ($expect in $inst.ExpectFiles) {
            $m = Get-ChildItem $targetDir -Recurse -Filter $expect -File -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if (-not $m) { $allPresent = $false; break }
            $currentSizes[$expect] = $m.Length
        }
        if (-not $allPresent) { continue }

        # Check if file sizes have stabilized (same as previous poll)
        $sizesSame = $true
        foreach ($expect in $inst.ExpectFiles) {
            if ($lastSizes[$expect] -ne $currentSizes[$expect]) { $sizesSame = $false; break }
        }
        $lastSizes = $currentSizes
        if ($sizesSame) { $stableCount++ } else { $stableCount = 0 }

        if ($stableCount -ge 2) {
            Write-Host "  Game files detected and stable - killing DOSBox"
            $filesFound = $true
            break
        }
    }

    if (-not $proc.HasExited) {
        $proc.Kill()
        Start-Sleep -Seconds 1
        if (-not $filesFound) {
            Write-Warning "  DOSBox-X timed out after ${TimeoutSec}s without expected game files"
        }
    }

    # Search for game files in the target directory
    Write-Host "  Scanning for game files..."
    $found = @()
    foreach ($ext in $GameExtensions) {
        $found += Get-ChildItem $targetDir -Recurse -Filter $ext -File -ErrorAction SilentlyContinue
    }

    if ($found.Count -eq 0) {
        Write-Warning "  No game files found. The installer may need different keystrokes"
        Write-Host "  Contents of DOSBox C: drive:"
        Get-ChildItem $targetDir -Recurse -File | ForEach-Object {
            $rel = $_.FullName.Substring($targetDir.Length)
            Write-Host "    $rel"
        }
        $failures++
    } else {
        $foundByName = @{}
        $collision = $false
        foreach ($f in $found) {
            $key = $f.Name.ToLowerInvariant()
            if ($foundByName.ContainsKey($key)) { $collision = $true }
            $foundByName[$key] = $f
        }
        $missing = @($inst.ExpectFiles | Where-Object { -not $foundByName.ContainsKey($_.ToLowerInvariant()) })
        $installerCompleted = $filesFound -or ($proc.HasExited -and $proc.ExitCode -eq 0)
        if (-not $installerCompleted -or $missing.Count -gt 0 -or $collision) {
            Write-Warning "  Extraction incomplete or contains colliding basenames"
            $failures++
            continue
        }
        $stagingDir = Join-Path $DemoDir ".${baseName}_extracted-$([Guid]::NewGuid().ToString('N'))"
        New-Item -ItemType Directory -Path $stagingDir | Out-Null
        foreach ($f in $foundByName.Values) {
            Copy-Item $f.FullName (Join-Path $stagingDir $f.Name)
            $sizeKB = [math]::Round($f.Length / 1024)
            Write-Host ("    {0} [{1} KB]" -f $f.Name, $sizeKB)
        }
        [PSCustomObject]@{
            source = $zipName
            files = @(Get-ChildItem -LiteralPath $stagingDir -File | Sort-Object Name | ForEach-Object {
                    [PSCustomObject]@{
                        name = $_.Name
                        size = $_.Length
                        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                    }
                })
        } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $stagingDir ".extraction-complete.json") -NoNewline
        Publish-ExtractionDirectory -StagingDirectory $stagingDir -DestinationDirectory $outputDir
        Write-Host ("  Extracted {0} files -> {1}" -f $found.Count, $outputDir) -ForegroundColor Green
    }

    } finally {
        if ($proc -and -not $proc.HasExited) {
            $proc.Kill()
            $proc.WaitForExit()
        }
        if ($proc) { $proc.Dispose() }
        Remove-Item $tempBase -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "Done" -ForegroundColor Cyan
if ($failures -gt 0) { exit 1 }
