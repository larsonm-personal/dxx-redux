#!/usr/bin/env pwsh
# Read-only inventory; unreferenced packages are not authorization to delete them
[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path (Split-Path $PSScriptRoot)),
    [string[]]$AvdRoots
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/sdk_package_inventory.ps1')
Get-DxxSdkInventory -RepoRoot $RepoRoot -AvdRoots $AvdRoots
