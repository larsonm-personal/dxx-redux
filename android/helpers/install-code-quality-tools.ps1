#!/usr/bin/env pwsh
# Install additional pinned formatters; the existing tools retain their get_deps installers
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'code-quality-files.ps1')
$ruffVersion = Get-CodeQualityToolVersion -RepoRoot $repoRoot -Name 'RUFF_VERSION'
$toolchain = Get-CodeQualityToolVersion -RepoRoot $repoRoot -Name 'RUSTFMT_TOOLCHAIN'
$python = Get-CodeQualityPython
$node = Get-CodeQualityNode -RepoRoot $repoRoot
& $python -m pip install --disable-pip-version-check --upgrade --target (Join-Path $repoRoot 'android/tools/code-quality/python') "ruff==$ruffVersion"
if ($LASTEXITCODE -ne 0) { exit 1 }
$npmArguments = @('ci', '--prefix', (Join-Path $repoRoot 'android/tools/code-quality'), '--ignore-scripts', '--no-audit', '--no-fund')
$npmCli = Join-Path (Split-Path $node) 'node_modules/npm/bin/npm-cli.js'
if (Test-Path -LiteralPath $npmCli -PathType Leaf) {
    & $node $npmCli @npmArguments
} else {
    & npm @npmArguments
}
if ($LASTEXITCODE -ne 0) { exit 1 }
[IO.File]::WriteAllText((Join-Path $repoRoot 'android/tools/code-quality/node-path.txt'), $node, [Text.UTF8Encoding]::new($false))
& rustup toolchain install $toolchain --profile minimal --component rustfmt
exit $LASTEXITCODE
