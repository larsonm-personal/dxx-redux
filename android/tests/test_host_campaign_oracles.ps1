#!/usr/bin/env pwsh
# Offline stdlib controls only; no game process, display, assets or device required
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_host_platform.ps1')
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for host campaign oracle controls' }
$prefix = @($python.PrefixArguments)
& $python.Path @prefix -S -c 'import sys; sys.exit("Python 3.9 or newer is required") if sys.version_info < (3, 9) else None; sys.exit("Disable Python optimization for oracle controls") if not __debug__ else None'
if ($LASTEXITCODE -ne 0) { throw 'Host campaign controls require Python 3.9+ and enabled assertions' }

# Keep this allowlist explicit: neighboring scripts include opt-in game integrations
$controls = @(
    'test_host_config_campaign.py',
    'test_host_gameplay_campaign.py',
    'test_host_music_campaign.py',
    'test_host_render_campaign.py'
)
foreach ($control in $controls) {
    & $python.Path @prefix -S (Join-Path $PSScriptRoot $control)
    if ($LASTEXITCODE -ne 0) { throw "Host campaign oracle control failed: $control" }
}
Write-Host 'PASS: four explicit offline host campaign control suites'
