#!/usr/bin/env pwsh

function Select-InputDemoReplayEngine {
    param([scriptblock]$ReadChoice = { Read-Host 'Choose engine [1]' })

    Write-Host ''
    Write-Host 'Replay a demo' -ForegroundColor Cyan
    Write-Host '  1. D1'
    Write-Host '  2. D2'
    Write-Host '  3. D1-in-D2 (D1 recording in the D2 engine)'
    Write-Host '  Q. Cancel'
    while ($true) {
        switch (([string](& $ReadChoice)).Trim().ToLowerInvariant()) {
            '' { return 'd1' }
            '1' { return 'd1' }
            '2' { return 'd2' }
            '3' { return 'd1-in-d2' }
            'q' { return 'cancel' }
            default { Write-Host 'Enter 1, 2, 3, or Q' -ForegroundColor Yellow }
        }
    }
}

function Select-InputDemoReplayDisplay {
    param([scriptblock]$ReadChoice = { Read-Host 'Choose playback [1]' })

    Write-Host ''
    Write-Host 'Playback:' -ForegroundColor Cyan
    Write-Host '  1. Headed - watch in a window at normal speed'
    Write-Host '  2. Headless - accelerated, without rendering'
    Write-Host '     D1 and D1-in-D2 use the no-present windowed runner'
    Write-Host '  Q. Cancel'
    while ($true) {
        switch (([string](& $ReadChoice)).Trim().ToLowerInvariant()) {
            '' { return 'headed' }
            '1' { return 'headed' }
            '2' { return 'headless' }
            'q' { return 'cancel' }
            default { Write-Host 'Enter 1, 2, or Q' -ForegroundColor Yellow }
        }
    }
}
