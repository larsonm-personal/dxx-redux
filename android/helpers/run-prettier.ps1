#!/usr/bin/env pwsh
param([switch]$Check, [string[]]$Paths)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'code-quality-files.ps1')
$Paths = @(Get-CodeQualityScriptPaths -InputPaths $Paths -RemainingPaths @($args) -ExplicitScope ($PSBoundParameters.ContainsKey('Paths')))
$files = @(Get-CodeQualityScopedFiles -RepoRoot $repoRoot -RootPath $repoRoot -InputPaths $Paths -ValidExtensions @('.json', '.jsonc', '.yaml', '.yml', '.md', '.xml', '.svg', '.html', '.js', '.mjs', '.ts', '.css') |
        Where-Object { $_.FullName -notmatch '[\\/]game_data[\\/].*\.json$' })
if (-not $files.Count) { Write-Host 'No structured text files in scope'; exit 0 }
$toolRoot = Join-Path $repoRoot 'android/tools/code-quality'
if (-not (Test-Path -LiteralPath (Join-Path $toolRoot 'node_modules/prettier/package.json'))) {
    throw 'Prettier is missing; run android/run-code-quality.ps1 -InstallTools with Node.js/npm in PATH'
}
$manifest = Join-Path $repoRoot ('android/temp/prettier-files-' + [guid]::NewGuid().ToString('N') + '.json')
New-Item -ItemType Directory -Path (Split-Path $manifest) -Force | Out-Null
try {
    [IO.File]::WriteAllText($manifest, (ConvertTo-Json -InputObject @($files.FullName)), [Text.UTF8Encoding]::new($false))
    $mode = if ($Check) { 'check' } else { 'fix' }
    $node = Get-CodeQualityNode -RepoRoot $repoRoot
    & $node (Join-Path $toolRoot 'format-text.mjs') $manifest $mode
    $result = $LASTEXITCODE
} finally {
    Remove-Item -LiteralPath $manifest -Force
}
exit $result
