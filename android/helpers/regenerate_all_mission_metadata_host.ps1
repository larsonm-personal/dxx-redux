#!/usr/bin/env pwsh

param(
    [switch]$NoRegressionCopy,
    [switch]$CdSourcesOnly
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $PSCommandPath
$androidRoot = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $androidRoot
. (Join-Path $scriptDir "standard_game_data.ps1")
. (Join-Path $scriptDir "cd_level_metadata_sources.ps1")
$zipDir = Join-Path $repoRoot "game_data\mission_files"
$cdSourceManifest = Join-Path $zipDir "cd_level_metadata_sources.json5"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outDir = Join-Path $androidRoot "temp\mission_zip_host_metadata\$stamp"
$metadataDir = Join-Path $outDir "metadata"
$rawDir = Join-Path $outDir "raw"
$logsDir = Join-Path $outDir "logs"
$stagesDir = Join-Path $outDir "stages"
$summaryJsonl = Join-Path $outDir "summary.jsonl"
$largeZipBytes = 524288000
$largeZipIncludePatterns = @("ewithin-versions.zip")
$invariantCulture = [System.Globalization.CultureInfo]::InvariantCulture
$d1DataCandidates = @(
    (Join-Path $repoRoot "game_data\extracted\d1 mac extracted"),
    (Join-Path $repoRoot "game_data_to_copy_to_emulator\data"),
    (Join-Path $repoRoot "game_data_to_copy_to_emulator\temp")
)
$d2DataCandidates = @(
    (Join-Path $repoRoot "game_data_to_copy_to_emulator\temp"),
    (Join-Path $repoRoot "game_data_to_copy_to_emulator\data"),
    (Join-Path $repoRoot "game_data\extracted\descent 2 demo 1-0_extracted")
)

function Write-Status {
    [Diagnostics.CodeAnalysis.SuppressMessageAttribute("PSAvoidUsingWriteHost", "", Justification = "Colored progress is intended for interactive batch runs")]
    param([string]$Message, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Message" -ForegroundColor $Color
}

function Get-7zaPath {
    $result = & "$androidRoot\get_deps\helpers\get_7zip.ps1"
    $candidate = @($result | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Last 1)
    if ($candidate) { return $candidate[0] }
    throw "Verified 7za.exe not found. Run android/get_deps/helpers/get_7zip.ps1"
}

function Get-HostExecutable {
    param([Parameter(Mandatory = $true)][ValidateSet("d1", "d2")][string]$Game)

    $buildDir = if ($Game -eq "d1") { "buildd1" } else { "buildd2" }
    $base = "dxx-redux-$Game-headless-metadata"
    foreach ($name in @("$base.exe", $base)) {
        $candidate = Join-Path $repoRoot "$buildDir\main\$name"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return ""
}

function Get-PowerShellPath {
    $current = (Get-Process -Id $PID).Path
    if ($current -and (Test-Path -LiteralPath $current -PathType Leaf)) {
        return $current
    }

    $name = if ($PSVersionTable.PSEdition -eq "Core") { "pwsh" } else { "powershell" }
    $command = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) {
        return $command.Source
    }

    throw "$name not found for host build"
}

function Initialize-HostExecutable {
    $d1 = Get-HostExecutable -Game d1
    $d2 = Get-HostExecutable -Game d2
    if ($d1 -and $d2) {
        return @{ d1 = $d1; d2 = $d2 }
    }

    Write-Status "Building host metadata executables"
    if ($IsWindows -or $env:OS -eq "Windows_NT") {
        $powershell = Get-PowerShellPath
        & $powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $repoRoot "run-windows-build.ps1") -Target both
    } else {
        $bash = Get-Command bash -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $bash) {
            throw "bash not found for host build"
        }
        & $bash.Source (Join-Path $repoRoot "run-linux-build.sh") --target both
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Host build failed with exit code $LASTEXITCODE"
    }

    $d1 = Get-HostExecutable -Game d1
    $d2 = Get-HostExecutable -Game d2
    if (-not $d1 -or -not $d2) {
        throw "Host metadata executables were not found after build"
    }
    return @{ d1 = $d1; d2 = $d2 }
}

function Get-SafeLabel {
    param([Parameter(Mandatory = $true)][string]$Name)

    $stem = [IO.Path]::GetFileNameWithoutExtension($Name).ToLowerInvariant()
    $safe = ([regex]::Replace($stem, '[^a-z0-9._-]+', '_')).Trim('_')
    if (-not $safe) { return "mission_zip" }
    return $safe
}

function Test-LargeMissionZipIncluded {
    param([Parameter(Mandatory = $true)][string]$Name)

    foreach ($pattern in $largeZipIncludePatterns) {
        if ($Name -like $pattern) {
            return $true
        }
    }
    return $false
}

