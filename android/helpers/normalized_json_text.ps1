function ConvertTo-WindowsProcessArgument {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Argument)

    if (-not $Argument) { return '""' }
    if ($Argument -notmatch '[\s"]') { return $Argument }

    $quoted = [System.Text.StringBuilder]::new()
    [void]$quoted.Append('"')
    $backslashes = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq '\') {
            $backslashes++
        } elseif ($character -eq '"') {
            [void]$quoted.Append(('\' * (($backslashes * 2) + 1)))
            [void]$quoted.Append('"')
            $backslashes = 0
        } else {
            [void]$quoted.Append(('\' * $backslashes))
            [void]$quoted.Append($character)
            $backslashes = 0
        }
    }
    [void]$quoted.Append(('\' * ($backslashes * 2)))
    [void]$quoted.Append('"')
    return $quoted.ToString()
}

function Set-CompatibleProcessArguments {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.ProcessStartInfo]$StartInfo,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]]$Arguments
    )

    if ($StartInfo.PSObject.Properties.Name -contains "ArgumentList") {
        foreach ($argument in $Arguments) {
            [void]$StartInfo.ArgumentList.Add($argument)
        }
    } else {
        $StartInfo.Arguments = ($Arguments | ForEach-Object { ConvertTo-WindowsProcessArgument $_ }) -join ' '
    }
}

function ConvertTo-NormalizedJsonText {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [switch]$MissionMetadata
    )

    $trimmed = $Text.Trim()
    if (-not $trimmed) { throw "JSON text is empty" }
    $formatterPath = Join-Path $PSScriptRoot "normalize_json.py"
    if (-not (Test-Path -LiteralPath $formatterPath -PathType Leaf)) {
        throw "JSON formatter not found: $formatterPath"
    }
    $python = Get-Command python -ErrorAction SilentlyContinue
    $usePyLauncher = $false
    if (-not $python) {
        $python = Get-Command py -ErrorAction SilentlyContinue
        $usePyLauncher = $true
    }
    if (-not $python) {
        throw "Python not found for JSON formatting"
    }

    $arguments = [System.Collections.Generic.List[string]]::new()
    if ($usePyLauncher) {
        $arguments.Add("-3")
    }
    $arguments.Add($formatterPath)
    if ($MissionMetadata) {
        $arguments.Add("--mission-metadata")
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $python.Source
    Set-CompatibleProcessArguments -StartInfo $startInfo -Arguments $arguments
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    if ($startInfo.PSObject.Properties.Name -contains "StandardInputEncoding") {
        $startInfo.StandardInputEncoding = $utf8NoBom
    }
    $startInfo.StandardOutputEncoding = $utf8NoBom
    $startInfo.StandardErrorEncoding = $utf8NoBom
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true

    $process = [System.Diagnostics.Process]::Start($startInfo)
    try {
        $process.StandardInput.Write($trimmed)
        $process.StandardInput.Close()
        $outputTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(30000)) {
            try {
                $process.Kill($true)
            } catch [System.Management.Automation.MethodException] {
                $process.Kill()
            }
            throw "JSON formatter timed out after 30 seconds"
        }
        $json = $outputTask.GetAwaiter().GetResult()
        $errorText = $errorTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw "JSON formatter failed with exit code $($process.ExitCode): $errorText"
        }
        $json = $json -replace "`r`n", "`n"
        return ($json.TrimEnd([char[]]@("`r", "`n")) + "`n")
    } finally {
        $process.Dispose()
    }
}
