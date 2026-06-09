#!/usr/bin/env pwsh
# Runs the reusable mission ZIP import/metadata/launch smoke test over a folder.

param(
    [string]$ZipDir = "C:\local\dxx-redux\game_data\mission_files",
    [string]$OutDir = "",
    [string]$Pattern = "*.zip",
    [string]$RegressionJsonDir = "",
    [switch]$Install,
    [switch]$IncludeLarge,
    [switch]$NoRegressionJson,
    [long]$LargeZipBytes = 524288000,
    [int]$TimeoutSeconds = 900
)

$ErrorActionPreference = "Stop"
$helpersDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $helpersDir
. (Join-Path $helpersDir "test_helpers.ps1")

if (-not $OutDir) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutDir = Join-Path $androidRoot "temp\mission_zip_batch\$stamp"
}

$metadataDir = Join-Path $OutDir "metadata"
$importsDir = Join-Path $OutDir "imports"
$artifactsDir = Join-Path $OutDir "artifacts"
$resolvedScriptDir = Join-Path $OutDir "resolved_scripts"
New-Item -ItemType Directory -Force -Path $metadataDir, $importsDir, $artifactsDir, $resolvedScriptDir | Out-Null
$script:LogFile = Join-Path $OutDir "batch.log"

function Get-SafeMissionZipLabel {
    param([Parameter(Mandatory = $true)][string]$Name)
    $stem = [IO.Path]::GetFileNameWithoutExtension($Name).ToLowerInvariant()
    $safe = ([regex]::Replace($stem, '[^a-z0-9._-]+', '_')).Trim('_')
    if (-not $safe) { $safe = "mission_zip" }
    return $safe
}

function ConvertTo-JsonStringContent {
    param([Parameter(Mandatory = $true)][string]$Value)
    $literal = $Value | ConvertTo-Json -Compress
    return $literal.Substring(1, $literal.Length - 2)
}

function Resolve-MissionZipTemplate {
    param(
        [Parameter(Mandatory = $true)][string]$DeviceZipName,
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )
    $templatePath = Join-Path $androidRoot "game_scripts\test_mission_zip_batch_import_metadata_launch.json5"
    $text = Get-Content -Path $templatePath -Raw
    $text = $text.Replace('${ZIP_FILE}', (ConvertTo-JsonStringContent $DeviceZipName))
    $text = $text.Replace('${ZIP_LABEL}', (ConvertTo-JsonStringContent $Label))
    $text = $text.Replace('${ZIP_DISPLAY_NAME}', (ConvertTo-JsonStringContent $DeviceZipName))
    Set-Content -Path $OutputPath -Value $text -Encoding utf8
}

function Push-AppPrivateFile {
    param(
        [Parameter(Mandatory = $true)][string]$LocalPath,
        [Parameter(Mandatory = $true)][string]$DeviceRelativePath
    )
    $deviceLeaf = Split-Path -Leaf $DeviceRelativePath
    $deviceDir = (Split-Path -Parent $DeviceRelativePath).Replace('\', '/')
    $tmp = "/data/local/tmp/$deviceLeaf"
    & $script:ADB push $LocalPath $tmp 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "adb push failed for $LocalPath"
    }
    $filesDir = if ($deviceDir) { "files/$deviceDir" } else { "files" }
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", $filesDir) | Out-Null
    $dest = "/data/data/$($script:PACKAGE)/files/$($DeviceRelativePath.Replace('\', '/'))"
    & $script:ADB shell "run-as $($script:PACKAGE) sh -c 'cat $tmp > $dest'" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "run-as copy failed for $DeviceRelativePath"
    }
    & $script:ADB shell "rm -f $tmp" 2>&1 | Out-Null
}

function Save-AppTextFile {
    param(
        [Parameter(Mandatory = $true)][string]$DeviceRelativePath,
        [Parameter(Mandatory = $true)][string]$LocalPath
    )
    $devicePath = "files/$($DeviceRelativePath.Replace('\', '/'))"
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", $devicePath) -Seconds 30
    if (-not $text -or $text -match 'No such file') {
        return $false
    }
    Set-Content -Path $LocalPath -Value $text -Encoding utf8
    return $true
}

function Read-AppAutomationResult {
    $text = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_result.json") -Seconds 5
    if (-not $text -or $text -match 'No such file') { return $null }
    try { return ($text | ConvertFrom-Json) } catch { return $null }
}

function Clear-RunArtifacts {
    param([Parameter(Mandatory = $true)][string]$Label)
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "rm", "-f",
        "files/automation_result.json",
        "files/automation_log.jsonl",
        "files/introspect.json",
        "files/level_metadata_automation_$Label.json",
        "files/mission_zip_import_$Label.json"
    ) | Out-Null
}

if (-not (Test-Path $ZipDir)) {
    Write-Status "FAIL: ZIP directory not found: $ZipDir" "Red"
    exit 1
}
if (-not $NoRegressionJson) {
    if (-not $RegressionJsonDir) {
        $RegressionJsonDir = $ZipDir
    }
    New-Item -ItemType Directory -Force -Path $RegressionJsonDir | Out-Null
}

Ensure-EmulatorHealthy
if ($Install) {
    Write-Status "Installing APK"
    if (-not (Install-ApkOnDevice)) {
        Write-Status "FAIL: APK install failed" "Red"
        exit 1
    }
}

