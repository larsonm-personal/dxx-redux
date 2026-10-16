#!/usr/bin/env pwsh
# Materialize verified public PC demos without executing their DOS installers
param(
    [ValidateSet('d1', 'd2', 'both')][string]$Game = 'both',
    [switch]$Download,
    [switch]$RequireArchives,
    [string]$ArchiveDirectory,
    [string]$DataRoot,
    [string]$ExtractorPath,
    [string]$BuildDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'shareware_test_data.ps1')
. (Join-Path $PSScriptRoot 'bounded_extraction.ps1')
if (-not $ArchiveDirectory) { $ArchiveDirectory = Join-Path $repoRoot 'game_data/demo installers' }
if (-not $DataRoot) { $DataRoot = Join-Path $repoRoot 'game_data/shareware' }
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repoRoot 'android/tests/build-shareware-fixture' }
$ArchiveDirectory = [IO.Path]::GetFullPath($ArchiveDirectory)
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$games = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
$toolSource = Join-Path $repoRoot 'android/tests/shareware_fixture_tool'
New-Item -ItemType Directory -Path $DataRoot -Force | Out-Null
$lock = [IO.File]::Open((Join-Path $DataRoot '.setup.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    foreach ($name in $games) {
        $config = Get-SharewareTestGameConfig -RepoRoot $repoRoot -Game $name
        $destination = Join-Path $DataRoot $name
        $existingVerified = $false
        if (Test-Path -LiteralPath $destination -PathType Container) {
            try {
                $verified = Assert-SharewareTestDataDirectory -Config $config -DataDir $destination
                Write-Host "Verified $name shareware: $verified"
                $existingVerified = $true
                if (-not $RequireArchives) { continue }
            } catch {
                Write-Host "Rebuilding invalid $name shareware fixture: $($_.Exception.Message)"
            }
        }
        $archivePath = Join-Path $ArchiveDirectory $config.Archive
        if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf) -and -not $Download) {
            throw "Missing cached public package: $archivePath. Rerun with -Download to fetch the pinned package"
        }
        $work = Join-Path $DataRoot ".setup-$name-$([Guid]::NewGuid().ToString('N'))"
        $stage = Join-Path $DataRoot ".stage-$name-$([Guid]::NewGuid().ToString('N'))"
        New-Item -ItemType Directory -Path $work | Out-Null
        try {
            if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
                New-Item -ItemType Directory -Path $ArchiveDirectory -Force | Out-Null
                $downloadManifest = Join-Path $work 'package.conf'
                [IO.File]::WriteAllText($downloadManifest, "SHAREWARE_URL=$($config.DownloadUrl)`nSHAREWARE_SHA256=$($config.ArchiveSha256)`n")
                & cmake "-DDXX_DEPENDENCY_MANIFEST=$downloadManifest" `
                    "-DDXX_DEPENDENCY_CACHE_DIR=$(Join-Path $ArchiveDirectory '.verified-cache')" `
                    "-DDXX_SHAREWARE_ARCHIVE_PATH=$archivePath" -P (Join-Path $toolSource 'download.cmake') | Out-Host
                if ($LASTEXITCODE -ne 0) { throw "Pinned public package download failed: $($config.Archive)" }
            }
            Assert-DxxFileSha256 -Path $archivePath -ExpectedSha256 $config.ArchiveSha256 -Label $config.Archive | Out-Null
            if ((Get-Item -LiteralPath $archivePath).Length -ne $config.ArchiveSize) {
                throw "Public package size mismatch: $archivePath"
            }
            # First-play imports consume original packages even when extracted fixtures are cached
            if ($existingVerified) { continue }
            if (-not $ExtractorPath) {
                & cmake -S $toolSource -B $BuildDirectory | Out-Host
                if ($LASTEXITCODE -ne 0) { throw 'Shareware extractor CMake configuration failed' }
                & cmake --build $BuildDirectory --config Release --target test_sow_direct | Out-Host
                if ($LASTEXITCODE -ne 0) { throw 'Shareware extractor build failed' }
                $candidates = @('test_sow_direct', 'test_sow_direct.exe', 'Release/test_sow_direct.exe')
                $ExtractorPath = @($candidates | ForEach-Object { Join-Path $BuildDirectory $_ } |
                        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }) | Select-Object -First 1
                if (-not $ExtractorPath) { throw 'Built test_sow_direct executable was not found' }
            }
            $ExtractorPath = (Resolve-Path -LiteralPath $ExtractorPath).Path
            $unpacked = Join-Path $work 'package'
            Expand-BoundedZipArchive -ArchivePath $archivePath -DestinationPath $unpacked `
                -MaxEntries 32 -MaxEntryBytes 8MB -MaxTotalBytes 16MB
            $volumes = @(Get-ChildItem -LiteralPath $unpacked -File -Recurse |
                    Where-Object Extension -IEQ '.sow' | Sort-Object Name | Select-Object -ExpandProperty FullName)
            if ($volumes.Count -eq 0) { throw "No SOW volumes in $archivePath" }
            $extracted = Join-Path $work 'extracted'
            New-Item -ItemType Directory -Path $extracted | Out-Null
            & $ExtractorPath --volumes $extracted @volumes | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "Native SOW assembly failed for $($config.Archive)" }
            Assert-SharewareTestDataDirectory -Config $config -DataDir $extracted -AllowExtraFiles | Out-Null
            New-Item -ItemType Directory -Path $stage | Out-Null
            foreach ($required in $config.RequiredFiles) {
                $file = Get-ChildItem -LiteralPath $extracted -File | Where-Object Name -IEQ $required
                Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $stage $required)
            }
            $provenance = New-ExtractionProvenance -Policy 'public-shareware-fixture-v1' -Sources @(
                (Get-ExtractionPathIdentity -Path $archivePath -Name $config.Archive)
            ) -Tools @(
                (Get-ExtractionPathIdentity -Path $ExtractorPath -Name 'test_sow_direct'),
                (Get-ExtractionPathIdentity -Path $PSCommandPath -Name 'setup_shareware_test_data.ps1')
            )
            Write-ExtractionCompletionManifest -Directory $stage -Provenance $provenance
            Assert-SharewareTestDataDirectory -Config $config -DataDir $stage | Out-Null
            Publish-ExtractionDirectory -StagingDirectory $stage -DestinationDirectory $destination
            Write-Host "Prepared $name shareware: $destination"
        } finally {
            Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
} finally {
    $lock.Dispose()
}
