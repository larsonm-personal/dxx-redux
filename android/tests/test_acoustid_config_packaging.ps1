$ErrorActionPreference = "Stop"

. "$PSScriptRoot\..\helpers\test_env.ps1"

$androidRoot = Split-Path $PSScriptRoot -Parent
$gradle = Resolve-RegressionGradleWrapper -AndroidDir $androidRoot
$tempRoot = Join-Path $androidRoot "temp/acoustid_config_packaging"
$generatedAsset = Join-Path $androidRoot "app/build/generated/acoustid-assets/main/acoustid_config.jsonc"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

function Invoke-ConfigGeneration {
    param(
        [string]$ConfigPath,
        [bool]$ShouldSucceed
    )

    $phase = [IO.Path]::GetFileNameWithoutExtension($ConfigPath)
    $logPath = Join-Path $tempRoot "$phase.log"
    Write-Host "Generating AcoustID asset: $phase (log: $logPath)"
    $timer = [Diagnostics.Stopwatch]::StartNew()
    Push-Location $androidRoot
    try {
        & $gradle :app:generateAcoustIdConfigAsset --rerun-tasks --console=plain "-PacoustIdConfigFile=$ConfigPath" *> $logPath
        $succeeded = $LASTEXITCODE -eq 0
    } finally {
        Pop-Location
    }
    if ($succeeded -ne $ShouldSucceed) {
        throw "Unexpected AcoustID asset generation result for $ConfigPath (see $logPath)"
    }
    Write-Host "AcoustID asset phase $phase passed in $([int]$timer.Elapsed.TotalSeconds)s"
}

$validConfig = Join-Path $tempRoot "valid.jsonc"
$placeholderConfig = Join-Path $tempRoot "placeholder.jsonc"
$malformedConfig = Join-Path $tempRoot "malformed.jsonc"
$missingConfig = Join-Path $tempRoot "missing.jsonc"

$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($validConfig, @'
{
    // Test-only key
    "api_key": "AbCd123456",
}
'@, $utf8NoBom)
[IO.File]::WriteAllText($placeholderConfig, @'
{"api_key": "YOUR_ACOUSTID_API_KEY_HERE"}
'@, $utf8NoBom)
[IO.File]::WriteAllText($malformedConfig, @'
{"api_key":
'@, $utf8NoBom)

Invoke-ConfigGeneration -ConfigPath $validConfig -ShouldSucceed $true
$generated = Get-Content $generatedAsset -Raw | ConvertFrom-Json
if ($generated.api_key -cne "AbCd123456") {
    throw "Generated asset did not preserve the configured key"
}

Invoke-ConfigGeneration -ConfigPath $placeholderConfig -ShouldSucceed $false
Invoke-ConfigGeneration -ConfigPath $malformedConfig -ShouldSucceed $false
Invoke-ConfigGeneration -ConfigPath $missingConfig -ShouldSucceed $true
if (Test-Path $generatedAsset) {
    throw "Unconfigured generation left a stale AcoustID asset"
}

$maintainedConfig = Join-Path $androidRoot "acoustid_config.jsonc"
Invoke-ConfigGeneration -ConfigPath $maintainedConfig -ShouldSucceed $true

Write-Host "AcoustID configuration packaging tests passed"
