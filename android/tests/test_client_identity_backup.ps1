$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repoRoot 'android\app\src\main\AndroidManifest.xml'
$legacyPath = Join-Path $repoRoot 'android\app\src\main\res\xml\backup_rules.xml'
$modernPath = Join-Path $repoRoot 'android\app\src\main\res\xml\data_extraction_rules.xml'
$identityPath = Join-Path $repoRoot 'android\app\src\main\java\com\dxxredux\app\multiplayer\ClientIdentity.kt'

[xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
$androidNamespace = 'http://schemas.android.com/apk/res/android'
$application = $manifest.manifest.application
if ($application.GetAttribute('fullBackupContent', $androidNamespace) -ne '@xml/backup_rules') {
    throw 'Manifest does not reference the legacy backup rules'
}
if ($application.GetAttribute('dataExtractionRules', $androidNamespace) -ne '@xml/data_extraction_rules') {
    throw 'Manifest does not reference the Android 12 backup rules'
}

[xml]$legacy = Get-Content -LiteralPath $legacyPath -Raw
$legacyExclusion = @($legacy.'full-backup-content'.exclude) | Where-Object {
    $_.domain -eq 'sharedpref' -and $_.path -eq 'client_identity.xml'
}
if ($legacyExclusion.Count -ne 1) {
    throw 'Legacy backup rules must exclude client_identity.xml exactly once'
}

[xml]$modern = Get-Content -LiteralPath $modernPath -Raw
foreach ($sectionName in @('cloud-backup', 'device-transfer')) {
    $section = $modern.'data-extraction-rules'.$sectionName
    $exclusion = @($section.exclude) | Where-Object {
        $_.domain -eq 'sharedpref' -and $_.path -eq 'client_identity.xml'
    }
    if ($exclusion.Count -ne 1) {
        throw "$sectionName rules must exclude client_identity.xml exactly once"
    }
}

$identitySource = Get-Content -LiteralPath $identityPath -Raw
if ($identitySource -notmatch 'PREFS_NAME\s*=\s*"client_identity"') {
    throw 'ClientIdentity preference name no longer matches the backup exclusion'
}
if ($identitySource -notmatch '\.commit\(\)') {
    throw 'Installation identity persistence is not synchronous'
}
if ($identitySource -notmatch '@Synchronized\s+fun getInstallationId') {
    throw 'Installation identity generation is not serialized'
}

Write-Host 'Client identity backup policy tests passed'
