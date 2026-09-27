function Write-Utf8NoBomTextAtomically {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $parent = [System.IO.Path]::GetDirectoryName($fullPath)
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $suffix = [Guid]::NewGuid().ToString('N')
    $temporary = Join-Path $parent ".$([System.IO.Path]::GetFileName($fullPath)).$suffix.tmp"
    $backup = Join-Path $parent ".$([System.IO.Path]::GetFileName($fullPath)).$suffix.bak"
    try {
        [System.IO.File]::WriteAllText($temporary, $Text, [System.Text.UTF8Encoding]::new($false))
        if (Test-Path -LiteralPath $fullPath -PathType Leaf) {
            $deadline = [DateTime]::UtcNow.AddSeconds(3)
            while ($true) {
                try {
                    [System.IO.File]::Replace($temporary, $fullPath, $backup)
                    break
                } catch {
                    $cause = $_.Exception.GetBaseException()
                    $nativeError = $cause.HResult -band 0xffff
                    # Windows readers/scanners can briefly deny replacement of the old file
                    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or
                        $nativeError -notin @(32, 33, 1175) -or [DateTime]::UtcNow -ge $deadline) { throw }
                    Start-Sleep -Milliseconds 50
                }
            }
        } else {
            [System.IO.File]::Move($temporary, $fullPath)
        }
    } finally {
        foreach ($scratch in @($temporary, $backup)) {
            Remove-Item -LiteralPath $scratch -Force -ErrorAction SilentlyContinue
        }
    }
}
