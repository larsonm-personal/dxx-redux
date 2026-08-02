#!/usr/bin/env pwsh
# fingerprint_mission_zip_music.ps1 -- Extract mission ZIP soundtrack audio,
# fingerprint tracks, and optionally look up names on AcoustID.
#
# The output is game_data/music/Mission ZIP - <zip name>/chromaprint_info.json5
# so update_known_discs_albums.ps1 can merge and deduplicate the results.

param(
    [switch]$Force,
    [switch]$SkipBuild,
    [switch]$SkipAcoustId,
    [switch]$UpdateDatabase,
    [switch]$BudgetTestOnly,
    [string]$Zip,
    [string]$MissionDir = (Join-Path $PSScriptRoot "mission_files"),
    [string]$OutputRoot = (Join-Path $PSScriptRoot "music"),
    [string]$FingerprintExePath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
. "$repoRoot\android\helpers\test_env.ps1"
. "$repoRoot\android\helpers\bounded_extraction.ps1"
. "$repoRoot\android\helpers\fingerprint_audio_results.ps1"
. "$repoRoot\android\helpers\acoustid_title_match.ps1"
. "$repoRoot\android\helpers\json5.ps1"

Add-Type -AssemblyName System.IO.Compression.FileSystem

$buildDir = Join-Path $repoRoot "android/tests/build"
$fpExe = if ($FingerprintExePath) { $FingerprintExePath } else { Join-Path $buildDir "Release/fingerprint_audio.exe" }
$MaxArchiveEntries = 4096
$MaxArchiveEntryBytes = 512MB
$MaxArchiveTotalBytes = 2GB
$MaxArchiveRatio = 1000L
$MaxArchiveSeconds = 300
$MaxZipPreambleBytes = 16MB

function Register-ArchiveEntry {
    param(
        [long]$Length,
        [long]$CompressedLength,
        [string]$Name,
        [ref]$ContainerDeclaredBytes
    )
    $script:ArchiveBudget.Entries++
    if ($script:ArchiveBudget.Entries -gt $MaxArchiveEntries) { throw "Archive exceeds $MaxArchiveEntries entries" }
    if ($Length -gt $MaxArchiveEntryBytes) { throw "$Name exceeds $MaxArchiveEntryBytes bytes" }
    if ($CompressedLength -gt 0) {
        $quotient = [math]::Floor($Length / $CompressedLength)
        if ($quotient -gt $MaxArchiveRatio -or ($quotient -eq $MaxArchiveRatio -and ($Length % $CompressedLength) -gt 0)) {
            throw "$Name exceeds the ${MaxArchiveRatio}:1 expansion ratio"
        }
    }
    $declaredBytes = [long]$ContainerDeclaredBytes.Value
    if ($Length -gt $MaxArchiveTotalBytes - $declaredBytes) {
        throw "Archive container exceeds $MaxArchiveTotalBytes declared bytes"
    }
    $ContainerDeclaredBytes.Value = $declaredBytes + $Length
}

function Copy-BoundedStream {
    param(
        [System.IO.Stream]$InputStream,
        [System.IO.Stream]$OutputStream,
        [long]$CompressedLength,
        [string]$Name,
        [long]$ExactLength = -1
    )
    $buffer = New-Object byte[] 65536
    $entryBytes = 0L
    while ($true) {
        $want = if ($ExactLength -ge 0) { [math]::Min($buffer.Length, $ExactLength - $entryBytes) } else { $buffer.Length }
        if ($want -le 0) { break }
        $read = $InputStream.Read($buffer, 0, [int]$want)
        if ($read -le 0) { break }
        $entryBytes += $read
        $script:ArchiveBudget.ActualBytes += $read
        if ($entryBytes -gt $MaxArchiveEntryBytes -or $script:ArchiveBudget.ActualBytes -gt $MaxArchiveTotalBytes) {
            throw 'Archive output budget exceeded'
        }
        if ($CompressedLength -gt 0 -and $entryBytes -gt $CompressedLength * $MaxArchiveRatio) {
            throw "$Name exceeds the ${MaxArchiveRatio}:1 expansion ratio"
        }
        if (([DateTime]::UtcNow - $script:ArchiveBudget.Started).TotalSeconds -gt $MaxArchiveSeconds) {
            throw "Archive work exceeds $MaxArchiveSeconds seconds"
        }
        if ($OutputStream -is [IO.FileStream] -and ($script:ArchiveBudget.ActualBytes % 8MB) -lt $read) {
            $drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($OutputStream.Name))
            if ($drive.AvailableFreeSpace -lt 50MB) { throw 'Archive extraction exhausted free-space headroom' }
        }
        $OutputStream.Write($buffer, 0, $read)
    }
    if ($ExactLength -ge 0 -and $entryBytes -ne $ExactLength) { throw "Unexpected length for $Name" }
}

function Remove-BudgetedTemporaryFile {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $length = (Get-Item -LiteralPath $Path).Length
    Remove-Item -LiteralPath $Path -Force
    $script:ArchiveBudget.ActualBytes = [math]::Max(
        0L,
        [long]$script:ArchiveBudget.ActualBytes - $length
    )
}

function Get-7zaPath {
    $result = & "$repoRoot/android/get_deps/helpers/get_7zip.ps1"
    $candidate = @($result | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Last 1)
    if ($candidate) { return $candidate[0] }
    Write-Error "Verified 7za.exe not found. Run android/get_deps/helpers/get_7zip.ps1"
}

