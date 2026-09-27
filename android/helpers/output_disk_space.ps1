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
        $comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
            [StringComparison]::OrdinalIgnoreCase
        } else { [StringComparison]::Ordinal }
        # Unix mounts can be below /; check the volume that actually holds output
        $drive = [IO.DriveInfo]::GetDrives() | Where-Object {
            $mount = $_.Name.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
            $full.Equals($_.Name, $comparison) -or $full.StartsWith($mount, $comparison)
        } | Sort-Object { $_.Name.Length } -Descending | Select-Object -First 1
        if (-not $drive) { throw "Cannot identify output volume for $full" }
        $root = $drive.Name
        if ($checked.ContainsKey($root)) { continue }
        $checked[$root] = $true
        $free = $drive.AvailableFreeSpace
        if ($free -lt $MinimumFreeGB * 1GB) {
            throw ("Insufficient output disk space on {0}: {1:n2} GiB free, {2:n2} GiB reserve required. Artifact production stopped; evidence is incomplete. Run android/clean-workspace.ps1 when jobs are idle or select another output volume" -f $root, ($free / 1GB), $MinimumFreeGB)
        }
    }
}
