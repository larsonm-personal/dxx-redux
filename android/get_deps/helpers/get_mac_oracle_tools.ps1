#!/usr/bin/env pwsh

param([switch]$Force)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot))
. (Join-Path $PSScriptRoot 'Get-DepPlatform.ps1')
. (Join-Path $repoRoot 'android\helpers\verified_dependencies.ps1')

if ((Get-HostPlatform) -ne 'Windows') {
    throw 'The repository-pinned Mac oracle toolchain is currently available only for Windows'
}

$config = Read-DxxDependencyConfig -RepoRoot $repoRoot
$depBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing
$installRoot = Join-Path $depBase $config['PYTHON_ORACLE_DIR_NAME']
$pythonDir = Join-Path $installRoot 'python'
$machfsDir = Join-Path $installRoot 'machfs'

function Assert-InstalledOracleTools {
    Assert-DxxTreeSha256 -Path $pythonDir -ExpectedSha256 $config['PYTHON_EMBED_TREE_SHA256'] `
        -Label 'embedded Python' | Out-Null
    Assert-DxxTreeSha256 -Path $machfsDir -ExpectedSha256 $config['MACHFS_TREE_SHA256'] `
        -Label 'machfs' | Out-Null
}

if ((Test-Path -LiteralPath $installRoot -PathType Container) -and -not $Force) {
    Assert-InstalledOracleTools
    return $installRoot
}

$operationId = [Guid]::NewGuid().ToString('N')
$stagingRoot = Join-Path $depBase ".$($config['PYTHON_ORACLE_DIR_NAME'])-$operationId"
$backupRoot = Join-Path $depBase ".$($config['PYTHON_ORACLE_DIR_NAME'])-backup-$operationId"
$pythonArchive = Join-Path $depBase "python-$operationId.zip"
$machfsArchive = Join-Path $depBase "machfs-$operationId.whl"

try {
    Invoke-WebRequest -Uri $config['PYTHON_EMBED_URL'] -OutFile $pythonArchive -UseBasicParsing
    Assert-DxxFileSha256 -Path $pythonArchive -ExpectedSha256 $config['PYTHON_EMBED_SHA256'] `
        -Label 'embedded Python package' | Out-Null
    Invoke-WebRequest -Uri $config['MACHFS_URL'] -OutFile $machfsArchive -UseBasicParsing
    Assert-DxxFileSha256 -Path $machfsArchive -ExpectedSha256 $config['MACHFS_SHA256'] `
        -Label 'machfs wheel' | Out-Null

    $stagedPython = Join-Path $stagingRoot 'python'
    $stagedMachfs = Join-Path $stagingRoot 'machfs'
    New-Item -ItemType Directory -Path $stagedPython, $stagedMachfs | Out-Null
    Expand-Archive -LiteralPath $pythonArchive -DestinationPath $stagedPython
    Expand-Archive -LiteralPath $machfsArchive -DestinationPath $stagedMachfs
    Assert-DxxTreeSha256 -Path $stagedPython -ExpectedSha256 $config['PYTHON_EMBED_TREE_SHA256'] `
        -Label 'staged embedded Python' | Out-Null
    Assert-DxxTreeSha256 -Path $stagedMachfs -ExpectedSha256 $config['MACHFS_TREE_SHA256'] `
        -Label 'staged machfs' | Out-Null

    if (Test-Path -LiteralPath $installRoot) {
        Move-Item -LiteralPath $installRoot -Destination $backupRoot
    }
    try {
        Move-Item -LiteralPath $stagingRoot -Destination $installRoot
    } catch {
        if (Test-Path -LiteralPath $backupRoot) {
            Move-Item -LiteralPath $backupRoot -Destination $installRoot
        }
        throw
    }
    if (Test-Path -LiteralPath $backupRoot) {
        Remove-Item -LiteralPath $backupRoot -Recurse -Force
    }
} finally {
    Remove-Item -LiteralPath $pythonArchive -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $machfsArchive -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $stagingRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Assert-InstalledOracleTools
return $installRoot