function ConvertTo-NormalizedJsonText {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [switch]$MissionMetadata
    )

    $formatter = Join-Path $scriptDir "normalize_json.py"
    $python = Get-Command python -ErrorAction SilentlyContinue
    $usePyLauncher = $false
    if (-not $python) {
        $python = Get-Command py -ErrorAction SilentlyContinue
        $usePyLauncher = $true
    }
    if (-not $python) {
        throw "Python not found for JSON formatting"
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $python.Source
    if ($usePyLauncher) {
        [void]$startInfo.ArgumentList.Add("-3")
    }
    [void]$startInfo.ArgumentList.Add($formatter)
    if ($MissionMetadata) {
        [void]$startInfo.ArgumentList.Add("--mission-metadata")
    }
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $encoding = [System.Text.UTF8Encoding]::new($false)
    $startInfo.StandardInputEncoding = $encoding
    $startInfo.StandardOutputEncoding = $encoding
    $startInfo.StandardErrorEncoding = $encoding
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($startInfo)
    try {
        $process.StandardInput.Write($Text.Trim())
        $process.StandardInput.Close()
        $json = $process.StandardOutput.ReadToEnd()
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw "JSON formatter failed with exit code $($process.ExitCode): $errorText"
        }
        return (($json -replace "`r`n", "`n").TrimEnd([char[]]@("`r", "`n")) + "`n")
    } finally {
        $process.Dispose()
    }
}

. (Join-Path $PSScriptRoot 'atomic_text_file.ps1')

function Add-Utf8NoBomText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text
    )

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::AppendAllText($Path, $Text, [System.Text.UTF8Encoding]::new($false))
}

function Write-JsonValue {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Value,
        [switch]$MissionMetadata
    )

    $json = ConvertTo-Json -InputObject $Value -Depth 100
    Write-Utf8NoBomTextAtomically -Path $Path -Text (ConvertTo-NormalizedJsonText -Text $json -MissionMetadata:$MissionMetadata)
}

function Write-FailureJson {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Reason
    )

    $short = ($Reason -replace '[\r\n\t]+', ' ') -replace '\s{2,}', ' '
    $short = $short.Trim()
    if ($short.Length -gt 240) {
        $short = $short.Substring(0, 237).TrimEnd() + "..."
    }
    Write-JsonValue -Path $Path -Value ([ordered]@{ failure_text = $short })
}

function Get-Prop {
    param($Object, [string]$Name, $Default = $null)

    if ($null -eq $Object) { return $Default }
    $prop = $Object.PSObject.Properties[$Name]
    if ($prop) { return $prop.Value }
    return $Default
}

function Get-StringProp {
    param($Object, [string]$Name, [string]$Default = "")

    $value = Get-Prop $Object $Name $Default
    if ($null -eq $value) { return $Default }
    return [string]$value
}

function Get-ArrayValue {
    param($Value)

    if ($null -eq $Value) { return @() }
    if ($Value -is [System.Array]) { return @($Value) }
    return @($Value)
}

function Format-LevelMultiplier {
    param([double]$Value)

    if ($Value -le 0.0) { return "" }
    if ($Value -ge 10.0) {
        $scale = [Math]::Pow(10.0, [Math]::Floor([Math]::Log10($Value)) - 1.0)
        $display = [Math]::Floor($Value / $scale + 0.5) * $scale
        return ($display.ToString("0", $invariantCulture) + "x")
    }
    $format = if ($Value -ge 1.0) { "0.0" } else { "0.00" }
    return ($Value.ToString($format, $invariantCulture) + "x")
}

function Format-LevelTime {
    param([int]$Seconds)

    if ($Seconds -lt 0) { $Seconds = 0 }
    $minutes = [Math]::Floor($Seconds / 60)
    $remaining = $Seconds % 60
    return ("{0}M:{1:d2}S" -f $minutes, $remaining)
}

function Add-IfString {
    param(
        [Parameter(Mandatory = $true)][System.Collections.Specialized.OrderedDictionary]$Target,
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$Value
    )

    if ($Value) {
        $Target[$Name] = $Value
    }
}

