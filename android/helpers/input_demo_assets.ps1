# One asset preflight implementation for all host replay runners
function Invoke-InputDemoAssetCheck {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][ValidateSet('inspect', 'verify', 'select-shareware')][string]$Command,
        [Parameter(Mandatory)][string[]]$Paths,
        [string]$DataDir,
        [ValidateSet('all', 'd1', 'd2')][string]$Game = 'all',
        [switch]$D1InD2,
        [switch]$AllowUnverifiedAssets
    )
    $python = Resolve-RegressionPythonCommand
    if (-not $python) { throw 'Python 3 is required for input-demo asset verification' }
    $prefix = @($python.PrefixArguments)
    $arguments = @((Join-Path $PSScriptRoot '../tests/input_demo_assets.py'), $Command) + $Paths + @('--repo', $RepoRoot, '--game', $Game)
    if ($DataDir) { $arguments += @('--data', $DataDir) }
    if ($D1InD2) { $arguments += '--imported' }
    if ($AllowUnverifiedAssets) { $arguments += '--allow-unverified-assets' }
    $output = & $python.Path @prefix @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Input-demo asset preflight failed before playback' }
    return ($output | ConvertFrom-Json)
}
