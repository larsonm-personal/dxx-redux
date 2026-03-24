# get_fpcalc.ps1 -- Download fpcalc (chromaprint CLI) if not present.
# Returns the path to fpcalc.exe. Uses $DEP_BASE from dependency_base.txt.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$depBase = (Get-Content (Join-Path $repoRoot "dependency_base.txt") -First 1).Trim()

# Parse version info from tool_versions.conf
$conf = @{}
Get-Content "$PSScriptRoot/tool_versions.conf" | ForEach-Object {
    if ($_ -match '^([A-Z_]+)=(.+)$') {
        $conf[$Matches[1]] = $Matches[2]
    }
}

$url = $conf["FPCALC_URL"]
$dirName = $conf["FPCALC_DIR_NAME"]
$installDir = Join-Path $depBase $dirName
$fpcalc = Join-Path $installDir "fpcalc.exe"

if ((Test-Path $fpcalc) -and -not $Force) {
    Write-Host "fpcalc.exe already present: $fpcalc"
    return $fpcalc
}

Write-Host "Downloading fpcalc from $url"

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

$tmpZip = Join-Path $installDir "fpcalc-download.zip"
try {
    Invoke-WebRequest -Uri $url -OutFile $tmpZip -UseBasicParsing
    Expand-Archive -Path $tmpZip -DestinationPath $installDir -Force
    # The zip extracts into a subfolder; find fpcalc.exe and move up
    $nested = Get-ChildItem -Path $installDir -Recurse -Filter "fpcalc.exe" | Select-Object -First 1
    if ($nested -and ($nested.DirectoryName -ne $installDir)) {
        Move-Item -Path $nested.FullName -Destination $fpcalc -Force
    }
} finally {
    Remove-Item $tmpZip -ErrorAction SilentlyContinue
}

if (-not (Test-Path $fpcalc)) {
    Write-Error "fpcalc.exe not found after extraction at: $fpcalc"
}

Write-Host "fpcalc.exe installed: $fpcalc"
return $fpcalc
