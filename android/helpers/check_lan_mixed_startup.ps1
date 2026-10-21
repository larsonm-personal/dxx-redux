# Actual pre-admission APK versus candidate, in both host/client directions
function Invoke-LanMixedStartupChecks {
    if (-not $env:DXX_COMPAT_APK -or -not (Test-Path -LiteralPath $env:DXX_COMPAT_APK)) { throw 'Actual compatibility APK unavailable' }
    $candidate = Get-AndroidTestApkPath
    if (-not $candidate) { throw 'Candidate APK unavailable' }
    [ordered]@{ old_revision = '01401425aff549eefd8b5a9e9fd8a8458d01a9b8'; old_sha256 = (Get-FileHash $env:DXX_COMPAT_APK -Algorithm SHA256).Hash
        candidate_revision = (git -C $script:REPO_ROOT rev-parse HEAD); candidate_sha256 = (Get-FileHash $candidate -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content (Join-Path $outputDirectory 'mixed-binary-provenance.json') -Encoding utf8
    foreach ($oldPeer in @($hostSerial, $clientSerial)) {
        try {
            foreach ($serial in @($hostSerial, $clientSerial)) {
                $apk = if ($serial -eq $oldPeer) { $env:DXX_COMPAT_APK } else { $candidate }
                Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                $installed = Adb-Dev-Timeout -Serial $serial -AdbArgs @('install', '-r', '-d', $apk) -Seconds 180
                if ($installed -notmatch 'Success') { throw "Mixed binary install failed: $serial" }
            }
            $case = if ($oldPeer -eq $hostSerial) { 'old-host' } else { 'old-client' }
            Invoke-LanParallelStartupChecks -Cases @($case)
        } finally {
            foreach ($serial in @($hostSerial, $clientSerial)) {
                $installed = Adb-Dev-Timeout -Serial $serial -AdbArgs @('install', '-r', '-d', $candidate) -Seconds 180
                if ($installed -notmatch 'Success') { throw "Candidate restore failed: $serial" }
            }
        }
    }
}
