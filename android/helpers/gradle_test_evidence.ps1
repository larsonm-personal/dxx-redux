# Validate the real Gradle XML, independently of Gradle's successful exit status
function Assert-GradleUnitTestEvidence {
    param([Parameter(Mandatory)][string]$ResultsDirectory)

    $files = @(Get-ChildItem -LiteralPath $ResultsDirectory -Filter 'TEST-*.xml' -File -ErrorAction SilentlyContinue)
    if ($files.Count -eq 0) { throw "No Gradle test XML in $ResultsDirectory" }
    $executed = 0
    $skipped = 0
    foreach ($file in $files) {
        [xml]$report = Get-Content -LiteralPath $file.FullName -Raw
        $suite = $report.DocumentElement
        if ($suite.LocalName -cne 'testsuite') { throw "Invalid Gradle test XML: $($file.FullName)" }
        $cases = @($suite.SelectNodes('testcase'))
        foreach ($name in @('tests', 'failures', 'errors', 'skipped')) {
            $value = $suite.GetAttribute($name)
            if ($value -notmatch '^\d+$') { throw "Missing or invalid $name in $($file.FullName)" }
        }
        if ([long]$suite.GetAttribute('tests') -ne $cases.Count) { throw "Test count mismatch in $($file.FullName)" }
        if ([long]$suite.GetAttribute('failures') -ne 0 -or [long]$suite.GetAttribute('errors') -ne 0 -or
            $suite.SelectNodes('testcase/failure | testcase/error').Count -ne 0) {
            throw "Failed Gradle tests in $($file.FullName)"
        }
        $suiteSkipped = $suite.SelectNodes('testcase[skipped]').Count
        if ([long]$suite.GetAttribute('skipped') -ne $suiteSkipped) { throw "Skip count mismatch in $($file.FullName)" }
        $skipped += $suiteSkipped
        $executed += $cases.Count - $suiteSkipped
    }
    if ($executed -le 0) { throw "No executed Gradle tests in $ResultsDirectory" }
    Write-Host "Gradle evidence: $executed executed, $skipped skipped in $ResultsDirectory"
}
