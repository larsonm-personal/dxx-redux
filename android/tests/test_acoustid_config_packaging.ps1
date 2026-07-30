$ErrorActionPreference = "Stop"

$androidRoot = Split-Path $PSScriptRoot -Parent
$repoRoot = Split-Path $androidRoot -Parent
$gradle = Join-Path $androidRoot "gradlew.bat"
$tempRoot = Join-Path $androidRoot "temp/acoustid_config_packaging"
$generatedAsset = Join-Path $androidRoot "app/build/generated/acoustid-assets/main/acoustid_config.json5"

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

function Invoke-ConfigGeneration {
    param(
        [string]$ConfigPath,
        [bool]$ShouldSucceed
    )

    Push-Location $androidRoot
    try {
        & $gradle :app:generateAcoustIdConfigAsset --rerun-tasks "-PacoustIdConfigFile=$ConfigPath" *> $null
        $succeeded = $LASTEXITCODE -eq 0
    } finally {
        Pop-Location
    }
    if ($succeeded -ne $ShouldSucceed) {
        throw "Unexpected AcoustID asset generation result for $ConfigPath"
    }
}

$validConfig = Join-Path $tempRoot "valid.json5"
$placeholderConfig = Join-Path $tempRoot "placeholder.json5"
$malformedConfig = Join-Path $tempRoot "malformed.json5"
$missingConfig = Join-Path $tempRoot "missing.json5"

Set-Content -Path $validConfig -Encoding utf8NoBOM -Value @'
{
    // Test-only key
    "api_key": "AbCd123456",
}
'@
Set-Content -Path $placeholderConfig -Encoding utf8NoBOM -Value @'
{"api_key": "YOUR_ACOUSTID_API_KEY_HERE"}
'@
Set-Content -Path $malformedConfig -Encoding utf8NoBOM -Value @'
{"api_key":
'@

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

$maintainedConfig = Join-Path $androidRoot "acoustid_config.json5"
Invoke-ConfigGeneration -ConfigPath $maintainedConfig -ShouldSucceed $true

Write-Host "AcoustID configuration packaging tests passed"
