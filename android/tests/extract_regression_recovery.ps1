function Test-ExtractRegressionAdbTransportFailure {
    param([AllowEmptyString()][string]$Reason)

    if ($Reason -match '(?i)ADB timeout \(\d+s\):') {
        return $true
    }
    if ($Reason -notmatch '(?i)ADB failed \(') {
        return $false
    }

    return $Reason -match '(?i)(daemon still not running|cannot connect to (?:the )?daemon|cannot connect to 127\.0\.0\.1:5037|failed to start daemon|ADB server didn''t ACK|device offline|device [^\s]+ not found|no devices/emulators found|protocol fault|connection (?:reset|closed))'
}