function ConvertTo-CheckedInLevelJson {
    param($Level)

    $levelNum = [int](Get-Prop $Level "level_num" 0)
    $mineVolumeNormalized = [double](Get-Prop $Level "mine_volume_normalized" 0.0)
    $travelTimeSeconds = [int](Get-Prop $Level "travel_time_seconds" 0)
    $routeStatus = Get-StringProp $Level "route_status" "failed"
    $rowStatus = Get-StringProp $Level "status" "ok"
    $notes = @(
        Get-ArrayValue (Get-Prop $Level "notes" @())
        Get-StringProp $Level "route_note"
        Get-StringProp $Level "guidebot_placement_note"
        Get-StringProp $Level "guidebot_note"
    ) | Where-Object { $_ } | Select-Object -Unique

    $row = [ordered]@{
        level_num = $levelNum
        secret = [bool](Get-Prop $Level "secret" ($levelNum -lt 0))
        level_name = Get-StringProp $Level "level_name"
        level_file = Get-StringProp $Level "level_file"
        robots = [int](Get-Prop $Level "robot_count" 0)
        hostages = [int](Get-Prop $Level "hostage_count" 0)
        secrets = [int](Get-Prop $Level "secret_count" 0)
        matcens = [int](Get-Prop $Level "matcen_count" 0)
        energy_centers = [int](Get-Prop $Level "energy_center_count" 0)
        mine_volume = [double](Get-Prop $Level "mine_volume" 0.0)
        mine_volume_normalized = $mineVolumeNormalized
        mine_volume_text = Format-LevelMultiplier -Value $mineVolumeNormalized
        travel_distance = [double](Get-Prop $Level "travel_distance" 0.0)
        travel_time_seconds = $travelTimeSeconds
        travel_time_text = Format-LevelTime -Seconds $travelTimeSeconds
        guidebot_count = [int](Get-Prop $Level "guidebot_count" 0)
        guidebot_placed = [bool](Get-Prop $Level "guidebot_placed" $false)
        guidebot_accessible = [bool](Get-Prop $Level "guidebot_accessible" $false)
        route_status = $routeStatus
        route_steps = @(Get-ArrayValue (Get-Prop $Level "route_steps" @()))
    }
    Add-IfString -Target $row -Name "guidebot_placement_note" -Value (Get-StringProp $Level "guidebot_placement_note")
    Add-IfString -Target $row -Name "guidebot_note" -Value (Get-StringProp $Level "guidebot_note")
    Add-IfString -Target $row -Name "route_problem" -Value (Get-StringProp $Level "route_problem")
    Add-IfString -Target $row -Name "route_note" -Value (Get-StringProp $Level "route_note")
    $problems = @(Get-ArrayValue (Get-Prop $Level "problems" @())) | Where-Object { $_ }
    if ($problems.Count -gt 0) { $row["problems"] = $problems }
    if ($notes.Count -gt 0) { $row["notes"] = @($notes) }
    if ($rowStatus -ne "ok") { $row["status"] = $rowStatus }
    return $row
}

function ConvertTo-CheckedInMissionJson {
    param($Raw, [int]$TargetIndex, [string]$SourceName = "", [string]$MissionFilename = "")

    $levels = @(Get-ArrayValue (Get-Prop $Raw "levels" @()) | ForEach-Object { ConvertTo-CheckedInLevelJson -Level $_ })
    $missionName = Get-StringProp $Raw "mission_name"
    $source = if ($SourceName) { $SourceName } elseif ($missionName) { $missionName } else { Get-StringProp $Raw "mission_filename" }
    $filename = if ($MissionFilename) { $MissionFilename } else { Get-StringProp $Raw "mission_filename" }
    $status = Get-StringProp $Raw "status" "ok"
    $result = [ordered]@{
        status = $status
        source = $source
        game = Get-StringProp $Raw "game"
        mission_name = $missionName
        mission_filename = $filename
    }
    Add-IfString -Target $result -Name "coop_starts" -Value (Get-StringProp $Raw "coop_starts")
    $result["level_count"] = $levels.Count
    $result["levels"] = $levels
    $problems = @(Get-ArrayValue (Get-Prop $Raw "problems" @())) | Where-Object { $_ }
    if ($problems.Count -gt 0) { $result["problems"] = $problems }
    $result["target_index"] = $TargetIndex
    return $result
}

function Copy-StageAlias {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Target
    )

    if ((Resolve-Path -LiteralPath $Source).Path -eq [IO.Path]::GetFullPath($Target)) {
        return
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Target) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Target -Force
}