Write-Status "Resolving standard D1/D2 base data"
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Write-Status "FAIL: could not provision base game data" "Red"
    exit 1
}

$zips = @(Get-ChildItem -Path $ZipDir -Filter $Pattern -File | Sort-Object Name)
if ($zips.Count -eq 0) {
    Write-Status "FAIL: no ZIPs matched $Pattern in $ZipDir" "Red"
    exit 1
}

$results = @()
foreach ($zip in $zips) {
    $label = Get-SafeMissionZipLabel $zip.Name
    $deviceZipName = "$label.zip"
    $metadataPath = Join-Path $metadataDir "$($zip.BaseName).json"
    $importPath = Join-Path $importsDir "$($zip.BaseName).import.json"
    $artifactPrefix = Join-Path $artifactsDir $label
    $resolvedScript = Join-Path $resolvedScriptDir "$label.json5"
    $deviceScriptName = "mission_zip_batch_current.json5"
    $regressionJsonPath =
    if ($NoRegressionJson) {
        ""
    } else {
        Join-Path $RegressionJsonDir "$($zip.BaseName).json"
    }
    $hash = (Get-FileHash -Algorithm SHA256 -Path $zip.FullName).Hash.ToLowerInvariant()

    $record = [ordered]@{
        zip = $zip.FullName
        name = $zip.Name
        sha256 = $hash
        size_bytes = $zip.Length
        label = $label
        status = "pending"
        metadata_json = $metadataPath
        regression_json = $regressionJsonPath
        import_json = $importPath
        automation_result_json = "$artifactPrefix.automation_result.json"
        automation_log_jsonl = "$artifactPrefix.automation_log.jsonl"
        introspect_json = "$artifactPrefix.introspect.json"
    }

    if ($zip.Length -gt $LargeZipBytes -and -not $IncludeLarge) {
        Write-Status "SKIP large ZIP: $($zip.Name) ($([math]::Round($zip.Length / 1MB, 1)) MB)" "Yellow"
        $record["status"] = "skipped_large"
        $results += [pscustomobject]$record
        Set-Content -Path $metadataPath -Value "[]" -Encoding utf8
        continue
    }

    Write-Status "Running mission ZIP: $($zip.Name)"
    try {
        Resolve-MissionZipTemplate -DeviceZipName $deviceZipName -Label $label -OutputPath $resolvedScript
        Push-AppPrivateFile -LocalPath $zip.FullName -DeviceRelativePath "mission_zip_batch_cache/$deviceZipName"
        Push-AppPrivateFile -LocalPath $resolvedScript -DeviceRelativePath $deviceScriptName

        Stop-AppAndWait
        Reset-GameState
        Clear-RunArtifacts -Label $label
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
        if (-not (Wait-SetupActivityReady)) {
            throw "SetupActivity did not become ready"
        }
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE",
            "--es", "script", $deviceScriptName
        ) | Out-Null

        $passed = Watch-AutomationResult -TimeoutSeconds $TimeoutSeconds -IsLauncherScript
        $automationResult = Read-AppAutomationResult
        $record["automation_result"] = $automationResult
        $record["status"] = if ($passed) { "passed" } else { "failed" }
        if (-not $passed -and $automationResult -and $automationResult.reason) {
            $record["reason"] = $automationResult.reason
        }
    } catch {
        $record["status"] = "failed"
        $record["reason"] = $_.Exception.Message
        Write-Status "FAIL: $($zip.Name): $($record["reason"])" "Red"
    } finally {
        $metadataSaved = Save-AppTextFile -DeviceRelativePath "level_metadata_automation_$label.json" -LocalPath $metadataPath
        if (-not $metadataSaved) {
            Set-Content -Path $metadataPath -Value "[]" -Encoding utf8
        }
        if ($metadataSaved -and -not $NoRegressionJson -and $record["status"] -eq "passed") {
            Copy-Item -Path $metadataPath -Destination $regressionJsonPath -Force
        }
        Save-AppTextFile -DeviceRelativePath "mission_zip_import_$label.json" -LocalPath $importPath | Out-Null
        Save-AppTextFile -DeviceRelativePath "automation_result.json" -LocalPath "$artifactPrefix.automation_result.json" | Out-Null
        Save-AppTextFile -DeviceRelativePath "automation_log.jsonl" -LocalPath "$artifactPrefix.automation_log.jsonl" | Out-Null
        Save-AppTextFile -DeviceRelativePath "introspect.json" -LocalPath "$artifactPrefix.introspect.json" | Out-Null
        $results += [pscustomobject]$record
        ($record | ConvertTo-Json -Depth 20 -Compress) | Add-Content -Path (Join-Path $OutDir "summary.jsonl") -Encoding utf8
    }
}

ConvertTo-Json -InputObject @($results) -Depth 30 | Set-Content -Path (Join-Path $OutDir "summary.json") -Encoding utf8
$failed = @($results | Where-Object { $_.status -eq "failed" })
Write-Status "Mission ZIP batch complete: $($results.Count) total, $($failed.Count) failed. Output: $OutDir"
if ($failed.Count -gt 0) { exit 1 }
exit 0