function Get-TarPath {
    $onPath = Get-Command tar -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    Write-Error "tar not found. Install bsdtar/libarchive or use a host with RAR extraction support"
}

function Escape-JsonString {
    param([AllowNull()][string]$Text)
    if ($null -eq $Text) { return "" }
    $json = ConvertTo-Json -Compress -InputObject ([string]$Text)
    return $json.Substring(1, $json.Length - 2)
}

function Get-SafeFilePart {
    param([string]$Text)
    $safe = $Text -replace '[\\/:*?"<>|!]+', "_" -replace '\s+', "_"
    $safe = $safe -replace '_+', "_"
    $safe = $safe.Trim("_")
    if (-not $safe) { return "track" }
    if ($safe.Length -gt 140) { return $safe.Substring($safe.Length - 140) }
    return $safe
}

function Test-AudioName {
    param([string]$Name)
    $ext = [System.IO.Path]::GetExtension($Name).ToLowerInvariant()
    return $ext -eq ".ogg" -or $ext -eq ".mp3" -or $ext -eq ".flac"
}

function Get-Sha1File {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha1 = [System.Security.Cryptography.SHA1]::Create()
        try {
            $hash = $sha1.ComputeHash($stream)
            return ([System.BitConverter]::ToString($hash) -replace "-", "").ToLowerInvariant()
        } finally {
            $sha1.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Get-HogEntryName {
    param([byte[]]$NameBytes)
    $length = 0
    while ($length -lt $NameBytes.Length -and $NameBytes[$length] -ne 0) {
        $length++
    }
    return [System.Text.Encoding]::ASCII.GetString($NameBytes, 0, $length).Trim(" ")
}

function Read-ExactBytes {
    param([System.IO.Stream]$Stream, [int]$Count)
    $bytes = New-Object byte[] $Count
    $offset = 0
    while ($offset -lt $Count) {
        $read = $Stream.Read($bytes, $offset, $Count - $offset)
        if ($read -le 0) { return $null }
        $offset += $read
    }
    return $bytes
}

function Copy-LimitedBytes {
    param(
        [System.IO.Stream]$InputStream,
        [System.IO.Stream]$OutputStream,
        [Int64]$Length
    )
    Copy-BoundedStream -InputStream $InputStream -OutputStream $OutputStream -CompressedLength $Length `
        -Name 'HOG member' -ExactLength $Length
}

function New-UniqueAudioPath {
    param(
        [string]$OutputDir,
        [string]$SourcePath,
        [hashtable]$SourceMap
    )
    $leaf = ($SourcePath -split '[\\\/!]')[-1]
    $ext = [System.IO.Path]::GetExtension($leaf).ToLowerInvariant()
    $base = Get-SafeFilePart ([System.IO.Path]::GetFileNameWithoutExtension($SourcePath))
    $candidate = "$base$ext"
    $i = 2
    while (Test-Path (Join-Path $OutputDir $candidate)) {
        $candidate = "${base}_${i}${ext}"
        $i++
    }
    $SourceMap[$candidate] = [ordered]@{
        source_path   = $SourcePath
        original_name = $leaf
    }
    return Join-Path $OutputDir $candidate
}

function Copy-EntryToTempFile {
    param(
        [System.IO.Compression.ZipArchiveEntry]$Entry,
        [string]$TempRoot,
        [string]$Extension
    )
    $tempPath = Join-Path $TempRoot "$([Guid]::NewGuid().ToString())$Extension"
    $inStream = $Entry.Open()
    try {
        $outStream = [System.IO.File]::Create($tempPath)
        try {
            Copy-BoundedStream -InputStream $inStream -OutputStream $outStream `
                -CompressedLength $Entry.CompressedLength -Name $Entry.FullName
        } finally { $outStream.Dispose() }
    } finally {
        $inStream.Dispose()
    }
    return $tempPath
}

