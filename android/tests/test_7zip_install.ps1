#!/usr/bin/env pwsh
# Exercise the real pinned installer and executable, including failure rollback
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$installer = Join-Path $repoRoot 'android/get_deps/helpers/get_7zip.ps1'
$tool = & $installer
$originalHash = (Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash
$root = Join-Path $repoRoot ('android/temp/sevenzip-install-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    $source = Join-Path $root 'payload with spaces.txt'
    $archive = Join-Path $root 'roundtrip.7z'
    $output = Join-Path $root 'extracted'
    [IO.File]::WriteAllText($source, 'verified cross-platform extraction')
    & $tool a -y $archive $source | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Pinned 7-Zip could not create the fixture archive' }
    & $tool x -y "-o$output" $archive | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Pinned 7-Zip could not extract the fixture archive' }
    if ([IO.File]::ReadAllText((Join-Path $output 'payload with spaces.txt')) -cne [IO.File]::ReadAllText($source)) {
        throw 'Pinned 7-Zip round trip changed the payload'
    }

    $depBase = Split-Path (Split-Path $tool)
    $before = @(Get-ChildItem -LiteralPath $depBase -Force | ForEach-Object Name)
    function Invoke-WebRequest {
        param($Uri, $OutFile, [switch]$UseBasicParsing)
        [IO.File]::WriteAllText($OutFile, 'corrupt downloaded archive')
    }
    # Cached reuse must still succeed when downloads would fail verification
    if ((& $installer) -ne $tool) { throw 'Cached executable was not reused' }
    $rejected = $false
    try { & $installer -Force | Out-Null } catch {
        if ($_.Exception.Message -notmatch 'SHA-256 mismatch') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Corrupt replacement was published' }
    if ((Get-FileHash -LiteralPath $tool -Algorithm SHA256).Hash -ne $originalHash) {
        throw 'Failed installation damaged the working executable'
    }
    $lockName = '.' + (Split-Path (Split-Path $tool) -Leaf) + '.install.lock'
    $newFiles = @(Get-ChildItem -LiteralPath $depBase -Force | Where-Object { $_.Name -notin $before -and $_.Name -ne $lockName })
    if ($newFiles.Count) { throw "Failed installer left temporary files: $($newFiles.Name -join ', ')" }
    Write-Host 'PASS: pinned 7-Zip install, round trip, cached verification, failure preservation, and cleanup'
} finally {
    Remove-Item Function:Invoke-WebRequest -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