function Copy-RawMissionFileSet {
    param(
        [Parameter(Mandatory = $true)][string]$RawDirPath,
        [Parameter(Mandatory = $true)][string]$StageDir
    )

    $stageFull = [IO.Path]::GetFullPath($StageDir)
    $stagesFull = [IO.Path]::GetFullPath($stagesDir)
    if (-not $stageFull.StartsWith($stagesFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Stage directory is outside host metadata temp root: $StageDir"
    }
    if (Test-Path -LiteralPath $StageDir) {
        Remove-Item -LiteralPath $StageDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
    $used = @{}
    foreach ($file in Get-ChildItem -LiteralPath $RawDirPath -Recurse -File) {
        $leaf = $file.Name
        if (-not $leaf -or $leaf -eq "." -or $leaf -eq "..") {
            continue
        }
        $key = $leaf.ToLowerInvariant()
        if ($used.ContainsKey($key)) {
            continue
        }
        $used[$key] = $true
        $target = Join-Path $StageDir $leaf
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
        $ext = [IO.Path]::GetExtension($leaf).ToLowerInvariant()
        if ($ext -in @(".msn", ".mn2", ".hog", ".rdl", ".rl2", ".sdl", ".sl2")) {
            $lower = $leaf.ToLowerInvariant()
            Copy-StageAlias -Source $target -Target (Join-Path $StageDir $lower)
            Copy-StageAlias -Source $target -Target (Join-Path (Join-Path $StageDir "missions") $leaf)
            Copy-StageAlias -Source $target -Target (Join-Path (Join-Path $StageDir "missions") $lower)
        }
    }
}

function Copy-CdMissionFileSet {
    param(
        [Parameter(Mandatory = $true)]$Source,
        [Parameter(Mandatory = $true)][string]$StageDir
    )

    $stageFull = [IO.Path]::GetFullPath($StageDir)
    $stagesFull = [IO.Path]::GetFullPath($stagesDir)
    if (-not $stageFull.StartsWith($stagesFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Stage directory is outside host metadata temp root: $StageDir"
    }
    if (Test-Path -LiteralPath $StageDir) {
        Remove-Item -LiteralPath $StageDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

    $sourceFiles = if ($Source.Discover) {
        @(
            Get-ChildItem -LiteralPath $Source.SourceDir -Recurse -File |
                Where-Object { $_.Extension.ToLowerInvariant() -in @(".msn", ".mn2", ".hog", ".rdl", ".rl2") } |
                Sort-Object FullName
        )
    } else {
        @($Source.Files)
    }
    $used = @{}
    foreach ($file in $sourceFiles) {
        $key = $file.Name.ToLowerInvariant()
        if ($used.ContainsKey($key)) {
            continue
        }
        $used[$key] = $true
        $target = Join-Path $StageDir $file.Name
        if ($file.Extension.ToLowerInvariant() -in @(".msn", ".mn2")) {
            $descriptorText = [IO.File]::ReadAllText($file.FullName).Replace(([char]0x1a).ToString(), "")
            [IO.File]::WriteAllText($target, $descriptorText, [Text.UTF8Encoding]::new($false))
        } else {
            Copy-Item -LiteralPath $file.FullName -Destination $target -Force
        }
        $lower = $file.Name.ToLowerInvariant()
        $upper = $file.Name.ToUpperInvariant()
        Copy-StageAlias -Source $target -Target (Join-Path $StageDir $lower)
        Copy-StageAlias -Source $target -Target (Join-Path $StageDir $upper)
        Copy-StageAlias -Source $target -Target (Join-Path (Join-Path $StageDir "missions") $file.Name)
        Copy-StageAlias -Source $target -Target (Join-Path (Join-Path $StageDir "missions") $lower)
        Copy-StageAlias -Source $target -Target (Join-Path (Join-Path $StageDir "missions") $upper)
    }
}

function Get-HeadlessFailureSummary {
    param(
        [Parameter(Mandatory = $true)][string]$Mission,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return "headless metadata failed for ${Mission}; log was not written"
    }

    $lines = @(Get-Content -LiteralPath $LogPath)
    $failLines = @($lines | Where-Object { $_ -like "SECRET-AREA-DUMP FAIL*" } | Select-Object -First 3)
    if ($failLines.Count -gt 0) {
        return "headless metadata failed for ${Mission}: $($failLines -join '; '); log=$LogPath"
    }

    $missingLevels = @(
        $lines |
            Where-Object { $_ -like "SECRET-AREA-DUMP WARN level missing *" } |
            ForEach-Object { $_ -replace '^SECRET-AREA-DUMP WARN level missing ', '' } |
            Select-Object -First 8
    )
    if ($missingLevels.Count -gt 0) {
        $suffix = if ($missingLevels.Count -ge 8) { ", ..." } else { "" }
        return "headless metadata failed for ${Mission}: missing required level files $($missingLevels -join ', ')$suffix; check that the mission HOG and descriptor were staged together; log=$LogPath"
    }

    $tail = ($lines | Select-Object -Last 8) -join " "
    return "headless metadata failed for ${Mission}: $tail; log=$LogPath"
}

function Expand-MissionArchive {
    param(
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$Archive,
        [Parameter(Mandatory = $true)][string]$RawArchiveDir
    )

    if (Test-Path -LiteralPath $RawArchiveDir) {
        Remove-Item -LiteralPath $RawArchiveDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $RawArchiveDir | Out-Null
    if ($Archive.Extension.Equals(".7z", [StringComparison]::OrdinalIgnoreCase)) {
        $sevenZip = Get-7zaPath
        $output = & $sevenZip x -y "-o$RawArchiveDir" -- $Archive.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "7z extract failed for $($Archive.Name): $($output -join ' ')"
        }
        return
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($Archive.FullName, $RawArchiveDir)
}

function Get-MissionDescriptor {
    param([Parameter(Mandatory = $true)][string]$StageDir)

    return @(
        Get-ChildItem -LiteralPath $StageDir -File |
            Where-Object { $_.Extension.ToLowerInvariant() -in @(".msn", ".mn2") } |
            Sort-Object Name
    )
}

function Get-CleanMissionDescriptorValue {
    param([string]$Value)

    if ($null -eq $Value) { return "" }
    return $Value.Split(";")[0].Trim()
}

function Get-MissionDescriptorInfo {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$Descriptor)

    $values = @{}
    $descriptorText = [IO.File]::ReadAllText($Descriptor.FullName)
    foreach ($line in [regex]::Split($descriptorText, "`r`n|`n|`r")) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith(";") -or $trimmed.StartsWith("#")) {
            continue
        }
        $eq = $trimmed.IndexOf("=")
        if ($eq -lt 0) {
            continue
        }
        $key = $trimmed.Substring(0, $eq).Trim().ToLowerInvariant()
        $values[$key] = Get-CleanMissionDescriptorValue -Value $trimmed.Substring($eq + 1)
    }
    $displayName = ""
    foreach ($key in @("name", "xname", "zname", "!name")) {
        if ($values.ContainsKey($key) -and $values[$key]) {
            $displayName = $values[$key]
            break
        }
    }
    if (-not $displayName) {
        $displayName = $Descriptor.BaseName
    }
    return [pscustomobject]@{
        DisplayName = $displayName
        Filename = $Descriptor.Name
        Type = if ($values.ContainsKey("type")) { $values["type"].ToLowerInvariant() } else { "normal" }
    }
}

function Invoke-HeadlessMetadataProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [int]$TimeoutSeconds = 120
    )

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    foreach ($argument in $Arguments) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($startInfo)
    try {
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            try { $process.Kill($true) } catch { try { $process.Kill() } catch {} }
            $process.WaitForExit()
            $stdout = $stdoutTask.GetAwaiter().GetResult()
            $stderr = $stderrTask.GetAwaiter().GetResult()
            Write-Utf8NoBomTextAtomically -Path $LogPath -Text (($stdout + "`n" + $stderr).Trim() + "`n")
            throw "headless metadata timed out after $TimeoutSeconds seconds; log=$LogPath"
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        Write-Utf8NoBomTextAtomically -Path $LogPath -Text (($stdout + "`n" + $stderr).Trim() + "`n")
        return $process.ExitCode
    } finally {
        $process.Dispose()
    }
}