function Get-ZipPayloadPath {
    param([string]$ArchivePath, [string]$TempRoot)
    $input = [IO.File]::OpenRead($ArchivePath)
    $signature = [byte[]](0x50, 0x4b, 0x03, 0x04)
    $matched = 0
    $offset = -1L
    try {
        while ($input.Position -lt $MaxZipPreambleBytes) {
            $next = $input.ReadByte()
            if ($next -lt 0) { break }
            if ($next -eq $signature[$matched]) {
                $matched++
                if ($matched -eq $signature.Length) { $offset = $input.Position - $signature.Length; break }
            } else {
                $matched = if ($next -eq $signature[0]) { 1 } else { 0 }
            }
        }
    } finally { $input.Dispose() }
    if ($offset -lt 0) { throw "Could not find a ZIP payload within $MaxZipPreambleBytes bytes in $ArchivePath" }
    if ($offset -eq 0) { return $ArchivePath }
    $zipPath = Join-Path $TempRoot "$([Guid]::NewGuid().ToString()).zip"
    $input = [IO.File]::OpenRead($ArchivePath)
    [void]$input.Seek($offset, [IO.SeekOrigin]::Begin)
    $outStream = [System.IO.File]::Create($zipPath)
    try {
        Copy-BoundedStream -InputStream $input -OutputStream $outStream -CompressedLength ($input.Length - $offset) `
            -Name ([IO.Path]::GetFileName($ArchivePath))
    } finally {
        $outStream.Dispose()
        $input.Dispose()
    }
    return $zipPath
}

function Extract-HogAudio {
    param(
        [string]$HogPath,
        [string]$OutputDir,
        [string]$SourcePrefix,
        [hashtable]$SourceMap
    )
    $count = 0
    $stream = [System.IO.File]::OpenRead($HogPath)
    $containerDeclaredBytes = 0L
    try {
        $magic = Read-ExactBytes -Stream $stream -Count 3
        if ($null -eq $magic -or [System.Text.Encoding]::ASCII.GetString($magic) -ne "DHF") {
            return 0
        }
        while ($stream.Position -lt $stream.Length) {
            $nameBytes = Read-ExactBytes -Stream $stream -Count 13
            if ($null -eq $nameBytes) {
                throw "Truncated HOG member name in $HogPath"
            }
            $lenBytes = Read-ExactBytes -Stream $stream -Count 4
            if ($null -eq $lenBytes) {
                throw "Truncated HOG member length in $HogPath"
            }
            $entryName = Get-HogEntryName $nameBytes
            $length = [Int64][BitConverter]::ToUInt32($lenBytes, 0)
            Register-ArchiveEntry -Length $length -CompressedLength $length -Name $entryName `
                -ContainerDeclaredBytes ([ref]$containerDeclaredBytes)
            if ($length -lt 0 -or $length -gt ($stream.Length - $stream.Position)) {
                throw "Invalid HOG member length $length for $entryName in $HogPath"
            }
            if (Test-AudioName $entryName) {
                $sourcePath = "$SourcePrefix!$entryName"
                $dest = New-UniqueAudioPath -OutputDir $OutputDir -SourcePath $sourcePath -SourceMap $SourceMap
                $outStream = [System.IO.File]::Create($dest)
                try { Copy-LimitedBytes -InputStream $stream -OutputStream $outStream -Length $length } finally { $outStream.Dispose() }
                $count++
            } elseif ($stream.CanSeek) {
                [void]$stream.Seek($length, [System.IO.SeekOrigin]::Current)
            } else {
                Copy-LimitedBytes -InputStream $stream -OutputStream ([System.IO.Stream]::Null) -Length $length
            }
        }
    } finally {
        $stream.Dispose()
    }
    return $count
}

function Extract-ZipAudio {
    param(
        [string]$ArchivePath,
        [string]$OutputDir,
        [string]$SourcePrefix,
        [hashtable]$SourceMap,
        [string]$TempRoot
    )
    $count = 0
    $zip = $null
    try {
        try {
            $zip = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
        } catch {
            $payloadPath = Get-ZipPayloadPath -ArchivePath $ArchivePath -TempRoot $TempRoot
            $zip = [System.IO.Compression.ZipFile]::OpenRead($payloadPath)
        }
        $containerDeclaredBytes = 0L
        foreach ($entry in $zip.Entries) {
            Register-ArchiveEntry -Length $entry.Length -CompressedLength $entry.CompressedLength `
                -Name $entry.FullName -ContainerDeclaredBytes ([ref]$containerDeclaredBytes)
        }
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith("/")) { continue }
            $entryName = $entry.FullName -replace '\\', '/'
            $sourcePath = if ($SourcePrefix) { "$SourcePrefix!$entryName" } else { $entryName }
            $ext = [System.IO.Path]::GetExtension($entryName).ToLowerInvariant()
            if (Test-AudioName $entryName) {
                $dest = New-UniqueAudioPath -OutputDir $OutputDir -SourcePath $sourcePath -SourceMap $SourceMap
                $inStream = $entry.Open()
                try {
                    $outStream = [System.IO.File]::Create($dest)
                    try {
                        Copy-BoundedStream -InputStream $inStream -OutputStream $outStream `
                            -CompressedLength $entry.CompressedLength -Name $entry.FullName
                    } finally { $outStream.Dispose() }
                } finally {
                    $inStream.Dispose()
                }
                $count++
            } elseif ($ext -eq ".hog") {
                $hogPath = Copy-EntryToTempFile -Entry $entry -TempRoot $TempRoot -Extension ".hog"
                try {
                    $count += Extract-HogAudio -HogPath $hogPath -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap
                } finally {
                    Remove-BudgetedTemporaryFile -Path $hogPath
                }
            } elseif ($ext -eq ".dxa" -or $ext -eq ".zip") {
                $nestedPath = Copy-EntryToTempFile -Entry $entry -TempRoot $TempRoot -Extension $ext
                try {
                    $count += Extract-ZipAudio -ArchivePath $nestedPath -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
                } finally {
                    Remove-BudgetedTemporaryFile -Path $nestedPath
                }
            } elseif ($ext -eq ".rar") {
                $nestedPath = Copy-EntryToTempFile -Entry $entry -TempRoot $TempRoot -Extension $ext
                try {
                    $count += Extract-RarAudio -ArchivePath $nestedPath -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
                } finally {
                    Remove-BudgetedTemporaryFile -Path $nestedPath
                }
            }
        }
    } finally {
        if ($zip) { $zip.Dispose() }
    }
    return $count
}

