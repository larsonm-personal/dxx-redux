#!/usr/bin/env pwsh

function Test-RunAllTestsProfileMenuEnabled {
    param(
        [Parameter(Mandatory)][int]$ExplicitParameterCount,
        [Parameter(Mandatory)][bool]$UserInteractive,
        [Parameter(Mandatory)][bool]$InputRedirected
    )

    return $ExplicitParameterCount -eq 0 -and $UserInteractive -and -not $InputRedirected
}

function Select-RunAllTestsProfile {
    param([scriptblock]$ReadChoice = { Read-Host 'Choose a test profile' })

    Write-Host ''
    Write-Host 'DXX-Redux test suite' -ForegroundColor Cyan
    Write-Host '  1. Run the full unattended test suite'
    Write-Host '  T. Resumable hash-ring sample targeting 45 minutes'
    Write-Host '  B. Regenerate level metadata benchmark history'
    Write-Host '  Q. Cancel'
    while ($true) {
        switch ((& $ReadChoice).Trim().ToLowerInvariant()) {
            '1' { return 'Full' }
            'all' { return 'Full' }
            'full' { return 'Full' }
            't' { return 'Target45' }
            'target' { return 'Target45' }
            'b' { return 'LevelMetadataBenchmark' }
            'benchmark' { return 'LevelMetadataBenchmark' }
            'q' { return 'Cancel' }
            'quit' { return 'Cancel' }
            default { Write-Host 'Enter 1, T, B, or Q' -ForegroundColor Yellow }
        }
    }
}
