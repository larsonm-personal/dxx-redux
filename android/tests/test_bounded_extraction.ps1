$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\bounded_extraction.ps1')
. (Join-Path $repoRoot 'android\helpers\verified_dependencies.ps1')

$testRoot = Join-Path $repoRoot "android\temp\bounded_extract_$([guid]::NewGuid().ToString('N'))"
$junctionPath = $null

function New-ZipFixture {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][object[]]$Entries
    )

    $file = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $archive = [IO.Compression.ZipArchive]::new($file, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($spec in $Entries) {
            $entry = $archive.CreateEntry([string]$spec.Name)
            if ($null -ne $spec.Data) {
                $bytes = [Text.Encoding]::UTF8.GetBytes([string]$spec.Data)
                $stream = $entry.Open()
                try {
                    $stream.Write($bytes, 0, $bytes.Length)
                } finally {
                    $stream.Dispose()
                }
            }
        }
    } finally {
        $archive.Dispose()
        $file.Dispose()
    }
}

function Assert-RejectedArchive {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][object[]]$Entries
    )

    $id = [guid]::NewGuid().ToString('N')
    $zipPath = Join-Path $testRoot "$id.zip"
    $output = Join-Path $testRoot "$id-output"
    $sentinel = Join-Path $testRoot 'escaped.txt'
    [IO.File]::WriteAllText($sentinel, 'unchanged')
    New-ZipFixture -Path $zipPath -Entries $Entries

    $rejected = $false
    try {
        Expand-BoundedZipArchive -ArchivePath $zipPath -DestinationPath $output `
            -MaxEntryBytes 1024 -MaxTotalBytes 4096 -FreeSpaceHeadroomBytes 0
    } catch {
        $rejected = $true
    }
    if (-not $rejected) { throw "Unsafe archive was accepted: $Label" }
    if (Test-Path -LiteralPath $output) { throw "Rejected archive left output residue: $Label" }
    if ([IO.File]::ReadAllText($sentinel) -ne 'unchanged') {
        throw "Rejected archive changed an outside file: $Label"
    }
    Write-Host "PASS: rejected $Label"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
New-Item -ItemType Directory -Path $testRoot | Out-Null
try {
    $validZip = Join-Path $testRoot 'valid.zip'
    New-ZipFixture -Path $validZip -Entries @(
        [pscustomobject]@{ Name = 'nested/'; Data = $null },
        [pscustomobject]@{ Name = 'nested/small.bin'; Data = '0123456789abcdef' }
    )
    $output = Join-Path $testRoot 'output'
    Expand-BoundedZipArchive -ArchivePath $validZip -DestinationPath $output `
        -MaxEntryBytes 16 -MaxTotalBytes 16 -FreeSpaceHeadroomBytes 0
    if ((Get-Item (Join-Path $output 'nested\small.bin')).Length -ne 16) {
        throw 'bounded ZIP success case failed'
    }
    Write-Host 'PASS: valid nested archive extracted'

    foreach ($case in @(
            [pscustomobject]@{ Label = 'forward traversal'; Name = '../escaped.txt' },
            [pscustomobject]@{ Label = 'backslash traversal'; Name = '..\escaped.txt' },
            [pscustomobject]@{ Label = 'absolute path'; Name = '/absolute.txt' },
            [pscustomobject]@{ Label = 'drive-qualified path'; Name = 'C:/drive.txt' },
            [pscustomobject]@{ Label = 'forward UNC path'; Name = '//server/share.txt' },
            [pscustomobject]@{ Label = 'backslash UNC path'; Name = '\\server\share.txt' },
            [pscustomobject]@{ Label = 'mixed separators'; Name = 'safe\../escaped.txt' },
            [pscustomobject]@{ Label = 'empty component'; Name = 'safe//empty.txt' },
            [pscustomobject]@{ Label = 'dot component'; Name = 'safe/./dot.txt' },
            [pscustomobject]@{ Label = 'parent component'; Name = 'safe/../parent.txt' },
            [pscustomobject]@{ Label = 'sibling prefix'; Name = '../output-sibling/file.txt' },
            [pscustomobject]@{ Label = 'trailing dot alias'; Name = 'safe./file.txt' },
            [pscustomobject]@{ Label = 'alternate data stream'; Name = 'safe/file.txt:stream' },
            [pscustomobject]@{ Label = 'reserved device'; Name = 'NUL.txt' }
        )) {
        Assert-RejectedArchive -Label $case.Label -Entries @(
            [pscustomobject]@{ Name = 'safe/first.bin'; Data = 'safe' },
            [pscustomobject]@{ Name = $case.Name; Data = 'evil' }
        )
    }

    Assert-RejectedArchive -Label 'duplicate destination' -Entries @(
        [pscustomobject]@{ Name = 'same.txt'; Data = 'first' },
        [pscustomobject]@{ Name = 'same.txt'; Data = 'second' }
    )
    Assert-RejectedArchive -Label 'file and child conflict' -Entries @(
        [pscustomobject]@{ Name = 'node'; Data = 'file' },
        [pscustomobject]@{ Name = 'node/child.txt'; Data = 'child' }
    )
    if ($env:OS -eq 'Windows_NT') {
        Assert-RejectedArchive -Label 'case-insensitive duplicate' -Entries @(
            [pscustomobject]@{ Name = 'Case.txt'; Data = 'first' },
            [pscustomobject]@{ Name = 'case.txt'; Data = 'second' }
        )
    }

    $oversizeRejected = $false
    try {
        Expand-BoundedZipArchive -ArchivePath $validZip `
            -DestinationPath (Join-Path $testRoot 'oversize') -MaxEntryBytes 15 `
            -FreeSpaceHeadroomBytes 0
    } catch {
        $oversizeRejected = $true
    }
    if (-not $oversizeRejected) { throw 'bounded ZIP did not reject an oversized entry' }

    $entryCountRejected = $false
    try {
        Expand-BoundedZipArchive -ArchivePath $validZip `
            -DestinationPath (Join-Path $testRoot 'entries') -MaxEntries 0 `
            -FreeSpaceHeadroomBytes 0
    } catch {
        $entryCountRejected = $true
    }
    if (-not $entryCountRejected) { throw 'bounded ZIP did not reject an excessive entry count' }

    $junctionTarget = Join-Path $testRoot 'junction-target'
    $junctionPath = Join-Path $testRoot 'junction-root'
    New-Item -ItemType Directory -Path $junctionTarget | Out-Null
    if ($env:OS -eq 'Windows_NT') {
        New-Item -ItemType Junction -Path $junctionPath -Target $junctionTarget | Out-Null
    } else {
        New-Item -ItemType SymbolicLink -Path $junctionPath -Target $junctionTarget | Out-Null
    }
    $reparseRejected = $false
    try {
        Expand-BoundedZipArchive -ArchivePath $validZip -DestinationPath $junctionPath `
            -FreeSpaceHeadroomBytes 0
    } catch {
        $reparseRejected = $true
    }
    if (-not $reparseRejected) { throw 'bounded ZIP accepted a reparse-point root' }
    if (Get-ChildItem -LiteralPath $junctionTarget -Force | Select-Object -First 1) {
        throw 'reparse-point rejection wrote through the link'
    }
    Remove-Item -LiteralPath $junctionPath -Force
    $junctionPath = $null
    Write-Host 'PASS: rejected reparse-point root'

    $verifiedPackages = @(
        [pscustomobject]@{
            Name = 'desc14sw.exe'
            Sha256 = '3dadb7fbc01efce2904d0908c55d9a9cf1f402e83bf771970552efaca15efcb0'
        },
        [pscustomobject]@{
            Name = 'descent 1 demo 1-4.zip'
            Sha256 = '64741386ad88d7a60a9529383affb4d2415e11d907ea6dbab8a8a66e1c20b745'
        },
        [pscustomobject]@{
            Name = 'descent 2 demo 1-0.zip'
            Sha256 = 'a7c31eae6dfd22e1f6a4c0b9fb2dfb2e25197831bc43c3e9d65734c7fa446c4d'
        },
        [pscustomobject]@{
            Name = 'd2demo10.zip'
            Sha256 = 'f8d005670fe5cd17e07ca9bf4022f1045aed436639c37f1e83dd647e14fcec1f'
        }
    )
    $packageRoot = Join-Path $repoRoot 'game_data\demo installers'
    foreach ($package in $verifiedPackages) {
        $packagePath = Join-Path $packageRoot $package.Name
        if (-not (Test-Path -LiteralPath $packagePath -PathType Leaf)) {
            Write-Host "SKIP: verified package unavailable: $($package.Name)"
            continue
        }
        Assert-DxxFileSha256 -Path $packagePath -ExpectedSha256 $package.Sha256 `
            -Label $package.Name | Out-Null
        $packageOutput = Join-Path $testRoot "package-$([guid]::NewGuid().ToString('N'))"
        Expand-BoundedZipArchive -ArchivePath $packagePath -DestinationPath $packageOutput `
            -FreeSpaceHeadroomBytes 0
        if (-not (Get-ChildItem -LiteralPath $packageOutput -Filter 'INSTALL.EXE' -File -Recurse)) {
            throw "Verified package did not extract INSTALL.EXE: $($package.Name)"
        }
        Remove-Item -LiteralPath $packageOutput -Recurse -Force
        Write-Host "PASS: verified package extracted: $($package.Name)"
    }

    Write-Host 'bounded extraction tests passed'
} finally {
    if ($junctionPath -and (Test-Path -LiteralPath $junctionPath)) {
        Remove-Item -LiteralPath $junctionPath -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