function Extract-DirectoryAudio {
    param(
        [string]$DirectoryPath,
        [string]$OutputDir,
        [string]$SourcePrefix,
        [hashtable]$SourceMap,
        [string]$TempRoot
    )
    $count = 0
    $files = Get-ChildItem -LiteralPath $DirectoryPath -File -Recurse
    $containerDeclaredBytes = 0L
    foreach ($file in $files) {
        Register-ArchiveEntry -Length $file.Length -CompressedLength 0 -Name $file.FullName `
            -ContainerDeclaredBytes ([ref]$containerDeclaredBytes)
        $relative = [System.IO.Path]::GetRelativePath($DirectoryPath, $file.FullName).Replace('\', '/')
        $sourcePath = if ($SourcePrefix) { "$SourcePrefix!$relative" } else { $relative }
        $ext = $file.Extension.ToLowerInvariant()
        if (Test-AudioName $file.Name) {
            $dest = New-UniqueAudioPath -OutputDir $OutputDir -SourcePath $sourcePath -SourceMap $SourceMap
            $input = [IO.File]::OpenRead($file.FullName)
            $output = [IO.File]::Create($dest)
            try { Copy-BoundedStream -InputStream $input -OutputStream $output -CompressedLength 0 -Name $file.Name } finally {
                $output.Dispose()
                $input.Dispose()
            }
            $count++
        } elseif ($ext -eq ".hog") {
            $count += Extract-HogAudio -HogPath $file.FullName -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap
        } elseif ($ext -eq ".dxa" -or $ext -eq ".zip") {
            $count += Extract-ZipAudio -ArchivePath $file.FullName -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
        } elseif ($ext -eq ".7z") {
            $count += Extract-SevenZipAudio -ArchivePath $file.FullName -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
        } elseif ($ext -eq ".rar") {
            $count += Extract-RarAudio -ArchivePath $file.FullName -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
        }
    }
    return $count
}

function Extract-SevenZipAudio {
    param(
        [string]$ArchivePath,
        [string]$OutputDir,
        [string]$SourcePrefix,
        [hashtable]$SourceMap,
        [string]$TempRoot
    )
    $extractDir = Join-Path $TempRoot "$([Guid]::NewGuid().ToString())_7z"
    New-Item -ItemType Directory -Path $extractDir -Force | Out-Null
    try {
        $sevenZip = Get-7zaPath
        $bounded = Invoke-BoundedExtractor -OutputDirectory $extractDir -FilePath $sevenZip `
            -ArgumentList @('x', "-o$extractDir", '-y', '--', $ArchivePath)
        if ($bounded.ExitCode -ne 0) {
            throw "7z extract failed for ${ArchivePath}: $($bounded.Output -join ' ')"
        }
        return Extract-DirectoryAudio -DirectoryPath $extractDir -OutputDir $OutputDir -SourcePrefix $SourcePrefix -SourceMap $SourceMap -TempRoot $TempRoot
    } finally {
        Remove-Item $extractDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Extract-RarAudio {
    param(
        [string]$ArchivePath,
        [string]$OutputDir,
        [string]$SourcePrefix,
        [hashtable]$SourceMap,
        [string]$TempRoot
    )
    $extractDir = Join-Path $TempRoot "$([Guid]::NewGuid().ToString())_rar"
    New-Item -ItemType Directory -Path $extractDir -Force | Out-Null
    try {
        $tar = Get-TarPath
        $bounded = Invoke-BoundedExtractor -OutputDirectory $extractDir -FilePath $tar `
            -ArgumentList @('-xf', $ArchivePath, '-C', $extractDir)
        if ($bounded.ExitCode -ne 0) {
            throw "RAR extract failed for ${ArchivePath}: $($bounded.Output -join ' ')"
        }
        return Extract-DirectoryAudio -DirectoryPath $extractDir -OutputDir $OutputDir -SourcePrefix $SourcePrefix -SourceMap $SourceMap -TempRoot $TempRoot
    } finally {
        Remove-Item $extractDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Extract-MissionArchiveAudio {
    param(
        [string]$ArchivePath,
        [string]$OutputDir,
        [string]$SourcePrefix,
        [hashtable]$SourceMap,
        [string]$TempRoot
    )
    if ([IO.Path]::GetExtension($ArchivePath).Equals(".7z", [StringComparison]::OrdinalIgnoreCase)) {
        return Extract-SevenZipAudio -ArchivePath $ArchivePath -OutputDir $OutputDir -SourcePrefix $SourcePrefix -SourceMap $SourceMap -TempRoot $TempRoot
    }
    if ([IO.Path]::GetExtension($ArchivePath).Equals(".rar", [StringComparison]::OrdinalIgnoreCase)) {
        return Extract-RarAudio -ArchivePath $ArchivePath -OutputDir $OutputDir -SourcePrefix $SourcePrefix -SourceMap $SourceMap -TempRoot $TempRoot
    }
    return Extract-ZipAudio -ArchivePath $ArchivePath -OutputDir $OutputDir -SourcePrefix $SourcePrefix -SourceMap $SourceMap -TempRoot $TempRoot
}

function Get-OptionalPropertyValue {
    param(
        [AllowNull()][object]$InputObject,
        [string]$Name
    )
    if ($null -eq $InputObject) { return $null }
    $property = $InputObject.PSObject.Properties[$Name]
    if ($property) { return $property.Value }
    return $null
}

function Get-MissionTracklistPath {
    param([System.IO.FileInfo]$ZipFile)
    $candidates = @(
        (Join-Path $ZipFile.DirectoryName "$($ZipFile.BaseName).tracklist.json"),
        (Join-Path $ZipFile.DirectoryName "$($ZipFile.Name).tracklist.json")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

function Get-TrackSlot {
    param($Track)
    $originalName = Get-OptionalPropertyValue $Track "original_name"
    $name = if ($originalName) { [string]$originalName } else {
        [string](Get-OptionalPropertyValue $Track "filename")
    }
    $base = [IO.Path]::GetFileNameWithoutExtension($name).ToLowerInvariant()
    if ($base -eq "briefing" -or $base -eq "credits") { return $base }
    if ($base -match '^game0*([1-9][0-9]*)$') { return "level$($Matches[1])" }
    return ""
}

function Add-TracklistKey {
    param([hashtable]$Lookup, [string]$Prefix, [AllowNull()][string]$Value, [string]$Name)
    if (-not $Value) { return }
    $key = "$Prefix`:$($Value.ToLowerInvariant())"
    if (-not $Lookup.ContainsKey($key)) {
        $Lookup[$key] = $Name
    }
}

function Read-MissionTracklist {
    param([System.IO.FileInfo]$ZipFile)
    $path = Get-MissionTracklistPath -ZipFile $ZipFile
    if (-not $path) { return @{} }
    $doc = Read-StrictJsonFile $path
    if ([string]$doc.schema -ne "dxx-mission-tracklist-v1") {
        throw "Unsupported tracklist schema in ${path}: $($doc.schema)"
    }
    $lookup = @{}
    foreach ($entry in @($doc.tracks)) {
        $title = Get-OptionalPropertyValue $entry "title"
        $name = if ($title) { [string]$title } else {
            [string](Get-OptionalPropertyValue $entry "name")
        }
        if (-not $name) { continue }
        Add-TracklistKey -Lookup $lookup -Prefix "filename" `
            -Value (Get-OptionalPropertyValue $entry "filename") -Name $name
        Add-TracklistKey -Lookup $lookup -Prefix "source_path" `
            -Value (Get-OptionalPropertyValue $entry "source_path") -Name $name
        Add-TracklistKey -Lookup $lookup -Prefix "original_name" `
            -Value (Get-OptionalPropertyValue $entry "original_name") -Name $name
        Add-TracklistKey -Lookup $lookup -Prefix "slot" `
            -Value (Get-OptionalPropertyValue $entry "slot") -Name $name
        $level = Get-OptionalPropertyValue $entry "level"
        if ($level) {
            Add-TracklistKey -Lookup $lookup -Prefix "slot" -Value "level$level" -Name $name
        }
    }
    Write-Host "  Using tracklist sidecar $path"
    return $lookup
}

function Get-TracklistName {
    param($Track, [hashtable]$TracklistLookup)
    if (-not $TracklistLookup -or $TracklistLookup.Count -eq 0) { return "" }
    foreach ($candidate in @(
            "filename:$([string](Get-OptionalPropertyValue $Track 'filename'))",
            "source_path:$([string](Get-OptionalPropertyValue $Track 'source_path'))",
            "original_name:$([string](Get-OptionalPropertyValue $Track 'original_name'))",
            "slot:$(Get-TrackSlot $Track)"
        )) {
        $key = $candidate.ToLowerInvariant()
        if ($TracklistLookup.ContainsKey($key)) { return [string]$TracklistLookup[$key] }
    }
    return ""
}

function Invoke-AcoustIdLookup {
    param([string]$Fingerprint, [int]$DurationSec)

    $elapsed = ([datetime]::UtcNow - $script:lastRequestTime).TotalMilliseconds
    if ($elapsed -lt $script:minDelayMs) {
        Start-Sleep -Milliseconds ([int]($script:minDelayMs - $elapsed))
    }

    $maxRetries = 3
    $backoffMs = 1000
    for ($attempt = 0; $attempt -le $maxRetries; $attempt++) {
        $script:lastRequestTime = [datetime]::UtcNow
        try {
            $wc = New-Object System.Net.WebClient
            $nvc = New-Object System.Collections.Specialized.NameValueCollection
            $nvc.Add("client", $script:acoustIdKey)
            $nvc.Add("meta", "recordings releases")
            $nvc.Add("duration", [string]$DurationSec)
            $nvc.Add("fingerprint", $Fingerprint)
            $responseBytes = $wc.UploadValues("https://api.acoustid.org/v2/lookup", "POST", $nvc)
            $responseStr = [System.Text.Encoding]::UTF8.GetString($responseBytes)
            $json = $responseStr | ConvertFrom-Json
            if ($json.status -eq "ok" -and $json.results) {
                foreach ($result in $json.results) {
                    if (-not $result.recordings) { continue }
                    foreach ($rec in $result.recordings) {
                        if (-not $rec.title) { continue }
                        $artists = ""
                        if ($rec.artists) {
                            $artists = ($rec.artists | ForEach-Object { $_.name }) -join ", "
                        }
                        $trackName = if ($artists) { "$artists - $($rec.title)" } else { $rec.title }
                        $albumTitle = $null
                        if ($rec.releases) { $albumTitle = $rec.releases[0].title }
                        return @{ name = $trackName; album = $albumTitle }
                    }
                }
            }
            if ($json.status -eq "error" -and $json.error -and
                $json.error.message -match "rate|limit|too fast") {
                Write-Warning "  AcoustID rate limited, backing off ${backoffMs}ms"
                Start-Sleep -Milliseconds $backoffMs
                $backoffMs *= 2
                continue
            }
            return $null
        } catch {
            $msg = $_.Exception.Message
            if ($msg -match "429|50[0-9]") {
                Write-Warning "  AcoustID HTTP error, backing off ${backoffMs}ms: $msg"
                Start-Sleep -Milliseconds $backoffMs
                $backoffMs *= 2
                continue
            }
            Write-Warning "  AcoustID lookup failed: $msg"
            return $null
        }
    }
    Write-Warning "  AcoustID lookup exhausted retries"
    return $null
}

function Add-AcoustIdResults {
    param(
        [array]$Tracks,
        [hashtable]$ExistingTracks,
        [hashtable]$TracklistLookup = @{},
        [switch]$RefreshAcoustId
    )
    $resultTracks = @()
    foreach ($track in $Tracks) {
        $fname = [string](Get-OptionalPropertyValue $track "filename")
        $out = [ordered]@{
            filename    = $fname
            chromaprint = [string]$track.chromaprint
            duration_ms = [int]$track.duration_ms
        }
        $sourcePath = Get-OptionalPropertyValue $track "source_path"
        $originalName = Get-OptionalPropertyValue $track "original_name"
        if ($sourcePath) { $out["source_path"] = [string]$sourcePath }
        if ($originalName) { $out["original_name"] = [string]$originalName }

        $existing = $ExistingTracks[$fname]
        $cachedMetadata = Get-DxxReusableAcoustIdMetadata -Existing $existing `
            -Chromaprint ([string]$track.chromaprint)
        if ($cachedMetadata -and -not $RefreshAcoustId) {
            foreach ($field in @('acoustid_name', 'acoustid_album', 'name_source')) {
                if ($cachedMetadata.Contains($field)) { $out[$field] = $cachedMetadata[$field] }
            }
            Write-Host "  $fname -> cached: $($cachedMetadata.acoustid_name)"
        } elseif (-not $SkipAcoustId -and $track.chromaprint -and $track.duration_ms -gt 0) {
            $durationSec = [math]::Round($track.duration_ms / 1000)
            $lookup = Invoke-AcoustIdLookup -Fingerprint $track.chromaprint -DurationSec $durationSec
            if ($lookup) {
                $out["acoustid_name"] = $lookup.name
                if ($lookup.album) { $out["acoustid_album"] = $lookup.album }
                $out["name_source"] = "acoustid"
                $display = $lookup.name
                if ($lookup.album) { $display += " [$($lookup.album)]" }
                Write-Host "  $fname -> $display"
            } elseif ($cachedMetadata) {
                foreach ($field in @('acoustid_name', 'acoustid_album', 'name_source')) {
                    if ($cachedMetadata.Contains($field)) { $out[$field] = $cachedMetadata[$field] }
                }
                Write-Host "  $fname -> lookup returned no usable result; preserved cached metadata"
            } else {
                $tracklistName = Get-TracklistName -Track $track -TracklistLookup $TracklistLookup
                if ($tracklistName) {
                    $out["acoustid_name"] = $tracklistName
                    $out["tracklist_name"] = $tracklistName
                    $out["name_source"] = "tracklist"
                    Write-Host "  $fname -> tracklist: $tracklistName"
                } else {
                    Write-Host "  $fname -> no AcoustID match"
                }
            }
        } elseif ($cachedMetadata) {
            foreach ($field in @('acoustid_name', 'acoustid_album', 'name_source')) {
                if ($cachedMetadata.Contains($field)) { $out[$field] = $cachedMetadata[$field] }
            }
            Write-Host "  $fname -> lookup skipped; preserved cached metadata"
        } else {
            $tracklistName = Get-TracklistName -Track $track -TracklistLookup $TracklistLookup
            if ($tracklistName) {
                $out["acoustid_name"] = $tracklistName
                $out["tracklist_name"] = $tracklistName
                $out["name_source"] = "tracklist"
                Write-Host "  $fname -> tracklist: $tracklistName"
            } else {
                Write-Host "  $fname -> fingerprinted (no lookup)"
            }
        }
        $resultTracks += [PSCustomObject]$out
    }
    return $resultTracks
}

function Write-ChromaprintInfo {
    param(
        [string]$Path,
        [string]$AlbumName,
        [string]$SourceZip,
        [string]$SourceSha1,
        [string]$SourceSha256,
        [array]$Tracks
    )
    $lines = @()
    $lines += "// chromaprint_info.json5 -- Fingerprint data for album: $AlbumName"
    $lines += "// Generated by fingerprint_mission_zip_music.ps1"
    $lines += "{"
    $lines += "  `"album`": `"$(Escape-JsonString $AlbumName)`","
    $lines += "  `"source_zip`": `"$(Escape-JsonString $SourceZip)`","
    $lines += "  `"source_sha1`": `"$SourceSha1`","
    $lines += "  `"source_sha256`": `"$SourceSha256`","
    $lines += "  `"complete`": true,"
    $lines += "  `"tracks`": ["
    for ($i = 0; $i -lt $Tracks.Count; $i++) {
        $t = $Tracks[$i]
        $comma = if ($i -lt $Tracks.Count - 1) { "," } else { "" }
        $line = "    {`"filename`": `"$(Escape-JsonString $t.filename)`""
        $sourcePath = Get-OptionalPropertyValue $t "source_path"
        $originalName = Get-OptionalPropertyValue $t "original_name"
        if ($sourcePath) { $line += ", `"source_path`": `"$(Escape-JsonString $sourcePath)`"" }
        if ($originalName) { $line += ", `"original_name`": `"$(Escape-JsonString $originalName)`"" }
        $line += ", `"chromaprint`": `"$(Escape-JsonString $t.chromaprint)`""
        $line += ", `"duration_ms`": $($t.duration_ms)"
        foreach ($field in @("acoustid_name", "acoustid_album", "tracklist_name", "name_source")) {
            $value = Get-OptionalPropertyValue $t $field
            if ($value) { $line += ", `"$field`": `"$(Escape-JsonString $value)`"" }
        }
        $line += "}$comma"
        $lines += $line
    }
    $lines += "  ]"
    $lines += "}"
    [System.IO.File]::WriteAllText($Path, ($lines -join "`n"), [System.Text.UTF8Encoding]::new($false))
}

function Test-MissionFingerprintCacheIdentity {
    param(
        [Parameter(Mandatory = $true)][object]$ExistingInfo,
        [Parameter(Mandatory = $true)][string]$SourceZip,
        [Parameter(Mandatory = $true)][string]$SourceSha1,
        [Parameter(Mandatory = $true)][string]$SourceSha256
    )

    $storedSourceZip = [string]$ExistingInfo.source_zip
    $storedSha1 = [string]$ExistingInfo.source_sha1
    $storedSha256 = [string]$ExistingInfo.source_sha256
    return $ExistingInfo.complete -ceq $true -and
    $storedSourceZip -ceq $SourceZip -and
    $storedSha1 -cmatch '^[0-9a-f]{40}$' -and $storedSha1 -ceq $SourceSha1 -and
    $storedSha256 -cmatch '^[0-9a-f]{64}$' -and $storedSha256 -ceq $SourceSha256
}

if ($BudgetTestOnly) { return }

if (-not $SkipBuild -and -not $FingerprintExePath) {
    Write-Host "Building fingerprint_audio.exe..."
    $srcDir = Join-Path $repoRoot "android/app/src/main/cpp/extract"
    if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
        cmake -S $srcDir -B $buildDir 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "fingerprint_audio CMake configuration failed with exit code $LASTEXITCODE" }
    }
    cmake --build $buildDir --config Release --target fingerprint_audio 2>&1 | ForEach-Object {
        if ($_ -match 'error') { Write-Host $_ }
    }
    if ($LASTEXITCODE -ne 0) { throw "fingerprint_audio build failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $fpExe)) { Write-Error "Failed to build fingerprint_audio.exe" }
}
if (-not (Test-Path $fpExe)) { Write-Error "fingerprint_audio.exe not found: $fpExe" }

$script:acoustIdKey = $null
if (-not $SkipAcoustId) {
    $configPath = Join-Path $repoRoot "android/acoustid_config.json5"
    if (Test-Path $configPath) {
        try {
            $cfg = Read-Json5File $configPath
            $script:acoustIdKey = $cfg.api_key
        } catch {
            Write-Warning "Failed to parse acoustid_config.json5: $_"
        }
    }
    if (-not $script:acoustIdKey) {
        Write-Warning "No AcoustID API key found. Skipping lookups"
        $SkipAcoustId = $true
    }
}
$script:lastRequestTime = [datetime]::MinValue
$script:minDelayMs = 350

$zipFiles = @(Get-ChildItem $MissionDir -File |
        Where-Object { $_.Extension -match '^\.(zip|7z|rar)$' } |
        Sort-Object Name)
if ($Zip) {
    $zipFiles = @($zipFiles | Where-Object { $_.Name -eq $Zip -or $_.BaseName -eq $Zip })
    if ($zipFiles.Count -eq 0) { Write-Error "No mission ZIP found matching: $Zip" }
}

Write-Host "Found $($zipFiles.Count) mission ZIP(s) to process"

$processed = 0
$withAudio = 0
$failed = @()

foreach ($zipFile in $zipFiles) {
    $albumName = "Mission ZIP - $($zipFile.BaseName)"
    $albumDir = Join-Path $OutputRoot $albumName
    $infoFile = Join-Path $albumDir "chromaprint_info.json5"
    $sourceSha1 = Get-Sha1File $zipFile.FullName
    $sourceSha256 = (Get-FileHash -LiteralPath $zipFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $tracklistLookup = @{}
    $existingTracks = @{}
    $workDir = $null

    Write-Host "`n=== $($zipFile.Name) ==="
    try {
        $tracklistLookup = Read-MissionTracklist -ZipFile $zipFile
        if (Test-Path $infoFile) {
            $existingInfo = Read-Json5File $infoFile
            foreach ($t in @($existingInfo.tracks)) {
                $filename = Get-OptionalPropertyValue $t "filename"
                if ($filename) { $existingTracks[[string]$filename] = $t }
            }
        }
        if ((Test-Path $infoFile) -and -not $Force) {
            $cacheMatchesSource = Test-MissionFingerprintCacheIdentity -ExistingInfo $existingInfo `
                -SourceZip $zipFile.Name -SourceSha1 $sourceSha1 -SourceSha256 $sourceSha256
            if (-not $cacheMatchesSource) {
                Write-Host "  Source identity is missing or changed, reprocessing"
            } else {
                if ($SkipAcoustId) {
                    Write-Host "  Already fingerprinted, skipping"
                    $processed++
                    $withAudio++
                    continue
                }
                $tracks = Add-AcoustIdResults -Tracks @($existingInfo.tracks) -ExistingTracks $existingTracks -TracklistLookup $tracklistLookup
                Write-ChromaprintInfo -Path $infoFile -AlbumName $albumName -SourceZip $zipFile.Name `
                    -SourceSha1 $sourceSha1 -SourceSha256 $sourceSha256 -Tracks $tracks
                Write-Host "  Updated $infoFile"
                $processed++
                $withAudio++
                continue
            }
        }

        $workDir = Join-Path $OutputRoot ".$albumName-$([Guid]::NewGuid().ToString('N'))"
        New-Item -ItemType Directory -Path $workDir | Out-Null
        $workInfoFile = Join-Path $workDir "chromaprint_info.json5"
        $tempRoot = Join-Path $workDir "_mission_zip_extract_temp"
        New-Item -ItemType Directory -Path $tempRoot | Out-Null

        $sourceMap = @{}
        $script:ArchiveBudget = @{ Entries = 0; ActualBytes = 0L; Started = [DateTime]::UtcNow }
        try {
            $count = Extract-MissionArchiveAudio -ArchivePath $zipFile.FullName -OutputDir $workDir -SourcePrefix $zipFile.Name -SourceMap $sourceMap -TempRoot $tempRoot
        } finally {
            Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }

        if ($count -eq 0) {
            Write-Host "  No OGG/MP3/FLAC tracks found"
            Remove-Item $workDir -Recurse -Force -ErrorAction SilentlyContinue
            $processed++
            continue
        }
        Write-Host "  Extracted $count audio file(s)"
        if ($count -ne $sourceMap.Count) {
            throw "Extracted audio count $count does not match source map count $($sourceMap.Count)"
        }
        $expectedAudioNames = @($sourceMap.Keys | ForEach-Object { [string]$_ })

        $fpStdout = Join-Path $workDir "_fp_stdout.json"
        $fpStderr = Join-Path $workDir "_fp_stderr.txt"
        $proc = Start-Process -FilePath $fpExe -ArgumentList "`"$workDir`"" `
            -RedirectStandardOutput $fpStdout -RedirectStandardError $fpStderr `
            -NoNewWindow -Wait -PassThru
        if (Test-Path $fpStderr) {
            Get-Content $fpStderr | ForEach-Object { Write-Host "  $_" }
            Remove-Item $fpStderr -ErrorAction SilentlyContinue
        }
        if (-not (Test-Path $fpStdout)) {
            throw "fingerprint_audio.exe produced no output"
        }
        $fpData = @(Get-Content $fpStdout -Raw | ConvertFrom-Json)
        Remove-Item $fpStdout -ErrorAction SilentlyContinue
        if ($proc.ExitCode -ne 0) {
            throw "fingerprint_audio.exe failed with exit code $($proc.ExitCode)"
        }
        Assert-DxxFingerprintAudioResults -ExpectedNames $expectedAudioNames -Results $fpData
        Write-Host "  Fingerprinted $($fpData.Count) file(s)"

        $tracks = @()
        foreach ($fp in $fpData) {
            $source = $sourceMap[$fp.filename]
            $track = [ordered]@{
                filename      = [string]$fp.filename
                source_path   = if ($source) { [string]$source.source_path } else { [string]$fp.filename }
                original_name = if ($source) { [string]$source.original_name } else { [string]$fp.filename }
                chromaprint   = [string]$fp.chromaprint
                duration_ms   = [int]$fp.duration_ms
            }
            $tracks += [PSCustomObject]$track
        }
        $tracks = Add-AcoustIdResults -Tracks $tracks -ExistingTracks $existingTracks `
            -TracklistLookup $tracklistLookup -RefreshAcoustId:$Force
        Write-ChromaprintInfo -Path $workInfoFile -AlbumName $albumName -SourceZip $zipFile.Name `
            -SourceSha1 $sourceSha1 -SourceSha256 $sourceSha256 -Tracks $tracks
        Publish-ExtractionDirectory -StagingDirectory $workDir -DestinationDirectory $albumDir
        Write-Host "  Wrote $infoFile"
        $processed++
        $withAudio++
    } catch {
        if ($workDir -and (Test-Path -LiteralPath $workDir)) {
            Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
        }
        $failed += "$($zipFile.Name): $($_.Exception.Message)"
        Write-Warning "  Failed: $($_.Exception.Message)"
    }
}

Write-Host "`nSummary: $processed processed, $withAudio with audio, $($failed.Count) failed"
foreach ($failure in $failed) {
    Write-Host "  $failure"
}

if ($failed.Count -gt 0) {
    exit 1
}

if ($UpdateDatabase) {
    Write-Host "`nUpdating known_discs.json5 from album sidecars"
    & "$PSScriptRoot/update_known_discs_albums.ps1" -Force
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Write-Error "update_known_discs_albums.ps1 failed with exit code $LASTEXITCODE"
    }
}

Write-Host "`nAll done"
