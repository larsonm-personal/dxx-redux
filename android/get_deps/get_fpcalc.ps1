# get_fpcalc.ps1 -- Download fpcalc (chromaprint CLI) if not present.
# Returns the path to fpcalc.exe. Uses $DEP_BASE from dependency_base.txt.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot "Get-DepPlatform.ps1")
$depBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing

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
$plainVersion = if ($dirName -match '^fpcalc-(.+)$') { $Matches[1] } else { ($conf["CHROMAPRINT_VERSION"] -replace '^v', '') }
$hostPlatform = Get-HostPlatform
$fpcalcName = Get-PlatformExecutableName -ToolName "fpcalc"
$fpcalc = Join-Path $installDir $fpcalcName

if ($hostPlatform -eq "Linux") {
    $url = "https://github.com/acoustid/chromaprint/releases/download/v$plainVersion/chromaprint-fpcalc-$plainVersion-linux-x86_64.tar.gz"
} elseif ($hostPlatform -eq "MacOS") {
    $url = "https://github.com/acoustid/chromaprint/releases/download/v$plainVersion/chromaprint-fpcalc-$plainVersion-macos-x86_64.tar.gz"
}

if ((Test-Path $fpcalc) -and -not $Force) {
    Write-Host "$fpcalcName already present: $fpcalc"
    return $fpcalc
}

Write-Host "Downloading fpcalc from $url"

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

$tmpArchive = Join-Path $installDir "fpcalc-download.tmp"
try {
    Invoke-WebRequest -Uri $url -OutFile $tmpArchive -UseBasicParsing
    if ($hostPlatform -eq "Windows") {
        Expand-Archive -Path $tmpArchive -DestinationPath $installDir -Force
    } else {
        tar -xzf $tmpArchive -C $installDir
    }
    $nested = Get-ChildItem -Path $installDir -Recurse -File | Where-Object { $_.Name -eq $fpcalcName } | Select-Object -First 1
    if ($nested -and ($nested.DirectoryName -ne $installDir)) {
        Move-Item -Path $nested.FullName -Destination $fpcalc -Force
    }
} finally {
    Remove-Item $tmpArchive -ErrorAction SilentlyContinue
}

if (-not (Test-Path $fpcalc)) {
    Write-Error "$fpcalcName not found after extraction at: $fpcalc"
}

Write-Host "$fpcalcName installed: $fpcalc"
return $fpcalc
