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
            [System.IO.File]::Replace($temporary, $fullPath, $backup)
        } else {
            [System.IO.File]::Move($temporary, $fullPath)
        }
    } finally {
        Remove-Item -LiteralPath $temporary, $backup -Force -ErrorAction SilentlyContinue
    }
}
