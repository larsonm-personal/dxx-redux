# Run the disc publication and content ownership integration tests on the host JVM
param(
    [string]$AnniversaryExtracted,
    [switch]$Device,
    [string]$Serial = 'emulator-5554'
)
$ErrorActionPreference = 'Stop'
if ($AnniversaryExtracted) {
    $fixture = (Resolve-Path -LiteralPath $AnniversaryExtracted).Path
    if (-not (Test-Path -LiteralPath (Join-Path $fixture 'newlevel/mad.rdl'))) {
        throw 'Anniversary fixture must be native extraction output containing newlevel/mad.rdl'
    }
    $env:DXX_DISC_CONTENT_FIXTURE = $fixture
}
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if ($IsWindows) {
    $env:JAVA_HOME = 'C:\local\jdk-21'
    $env:Path = "$env:JAVA_HOME\bin;$env:Path"
}
$gradle = Join-Path $repoRoot $(if ($IsWindows) { 'android/gradlew.bat' } else { 'android/gradlew' })
& $gradle -p (Join-Path $repoRoot 'android') :app:testDebugUnitTest `
    --tests com.dxxredux.app.DiscContentImportTest `
    --tests com.dxxredux.app.CueDataTrackExtractionTest `
    --tests com.dxxredux.app.DiscImportHoistTest `
    --tests com.dxxredux.app.AndroidGameFileExtensionsTest `
    --tests com.dxxredux.app.GogAudioSourceRegistrationTest `
    --tests com.dxxredux.app.FileSetContentCatalogTest `
    --tests com.dxxredux.app.FileSetContentManagerTest --console=plain
if ($LASTEXITCODE -ne 0) { throw 'Disc content import tests failed' }
if ($Device) {
    $iso = Join-Path $repoRoot 'game_data/CD images/Descent Anniversary (ISO)/descent_anniversary.iso'
    if (-not (Test-Path -LiteralPath $iso)) { throw 'Anniversary ISO is required for the device test' }
    $env:ANDROID_SERIAL = $Serial
    $adb = if ($IsWindows) { 'C:/local/android-sdk/platform-tools/adb.exe' } else { 'adb' }
    & $adb -s $Serial push $iso /data/local/tmp/descent_anniversary.iso
    if ($LASTEXITCODE -ne 0) { throw 'Could not stage Anniversary ISO' }
    & $adb -s $Serial logcat -c
    & (Join-Path $repoRoot 'android/helpers/run_test.ps1') -ScriptName test_anniversary_content_import.jsonc -Game d1
    if ($LASTEXITCODE -ne 0) { throw 'Anniversary device import test failed' }
}
