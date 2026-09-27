# Shared process inventory for cleanup and host tooling
if (-not (Get-Command Get-DxxHostProcessInventory -ErrorAction SilentlyContinue)) {
    function Get-DxxHostProcessInventory {
        param([switch]$IncludePaths)
        if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
            foreach ($process in Get-CimInstance Win32_Process -ErrorAction Stop) {
                $process | Add-Member -NotePropertyName PathInspectionFailed -NotePropertyValue ($IncludePaths -and $process.CommandLine -and -not $process.ExecutablePath)
                $process
            }
            return
        }
        if (-not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
            throw 'Process inventory is not implemented on this host'
        }
        # Read procfs directly so command lines with spaces retain their boundaries
        foreach ($entry in Get-ChildItem -LiteralPath /proc -Directory -ErrorAction Stop) {
            if ($entry.Name -notmatch '^\d+$') { continue }
            try {
                $stat = [IO.File]::ReadAllText((Join-Path $entry.FullName 'stat'))
                $commandLine = [IO.File]::ReadAllText((Join-Path $entry.FullName 'cmdline')).Replace([char]0, ' ').Trim()
                if ($stat -notmatch '^\d+ \((.*)\) \S+ (\d+) ') {
                    throw "Cannot parse process status for $($entry.Name)"
                }
                $processName = $Matches[1]
                $parentId = [int]$Matches[2]
                $executablePath = $null
                $workingDirectory = $null
                $pathInspectionFailed = $false
                if ($IncludePaths -and $commandLine) {
                    try {
                        $executablePath = ([IO.FileInfo]::new((Join-Path $entry.FullName 'exe'))).LinkTarget
                        $workingDirectory = ([IO.DirectoryInfo]::new((Join-Path $entry.FullName 'cwd'))).LinkTarget
                        $pathInspectionFailed = -not $executablePath -or -not $workingDirectory
                    } catch { $pathInspectionFailed = $true }
                }
                [pscustomobject]@{
                    ProcessId = [int]$entry.Name
                    ParentProcessId = $parentId
                    Name = $processName
                    CommandLine = $commandLine
                    ExecutablePath = $executablePath
                    WorkingDirectory = $workingDirectory
                    PathInspectionFailed = $pathInspectionFailed
                }
            } catch {
                # A process exiting between enumeration and reading is harmless
                if (Test-Path -LiteralPath $entry.FullName) { throw }
            }
        }
    }
}