function Invoke-HeadlessScan {
    param(
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$Descriptor,
        [Parameter(Mandatory = $true)][string]$StageDir,
        [Parameter(Mandatory = $true)][hashtable]$Executables,
        [Parameter(Mandatory = $true)][hashtable]$DataDirs,
        [Parameter(Mandatory = $true)][string]$RawOutputPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [int]$TimeoutSeconds = 120
    )

    $game = if ($Descriptor.Extension.Equals(".msn", [StringComparison]::OrdinalIgnoreCase)) { "d1" } else { "d2" }
    $mission = [IO.Path]::GetFileNameWithoutExtension($Descriptor.Name)
    $exe = $Executables[$game]
    $dataDir = $DataDirs[$game]
    $exitCode = Invoke-HeadlessMetadataProcess -Executable $exe -Arguments @(
        "-hogdir", $dataDir,
        "-extra-dir", $StageDir,
        "-mission", $mission,
        "-secretarea-json-out", $RawOutputPath
    ) -LogPath $LogPath -TimeoutSeconds $TimeoutSeconds
    if ($exitCode -ne 0) {
        throw (Get-HeadlessFailureSummary -Mission $mission -LogPath $LogPath)
    }
    return Get-Content -LiteralPath $RawOutputPath -Raw | ConvertFrom-Json
}

