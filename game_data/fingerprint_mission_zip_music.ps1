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
    [string]$Zip,
    [string]$MissionDir = (Join-Path $PSScriptRoot "mission_files"),
    [string]$OutputRoot = (Join-Path $PSScriptRoot "music")
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path
. "$repoRoot\android\helpers\test_env.ps1"

Add-Type -AssemblyName System.IO.Compression.FileSystem

$buildDir = Join-Path $repoRoot "android/tests/build"
$fpExe = Join-Path $buildDir "Release/fingerprint_audio.exe"

function Get-7zaPath {
    $onPath = Get-Command 7za -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $onPath = Get-Command 7z -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $result = & "$repoRoot/android/get_deps/helpers/get_7zip.ps1"
    $candidate = @($result | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Last 1)
    if ($candidate) { return $candidate[0] }
    Write-Error "7za.exe not found. Run android/get_deps/helpers/get_7zip.ps1 or install 7-Zip"
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
    $buffer = New-Object byte[] 65536
    $remaining = $Length
    while ($remaining -gt 0) {
        $want = [Math]::Min([Int64]$buffer.Length, $remaining)
        $read = $InputStream.Read($buffer, 0, [int]$want)
        if ($read -le 0) { throw "Unexpected end of stream while copying $Length bytes" }
        $OutputStream.Write($buffer, 0, $read)
        $remaining -= $read
    }
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
        try { $inStream.CopyTo($outStream) } finally { $outStream.Dispose() }
    } finally {
        $inStream.Dispose()
    }
    return $tempPath
}

function Find-ZipPayloadOffset {
    param([byte[]]$Bytes)
    for ($i = 0; $i -le $Bytes.Length - 4; $i++) {
        if ($Bytes[$i] -eq 0x50 -and $Bytes[$i + 1] -eq 0x4b -and
            $Bytes[$i + 2] -eq 0x03 -and $Bytes[$i + 3] -eq 0x04) {
            return $i
        }
    }
    return -1
}

