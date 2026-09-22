# Shared output-volume reserve for replay producers; checks do not delete files

function Assert-OutputDiskSpace {
    param(
        [Parameter(Mandatory)][string[]]$Paths,
        [ValidateRange(0.01, 1048576)][double]$MinimumFreeGB = 4
    )

    $checked = @{}
    foreach ($path in $Paths) {
        if (-not $path) { continue }
        $full = [IO.Path]::GetFullPath($path)
        $root = [IO.Path]::GetPathRoot($full)
        if ($checked.ContainsKey($root)) { continue }
        $checked[$root] = $true
        $drive = [IO.DriveInfo]::new($root)
        $free = $drive.AvailableFreeSpace
        if ($free -lt $MinimumFreeGB * 1GB) {
            throw ("Insufficient output disk space on {0}: {1:n2} GiB free, {2:n2} GiB reserve required. Replay stopped; evidence is incomplete. Run android/clean-workspace.ps1 when jobs are idle or select another output volume" -f $root, ($free / 1GB), $MinimumFreeGB)
        }
    }
}
