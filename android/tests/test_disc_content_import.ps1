# Run the disc publication and content ownership integration tests on the host JVM
$ErrorActionPreference = 'Stop'
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
    --tests com.dxxredux.app.FileSetContentCatalogTest `
    --tests com.dxxredux.app.FileSetContentManagerTest --console=plain
if ($LASTEXITCODE -ne 0) { throw 'Disc content import tests failed' }
