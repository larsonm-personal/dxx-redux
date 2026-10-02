#!/usr/bin/env pwsh
# Own the fixture, trigger and restart scripts that require provisioned fault state
[CmdletBinding()]
param(
    [switch]$Install,
    [ValidateSet('d1', 'd2')][string]$Game,
    [ValidateSet('stall', 'record_unreadable', 'publication_blocked', 'rebuild_failure', 'menu_abandoned', 'accept_publication_blocked', 'normal_exit', 'repair_interrupted')][string]$Fault,
    [string]$Serial = 'emulator-5554'
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot '../helpers/run_graphics_recovery_tests.ps1') @PSBoundParameters
exit 0
