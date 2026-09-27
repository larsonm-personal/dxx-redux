param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$ExpectedSha256)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../../helpers/verified_dependencies.ps1')
Assert-DxxTreeSha256 -Path $Path -ExpectedSha256 $ExpectedSha256 | Out-Null
