# extract_dos_demos.ps1 — Extract game files from DOS shareware demo installers using DOSBox-X.
#
# For each zip in game_data\demo installers\, this script:
#   1. Extracts the zip to a temp directory
#   2. Runs the DOS INSTALL.EXE inside DOSBox-X with automated keystrokes
#   3. Copies the resulting game files (.HOG, .PIG, etc.) to an _extracted\ folder
#
# Requires: DOSBox-X installed via android\get_deps\get_dosbox.sh
# Usage:    .\extract_dos_demos.ps1 [-Force]
param(
    [switch]$Force  # Re-extract even if output already has game files
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot  = Split-Path $ScriptDir
$DemoDir   = Join-Path $ScriptDir "demo installers"

# --- Load DOSBox path from tool_versions.conf ---
$confFile = Join-Path (Join-Path (Join-Path $RepoRoot "android") "get_deps") "tool_versions.conf"
$dosboxDirName = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^DOSBOX_DIR_NAME=(.+)$') { $dosboxDirName = $Matches[1] }
}
if (-not $dosboxDirName) {
    Write-Error "DOSBOX_DIR_NAME not found in $confFile"
    exit 1
}

$DosboxExe = "C:\local\$dosboxDirName\dosbox-x.exe"
if (-not (Test-Path $DosboxExe)) {
    Write-Error "DOSBox-X not found at $DosboxExe`nRun:  bash android/get_deps/get_dosbox.sh"
    exit 1
}
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
        Zip       = "descent 1 demo 1-4.zip"
        Exe       = "INSTALL.EXE"
        # Stdin sequence: Enter (welcome), C (drive letter), Enter, then many Enters
        # for remaining prompts including post-install sound card config
        StdinKeys = @([char]13, [char]10, [char]67, [char]13, [char]10) +
                    (1..50 | ForEach-Object { [char]13; [char]10 })
        ExpectFiles = @("DESCENT.HOG", "DESCENT.PIG")
    },
    @{
        Zip       = "descent 2 demo 1-0.zip"
        Exe       = "INSTALL.EXE"
        # Stdin sequence: Enter (welcome), C (drive letter), Enter, then many Enters
        # for remaining prompts including post-install sound card detection/config
        StdinKeys = @([char]13, [char]10, [char]67, [char]13, [char]10) +
                    (1..50 | ForEach-Object { [char]13; [char]10 })
        ExpectFiles = @("D2DEMO.HOG", "D2DEMO.PIG", "D2DEMO.HAM")
    }
)

$TimeoutSec = 120
$PollIntervalMs = 2000

foreach ($inst in $Installers) {
    $zipName  = $inst.Zip
    $zipPath  = Join-Path $DemoDir $zipName
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($zipName)
    $outputDir = Join-Path $DemoDir "${baseName}_extracted"

    if (-not (Test-Path $zipPath)) {
        Write-Warning "Zip not found, skipping: $zipPath"
        continue
    }

    # Skip if already extracted (unless -Force)
    if ((Test-Path $outputDir) -and -not $Force) {
        $existing = Get-ChildItem $outputDir -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Extension -match '\.(hog|pig|ham|mvl)$' }
        if ($existing.Count -gt 0) {
            Write-Host ("$baseName already extracted, {0} game files. Use -Force to redo." -f $existing.Count)
            continue
        }
    }

    Write-Host "`n=== $baseName ===" -ForegroundColor Cyan

    # Create temp dirs (no spaces in path for DOSBox compatibility)
    $safeName = $baseName -replace '[^a-zA-Z0-9_-]', '_'
    $tempBase  = Join-Path $env:TEMP "dxx_dosbox_$safeName"
    $sourceDir = Join-Path $tempBase "src"    # extracted zip (DOSBox D:)
    $targetDir = Join-Path $tempBase "dst"    # DOSBox C: — installer writes here

    # Clean and create
    if (Test-Path $tempBase) { Remove-Item $tempBase -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null

    # Extract zip
    Write-Host "  Extracting $zipName..."
    Expand-Archive -Path $zipPath -DestinationPath $sourceDir -Force

    # Find INSTALL.EXE (might be in root or subdirectory)
    $installerExe = Get-ChildItem $sourceDir -Recurse -Filter $inst.Exe -File | Select-Object -First 1
    if (-not $installerExe) {
        Write-Warning "  $($inst.Exe) not found in $zipName. Skipping."
        Remove-Item $tempBase -Recurse -Force -ErrorAction SilentlyContinue
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
    $targetFwd = $targetDir -replace '\\','/'
    $installerFwd = $installerDir -replace '\\','/'

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
            Write-Host "  Game files detected and stable - killing DOSBox."
            $filesFound = $true
            break
        }
    }

    if (-not $proc.HasExited) {
        $proc.Kill()
        Start-Sleep -Seconds 1
        if (-not $filesFound) {
            Write-Warning "  DOSBox-X timed out after ${TimeoutSec}s without expected game files."
        }
    }

    # Search for game files in the target directory
    Write-Host "  Scanning for game files..."
    $found = @()
    foreach ($ext in $GameExtensions) {
        $found += Get-ChildItem $targetDir -Recurse -Filter $ext -File -ErrorAction SilentlyContinue
    }

    if ($found.Count -eq 0) {
        Write-Warning "  No game files found. The installer may need different keystrokes."
        Write-Host "  Contents of DOSBox C: drive:"
        Get-ChildItem $targetDir -Recurse -File | ForEach-Object {
            $rel = $_.FullName.Substring($targetDir.Length)
            Write-Host "    $rel"
        }
    } else {
        New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
        foreach ($f in $found) {
            Copy-Item $f.FullName (Join-Path $outputDir $f.Name) -Force
            $sizeKB = [math]::Round($f.Length / 1024)
            Write-Host ("    {0} [{1} KB]" -f $f.Name, $sizeKB)
        }
        Write-Host ("  Extracted {0} files -> {1}" -f $found.Count, $outputDir) -ForegroundColor Green
    }

    # Cleanup temp
    Remove-Item $tempBase -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "Done." -ForegroundColor Cyan