function Invoke-BuiltinHeadlessScan {
    param(
        [Parameter(Mandatory = $true)][ValidateSet("d1", "d2")][string]$Game,
        [Parameter(Mandatory = $true)][hashtable]$Executables,
        [Parameter(Mandatory = $true)][hashtable]$DataDirs,
        [Parameter(Mandatory = $true)][string]$RawOutputPath,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $exitCode = Invoke-HeadlessMetadataProcess -Executable $Executables[$Game] -Arguments @(
        "-hogdir", $DataDirs[$Game],
        "-secretarea-json-out", $RawOutputPath
    ) -LogPath $LogPath
    if ($exitCode -ne 0) {
        throw (Get-HeadlessFailureSummary -Mission $Game -LogPath $LogPath)
    }
    return Get-Content -LiteralPath $RawOutputPath -Raw | ConvertFrom-Json
}

function Write-SummaryRecord {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Record)

    Add-Utf8NoBomText -Path $summaryJsonl -Text (($Record | ConvertTo-Json -Depth 30 -Compress) + "`n")
}

New-Item -ItemType Directory -Force -Path $metadataDir, $rawDir, $logsDir, $stagesDir | Out-Null
& (Join-Path $scriptDir "retain-recent-artifacts.ps1") -Artifacts $outDir
if (-not (Test-Path -LiteralPath $zipDir -PathType Container)) {
    throw "Mission metadata source directory not found: $zipDir"
}

$executables = Initialize-HostExecutable
$standardDeps = @(Get-StandardGameDataDeps)
$d1Dependencies = @($standardDeps | Where-Object { $_.file -in @("descent.hog", "descent.pig") })
$d2Dependencies = @($standardDeps | Where-Object { $_.file -in @("descent2.hog", "descent2.ham", "groupa.pig") })
$dataSelections = @{
    d1 = Resolve-StandardGameDataDirectory -Candidates $d1DataCandidates -Dependencies $d1Dependencies -Label "D1"
    d2 = Resolve-StandardGameDataDirectory -Candidates $d2DataCandidates -Dependencies $d2Dependencies -Label "D2"
}
$dataDirs = @{
    d1 = $dataSelections.d1.Path
    d2 = $dataSelections.d2.Path
}

Write-Status "Host mission metadata output: $outDir"
Write-Status "D1 data: $($dataDirs.d1)"
Write-Status "D1 hashes: $(($dataSelections.d1.Hashes.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ', ')"
Write-Status "D2 data: $($dataDirs.d2)"
Write-Status "D2 hashes: $(($dataSelections.d2.Hashes.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ', ')"

$results = @()
$batchStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$seenCdDescriptorHashes = @{}

