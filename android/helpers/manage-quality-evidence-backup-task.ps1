param(
    [ValidateSet("Menu", "Check", "StopDisable", "EnableStart")]
    [string]$Action = "Menu",
    [string]$TaskName = "DxxReduxEvidenceLedgerBackup"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-BackupTask {
    $matches = @(
        Get-ScheduledTask -ErrorAction Stop |
            Where-Object { $_.TaskName -eq $TaskName }
    )

    if ($matches.Count -gt 1) {
        $paths = ($matches | ForEach-Object { "$($_.TaskPath)$($_.TaskName)" }) -join ", "
        throw "More than one scheduled task named '$TaskName' was found: $paths"
    }

    if ($matches.Count -eq 0) {
        return $null
    }

    return $matches[0]
}

function Show-BackupTask {
    param([Microsoft.Management.Infrastructure.CimInstance]$Task)

    if ($null -eq $Task) {
        Write-Host "Scheduled task '$TaskName' is not present"
        return
    }

    $info = Get-ScheduledTaskInfo -InputObject $Task
    [pscustomobject]@{
        TaskName       = $Task.TaskName
        TaskPath       = $Task.TaskPath
        State          = $Task.State
        Enabled        = $Task.State -ne "Disabled"
        LastRunTime    = $info.LastRunTime
        LastTaskResult = $info.LastTaskResult
        NextRunTime    = $info.NextRunTime
    } | Format-List
}

if ($Action -eq "Menu") {
    Write-Host "Manage scheduled task '$TaskName'"
    Write-Host "  1. Check for the task"
    Write-Host "  2. Stop and disable the task"
    Write-Host "  3. Enable and start the task"
    Write-Host "  Q. Quit"

    switch ((Read-Host "Select an option").Trim().ToUpperInvariant()) {
        "1" { $Action = "Check" }
        "2" { $Action = "StopDisable" }
        "3" { $Action = "EnableStart" }
        "Q" { return }
        default { throw "Invalid selection" }
    }
}

$task = Get-BackupTask

switch ($Action) {
    "Check" {
        Show-BackupTask -Task $task
    }
    "StopDisable" {
        if ($null -eq $task) {
            throw "Scheduled task '$TaskName' is not present"
        }

        # Disable first so a trigger cannot start another instance while this runs.
        Disable-ScheduledTask -InputObject $task | Out-Null
        Stop-ScheduledTask -InputObject $task
        Write-Host "Stopped and disabled scheduled task '$TaskName'"
        Show-BackupTask -Task (Get-BackupTask)
    }
    "EnableStart" {
        if ($null -eq $task) {
            throw "Scheduled task '$TaskName' is not present"
        }

        Enable-ScheduledTask -InputObject $task | Out-Null
        $task = Get-BackupTask
        if ($task.State -ne "Running") {
            Start-ScheduledTask -InputObject $task
        }
        Write-Host "Enabled and started scheduled task '$TaskName'"
        Show-BackupTask -Task (Get-BackupTask)
    }
}
