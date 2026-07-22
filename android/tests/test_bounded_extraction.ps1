$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1')

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('dxx_bounded_extract_test_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zipPath = Join-Path $testRoot 'fixture.zip'
    $source = Join-Path $testRoot 'source'
    New-Item -ItemType Directory -Path $source | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $source 'small.bin'), (New-Object byte[] 16))
    [System.IO.Compression.ZipFile]::CreateFromDirectory($source, $zipPath)

    $output = Join-Path $testRoot 'output'
    Expand-BoundedZipArchive -ArchivePath $zipPath -DestinationPath $output -MaxEntryBytes 16 -MaxTotalBytes 16
    if ((Get-Item (Join-Path $output 'small.bin')).Length -ne 16) { throw 'bounded ZIP success case failed' }

    $rejected = $false
    try {
        Expand-BoundedZipArchive -ArchivePath $zipPath -DestinationPath (Join-Path $testRoot 'oversize') -MaxEntryBytes 15
    } catch {
        $rejected = $true
    }
    if (-not $rejected) { throw 'bounded ZIP did not reject an oversized entry' }

    $rejected = $false
    try {
        Expand-BoundedZipArchive -ArchivePath $zipPath -DestinationPath (Join-Path $testRoot 'entries') -MaxEntries 0
    } catch {
        $rejected = $true
    }
    if (-not $rejected) { throw 'bounded ZIP did not reject an excessive entry count' }

    Write-Host 'bounded extraction tests passed'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