function Get-ZipPayloadPath {
    param([string]$ArchivePath, [string]$TempRoot)
    $bytes = [System.IO.File]::ReadAllBytes($ArchivePath)
    $offset = Find-ZipPayloadOffset $bytes
    if ($offset -lt 0) { throw "Could not find ZIP payload in $ArchivePath" }
    if ($offset -eq 0) { return $ArchivePath }
    $zipPath = Join-Path $TempRoot "$([Guid]::NewGuid().ToString()).zip"
    $outStream = [System.IO.File]::Create($zipPath)
    try {
        $outStream.Write($bytes, $offset, $bytes.Length - $offset)
    } finally {
        $outStream.Dispose()
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
    try {
        $magic = Read-ExactBytes -Stream $stream -Count 3
        if ($null -eq $magic -or [System.Text.Encoding]::ASCII.GetString($magic) -ne "DHF") {
            return 0
        }
        while ($stream.Position -lt $stream.Length) {
            $nameBytes = Read-ExactBytes -Stream $stream -Count 13
            if ($null -eq $nameBytes) { break }
            $lenBytes = Read-ExactBytes -Stream $stream -Count 4
            if ($null -eq $lenBytes) { break }
            $entryName = Get-HogEntryName $nameBytes
            $length = [Int64][BitConverter]::ToUInt32($lenBytes, 0)
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
                    try { $inStream.CopyTo($outStream) } finally { $outStream.Dispose() }
                } finally {
                    $inStream.Dispose()
                }
                $count++
            } elseif ($ext -eq ".hog") {
                $hogPath = Copy-EntryToTempFile -Entry $entry -TempRoot $TempRoot -Extension ".hog"
                try {
                    $count += Extract-HogAudio -HogPath $hogPath -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap
                } finally {
                    Remove-Item $hogPath -Force -ErrorAction SilentlyContinue
                }
            } elseif ($ext -eq ".dxa" -or $ext -eq ".zip") {
                $nestedPath = Copy-EntryToTempFile -Entry $entry -TempRoot $TempRoot -Extension $ext
                try {
                    $count += Extract-ZipAudio -ArchivePath $nestedPath -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
                } finally {
                    Remove-Item $nestedPath -Force -ErrorAction SilentlyContinue
                }
            } elseif ($ext -eq ".rar") {
                $nestedPath = Copy-EntryToTempFile -Entry $entry -TempRoot $TempRoot -Extension $ext
                try {
                    $count += Extract-RarAudio -ArchivePath $nestedPath -OutputDir $OutputDir -SourcePrefix $sourcePath -SourceMap $SourceMap -TempRoot $TempRoot
                } finally {
                    Remove-Item $nestedPath -Force -ErrorAction SilentlyContinue
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
    foreach ($file in $files) {
        $relative = [System.IO.Path]::GetRelativePath($DirectoryPath, $file.FullName).Replace('\', '/')
        $sourcePath = if ($SourcePrefix) { "$SourcePrefix!$relative" } else { $relative }
        $ext = $file.Extension.ToLowerInvariant()
        if (Test-AudioName $file.Name) {
            $dest = New-UniqueAudioPath -OutputDir $OutputDir -SourcePath $sourcePath -SourceMap $SourceMap
            Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
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
        $output = & $sevenZip x "-o$extractDir" -y -- $ArchivePath 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "7z extract failed for ${ArchivePath}: $($output -join ' ')"
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
        $output = & $tar -xf $ArchivePath -C $extractDir 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "RAR extract failed for ${ArchivePath}: $($output -join ' ')"
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

function Read-Json5File {
    param([string]$Path)
    $raw = Get-Content $Path -Raw
    $stripped = $raw -replace '//[^\n]*', '' -replace '/\*[\s\S]*?\*/', ''
    return $stripped | ConvertFrom-Json
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
    $name = if ($Track.original_name) { [string]$Track.original_name } else { [string]$Track.filename }
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
    $doc = Read-Json5File $path
    if ([string]$doc.schema -ne "dxx-mission-tracklist-v1") {
        throw "Unsupported tracklist schema in ${path}: $($doc.schema)"
    }
    $lookup = @{}
    foreach ($entry in @($doc.tracks)) {
        $name = if ($entry.title) { [string]$entry.title } else { [string]$entry.name }
        if (-not $name) { continue }
        Add-TracklistKey -Lookup $lookup -Prefix "filename" -Value $entry.filename -Name $name
        Add-TracklistKey -Lookup $lookup -Prefix "source_path" -Value $entry.source_path -Name $name
        Add-TracklistKey -Lookup $lookup -Prefix "original_name" -Value $entry.original_name -Name $name
        Add-TracklistKey -Lookup $lookup -Prefix "slot" -Value $entry.slot -Name $name
        if ($entry.level) {
            Add-TracklistKey -Lookup $lookup -Prefix "slot" -Value "level$($entry.level)" -Name $name
        }
    }
    Write-Host "  Using tracklist sidecar $path"
    return $lookup
}

function Get-TracklistName {
    param($Track, [hashtable]$TracklistLookup)
    if (-not $TracklistLookup -or $TracklistLookup.Count -eq 0) { return "" }
    foreach ($candidate in @(
            "filename:$([string]$Track.filename)",
            "source_path:$([string]$Track.source_path)",
            "original_name:$([string]$Track.original_name)",
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
    param([array]$Tracks, [hashtable]$ExistingTracks, [hashtable]$TracklistLookup = @{})
    $resultTracks = @()
    foreach ($track in $Tracks) {
        $fname = [string]$track.filename
        $out = [ordered]@{
            filename    = $fname
            chromaprint = [string]$track.chromaprint
            duration_ms = [int]$track.duration_ms
        }
        if ($track.source_path) { $out["source_path"] = [string]$track.source_path }
        if ($track.original_name) { $out["original_name"] = [string]$track.original_name }

        $existing = $ExistingTracks[$fname]
        $existingNameSource = if ($existing -and $existing.name_source) { [string]$existing.name_source } else { "" }
        $canReuseExistingLookup = $existing -and $existing.acoustid_name -and
            $existing.chromaprint -eq $track.chromaprint -and $existingNameSource -ne "tracklist"
        if ($canReuseExistingLookup) {
            $out["acoustid_name"] = [string]$existing.acoustid_name
            if ($existing.acoustid_album) { $out["acoustid_album"] = [string]$existing.acoustid_album }
            if ($existing.name_source) { $out["name_source"] = [string]$existing.name_source }
            Write-Host "  $fname -> cached: $($existing.acoustid_name)"
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
        [array]$Tracks
    )
    $lines = @()
    $lines += "// chromaprint_info.json5 -- Fingerprint data for album: $AlbumName"
    $lines += "// Generated by fingerprint_mission_zip_music.ps1"
    $lines += "{"
    $lines += "  `"album`": `"$(Escape-JsonString $AlbumName)`","
    $lines += "  `"source_zip`": `"$(Escape-JsonString $SourceZip)`","
    $lines += "  `"source_sha1`": `"$SourceSha1`","
    $lines += "  `"tracks`": ["
    for ($i = 0; $i -lt $Tracks.Count; $i++) {
        $t = $Tracks[$i]
        $comma = if ($i -lt $Tracks.Count - 1) { "," } else { "" }
        $line = "    {`"filename`": `"$(Escape-JsonString $t.filename)`""
        if ($t.source_path) { $line += ", `"source_path`": `"$(Escape-JsonString $t.source_path)`"" }
        if ($t.original_name) { $line += ", `"original_name`": `"$(Escape-JsonString $t.original_name)`"" }
        $line += ", `"chromaprint`": `"$(Escape-JsonString $t.chromaprint)`""
        $line += ", `"duration_ms`": $($t.duration_ms)"
        if ($t.acoustid_name) { $line += ", `"acoustid_name`": `"$(Escape-JsonString $t.acoustid_name)`"" }
        if ($t.acoustid_album) { $line += ", `"acoustid_album`": `"$(Escape-JsonString $t.acoustid_album)`"" }
        if ($t.tracklist_name) { $line += ", `"tracklist_name`": `"$(Escape-JsonString $t.tracklist_name)`"" }
        if ($t.name_source) { $line += ", `"name_source`": `"$(Escape-JsonString $t.name_source)`"" }
        $line += "}$comma"
        $lines += $line
    }
    $lines += "  ]"
    $lines += "}"
    [System.IO.File]::WriteAllText($Path, ($lines -join "`n"), [System.Text.UTF8Encoding]::new($false))
}

if (-not $SkipBuild -and -not (Test-Path $fpExe)) {
    Write-Host "Building fingerprint_audio.exe..."
    $srcDir = Join-Path $repoRoot "android/app/src/main/cpp/extract"
    if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
        cmake -S $srcDir -B $buildDir 2>&1 | Out-Null
    }
    cmake --build $buildDir --config Release --target fingerprint_audio 2>&1 | ForEach-Object {
        if ($_ -match 'error') { Write-Host $_ }
    }
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

$zipFiles = Get-ChildItem $MissionDir -File | Where-Object { $_.Extension -match '^\.(zip|7z|rar)$' } | Sort-Object Name
if ($Zip) {
    $zipFiles = $zipFiles | Where-Object { $_.Name -eq $Zip -or $_.BaseName -eq $Zip }
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
    $tracklistLookup = @{}

    Write-Host "`n=== $($zipFile.Name) ==="
    try {
        $tracklistLookup = Read-MissionTracklist -ZipFile $zipFile
        if ((Test-Path $infoFile) -and -not $Force) {
            $existingInfo = Read-Json5File $infoFile
            $storedSha1 = [string]$existingInfo.source_sha1
            if ($storedSha1 -and $storedSha1 -ne $sourceSha1) {
                Write-Host "  Source ZIP changed, reprocessing"
            } else {
                $existingTracks = @{}
                foreach ($t in @($existingInfo.tracks)) { $existingTracks[$t.filename] = $t }
                if ($SkipAcoustId) {
                    Write-Host "  Already fingerprinted, skipping"
                    $processed++
                    $withAudio++
                    continue
                }
                $tracks = Add-AcoustIdResults -Tracks @($existingInfo.tracks) -ExistingTracks $existingTracks -TracklistLookup $tracklistLookup
                Write-ChromaprintInfo -Path $infoFile -AlbumName $albumName -SourceZip $zipFile.Name -SourceSha1 $sourceSha1 -Tracks $tracks
                Write-Host "  Updated $infoFile"
                $processed++
                $withAudio++
                continue
            }
        }

        if (-not (Test-Path $albumDir)) {
            New-Item -ItemType Directory -Path $albumDir -Force | Out-Null
        }
        Get-ChildItem $albumDir -File | Where-Object {
            $_.Extension -match '^\.(mp3|ogg|flac)$'
        } | Remove-Item -Force

        $tempRoot = Join-Path $albumDir "_mission_zip_extract_temp"
        if (Test-Path $tempRoot) { Remove-Item $tempRoot -Recurse -Force }
        New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

        $sourceMap = @{}
        try {
            $count = Extract-MissionArchiveAudio -ArchivePath $zipFile.FullName -OutputDir $albumDir -SourcePrefix $zipFile.Name -SourceMap $sourceMap -TempRoot $tempRoot
        } finally {
            Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }

        if ($count -eq 0) {
            Write-Host "  No OGG/MP3/FLAC tracks found"
            Remove-Item $albumDir -Recurse -Force -ErrorAction SilentlyContinue
            $processed++
            continue
        }
        Write-Host "  Extracted $count audio file(s)"

        $fpStdout = Join-Path $albumDir "_fp_stdout.json"
        $fpStderr = Join-Path $albumDir "_fp_stderr.txt"
        $proc = Start-Process -FilePath $fpExe -ArgumentList "`"$albumDir`"" `
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
        if ($proc.ExitCode -ne 0 -and $fpData.Count -eq 0) {
            throw "fingerprint_audio.exe failed with exit code $($proc.ExitCode)"
        }
        if ($fpData.Count -eq 0) {
            throw "fingerprint_audio.exe found no usable audio"
        }
        if ($proc.ExitCode -ne 0) {
            Write-Warning "  fingerprint_audio.exe reported failures, keeping $($fpData.Count) successful track(s)"
        }
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
        $tracks = Add-AcoustIdResults -Tracks $tracks -ExistingTracks @{} -TracklistLookup $tracklistLookup
        Write-ChromaprintInfo -Path $infoFile -AlbumName $albumName -SourceZip $zipFile.Name -SourceSha1 $sourceSha1 -Tracks $tracks
        Write-Host "  Wrote $infoFile"
        $processed++
        $withAudio++
    } catch {
        $failed += "$($zipFile.Name): $($_.Exception.Message)"
        Write-Warning "  Failed: $($_.Exception.Message)"
    }
}

Write-Host "`nSummary: $processed processed, $withAudio with audio, $($failed.Count) failed"
foreach ($failure in $failed) {
    Write-Host "  $failure"
}

if ($UpdateDatabase) {
    Write-Host "`nUpdating known_discs.json5 from album sidecars"
    & "$PSScriptRoot/update_known_discs_albums.ps1" -Force
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Write-Error "update_known_discs_albums.ps1 failed with exit code $LASTEXITCODE"
    }
}

Write-Host "`nAll done"