$cdSources = @(Resolve-CdLevelMetadataSources -RepoRoot $repoRoot -ManifestPath $cdSourceManifest -OutputDir $zipDir)
foreach ($source in $cdSources) {
    $runStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $label = "cd_$($source.Id)"
    $metadataPath = Join-Path $metadataDir ([IO.Path]::GetFileName($source.OutputPath))
    $stageDir = Join-Path $stagesDir $label
    $record = [ordered]@{
        source = $source.SourceDir
        name = $source.Id
        status = "pending"
        metadata_json = $metadataPath
        regression_json = $source.OutputPath
    }
    Write-Status "Host CD metadata: $($source.Id)"
    try {
        Copy-CdMissionFileSet -Source $source -StageDir $stageDir
        $descriptors = if ($source.Discover) {
            @(Get-MissionDescriptor -StageDir $stageDir)
        } else {
            @(Get-Item -LiteralPath (Join-Path $stageDir $source.Descriptor.Name))
        }
        $missions = @()
        $descriptorFailures = @()
        foreach ($descriptor in $descriptors) {
            $descriptorHash = (Get-FileHash -LiteralPath $descriptor.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($seenCdDescriptorHashes.ContainsKey($descriptorHash)) {
                continue
            }
            $seenCdDescriptorHashes[$descriptorHash] = $true
            if ($descriptor.Name.ToLowerInvariant() -in $source.ExcludeDescriptors) {
                Write-Status "  EXCLUDED $($descriptor.Name)" "DarkGray"
                continue
            }
            $descriptorInfo = Get-MissionDescriptorInfo -Descriptor $descriptor
            if ($descriptorInfo.Type -eq "anarchy") {
                continue
            }
            $rawOutputPath = Join-Path $rawDir "$label.$($descriptor.BaseName).metadata.json"
            $logPath = Join-Path $logsDir "$label.$($descriptor.BaseName).log"
            try {
                $raw = Invoke-HeadlessScan -Descriptor $descriptor -StageDir $stageDir -Executables $executables -DataDirs $dataDirs -RawOutputPath $rawOutputPath -LogPath $logPath -TimeoutSeconds 30
            } catch {
                $reason = $_.Exception.Message
                $descriptorFailures += [pscustomobject]@{ Name = $descriptor.Name; Reason = $reason }
                Write-Status "  UNANALYZABLE $($descriptor.Name): $reason" "Yellow"
                continue
            }
            $missions += ConvertTo-CheckedInMissionJson -Raw $raw -TargetIndex $missions.Count -SourceName $descriptorInfo.DisplayName -MissionFilename $descriptorInfo.Filename
        }
        if ($missions.Count -eq 0) {
            throw "CD metadata source has no new non-anarchy mission descriptors"
        }
        Write-JsonValue -Path $metadataPath -Value ([object[]]$missions) -MissionMetadata
        if ($descriptorFailures.Count -gt 0) {
            $failureNames = @($descriptorFailures | ForEach-Object { $_.Name }) -join ", "
            throw "$($descriptorFailures.Count) CD mission descriptors were unanalyzable: $failureNames"
        }
        if (-not $NoRegressionCopy) {
            Write-Utf8NoBomTextAtomically -Path $source.OutputPath -Text ([System.IO.File]::ReadAllText($metadataPath))
        }
        $record["status"] = "passed"
        $record["mission_count"] = $missions.Count
        $record["level_count"] = @($missions | ForEach-Object { $_.level_count } | Measure-Object -Sum).Sum
    } catch {
        $record["status"] = "failed"
        $record["reason"] = $_.Exception.Message
        Write-FailureJson -Path $metadataPath -Reason $record["reason"]
    } finally {
        $runStopwatch.Stop()
        $record["elapsed_ms"] = $runStopwatch.ElapsedMilliseconds
        $results += [pscustomobject]$record
        Write-SummaryRecord -Record $record
        $color = if ($record["status"] -eq "passed") { "Green" } else { "Red" }
        Write-Status "$($record["status"].ToUpperInvariant()): $($source.Id) in $([Math]::Round($runStopwatch.Elapsed.TotalSeconds, 1))s" $color
        if ($record.Contains("reason") -and $record["reason"]) {
            Write-Status "  reason: $($record["reason"])" $color
        }
    }
}

if (-not $CdSourcesOnly) {
    $counterstrikeRawPath = Join-Path $rawDir "Counterstrike.metadata.json"
    $counterstrikeLogPath = Join-Path $logsDir "Counterstrike.log"
    $counterstrikeMetadataPath = Join-Path $metadataDir "Counterstrike.json"
    $counterstrikeRegressionPath = Join-Path $zipDir "Counterstrike.json"
    Write-Status "Host metadata: built-in Counterstrike"
    $counterstrikeRaw = Invoke-BuiltinHeadlessScan -Game d2 -Executables $executables -DataDirs $dataDirs -RawOutputPath $counterstrikeRawPath -LogPath $counterstrikeLogPath
    $counterstrike = ConvertTo-CheckedInMissionJson -Raw $counterstrikeRaw -TargetIndex 0 -SourceName "descent2.hog" -MissionFilename "d2"
    Write-JsonValue -Path $counterstrikeMetadataPath -Value $counterstrike -MissionMetadata
    if (-not $NoRegressionCopy) {
        Write-Utf8NoBomTextAtomically -Path $counterstrikeRegressionPath -Text ([System.IO.File]::ReadAllText($counterstrikeMetadataPath))
    }
    Write-Status "PASSED: built-in Counterstrike" "Green"

    $archives = @(
        Get-ChildItem -LiteralPath $zipDir -File |
            Where-Object { $_.Extension.ToLowerInvariant() -in @(".zip", ".7z") } |
            Sort-Object Name
    )
    if ($archives.Count -eq 0) {
        throw "No mission archives found in $zipDir"
    }

    $index = 0
    foreach ($archive in $archives) {
        $index++
        $runStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $label = Get-SafeLabel -Name $archive.Name
        $metadataPath = Join-Path $metadataDir "$($archive.BaseName).json"
        $regressionPath = Join-Path $zipDir "$($archive.BaseName).json"
        $rawArchiveDir = Join-Path $rawDir $label
        $stageDir = Join-Path $stagesDir $label
        $counts = @{
            passed = @($results | Where-Object { $_.status -eq "passed" }).Count
            skipped = @($results | Where-Object { $_.status -like "skipped*" }).Count
            failed = @($results | Where-Object { $_.status -eq "failed" }).Count
        }
        Write-Progress -Activity "Host mission metadata batch" -Status "Running ${index}/$($archives.Count): $($archive.Name)" -PercentComplete ([int](($index - 1) * 100 / $archives.Count))
        Write-Status ("[{0}/{1}] Host metadata: {2} ({3} MB, elapsed {4:n1}s, passed {5}, skipped {6}, failed {7})" -f
            $index, $archives.Count, $archive.Name, [Math]::Round($archive.Length / 1MB, 1),
            $batchStopwatch.Elapsed.TotalSeconds, $counts.passed, $counts.skipped, $counts.failed)

        $record = [ordered]@{
            zip = $archive.FullName
            name = $archive.Name
            size_bytes = $archive.Length
            status = "pending"
            metadata_json = $metadataPath
            regression_json = $regressionPath
        }
        try {
            if ($archive.Length -gt $largeZipBytes -and -not (Test-LargeMissionZipIncluded -Name $archive.Name)) {
                $record["status"] = "skipped_large"
                $record["reason"] = "archive is larger than the configured host metadata limit"
                Write-FailureJson -Path $metadataPath -Reason $record["reason"]
                continue
            }
            Expand-MissionArchive -Archive $archive -RawArchiveDir $rawArchiveDir
            Copy-RawMissionFileSet -RawDirPath $rawArchiveDir -StageDir $stageDir
            $descriptors = @(Get-MissionDescriptor -StageDir $stageDir)
            if ($descriptors.Count -eq 0) {
                $record["status"] = "skipped_no_descriptor"
                $record["reason"] = "archive contains no .msn or .mn2 mission descriptor"
                Write-FailureJson -Path $metadataPath -Reason $record["reason"]
                continue
            }
            $missions = @()
            $targetIndex = 0
            foreach ($descriptor in $descriptors) {
                $rawOutputPath = Join-Path $rawDir "$label.$($descriptor.BaseName).metadata.json"
                $logPath = Join-Path $logsDir "$label.$($descriptor.BaseName).log"
                $descriptorInfo = Get-MissionDescriptorInfo -Descriptor $descriptor
                $raw = Invoke-HeadlessScan -Descriptor $descriptor -StageDir $stageDir -Executables $executables -DataDirs $dataDirs -RawOutputPath $rawOutputPath -LogPath $logPath
                $missions += ConvertTo-CheckedInMissionJson -Raw $raw -TargetIndex $targetIndex -SourceName $descriptorInfo.DisplayName -MissionFilename $descriptorInfo.Filename
                $targetIndex++
            }
            Write-JsonValue -Path $metadataPath -Value ([object[]]$missions) -MissionMetadata
            if (-not $NoRegressionCopy) {
                Write-Utf8NoBomTextAtomically -Path $regressionPath -Text ([System.IO.File]::ReadAllText($metadataPath))
            }
            $record["status"] = "passed"
            $record["mission_count"] = $missions.Count
            $record["level_count"] = @($missions | ForEach-Object { $_.level_count } | Measure-Object -Sum).Sum
        } catch {
            $record["status"] = "failed"
            $record["reason"] = $_.Exception.Message
            Write-FailureJson -Path $metadataPath -Reason $record["reason"]
        } finally {
            $runStopwatch.Stop()
            $record["elapsed_ms"] = $runStopwatch.ElapsedMilliseconds
            $results += [pscustomobject]$record
            Write-SummaryRecord -Record $record
            $color = if ($record["status"] -eq "passed") { "Green" } elseif ($record["status"] -like "skipped*") { "Yellow" } else { "Red" }
            Write-Status ("[{0}/{1}] {2}: {3} in {4:n1}s" -f $index, $archives.Count, $record["status"].ToUpperInvariant(), $archive.Name, $runStopwatch.Elapsed.TotalSeconds) $color
            if ($record.Contains("reason") -and $record["reason"]) {
                Write-Status "  reason: $($record["reason"])" $color
            }
        }
    }
}

$batchStopwatch.Stop()
Write-Progress -Activity "Host mission metadata batch" -Completed
Write-JsonValue -Path (Join-Path $outDir "summary.json") -Value ([object[]]$results)
$failed = @($results | Where-Object { $_.status -eq "failed" })
$skipped = @($results | Where-Object { $_.status -like "skipped*" })
$passed = @($results | Where-Object { $_.status -eq "passed" })
$failedLines = @($failed | ForEach-Object { "$($_.name)`t$($_.reason)" })
Write-Utf8NoBomTextAtomically -Path (Join-Path $outDir "failed_zips.txt") -Text (($failedLines -join "`n") + $(if ($failedLines.Count) { "`n" } else { "" }))
Write-Status "Host mission metadata complete: $($results.Count) total, $($passed.Count) passed, $($skipped.Count) skipped, $($failed.Count) failed in $([Math]::Round($batchStopwatch.Elapsed.TotalSeconds, 1))s"
Write-Status "Output: $outDir"
if ($failed.Count -gt 0) {
    exit 1
}
exit 0
