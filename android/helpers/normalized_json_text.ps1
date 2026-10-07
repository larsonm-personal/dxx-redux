. (Join-Path $PSScriptRoot 'test_host_platform.ps1')

function Write-NormalizedJsoncFile {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Text,
        [string]$ExistingPath = $Path
    )

    $formatted = ConvertTo-NormalizedJsonText -Text $Text -RepositoryJsonc
    if ([IO.File]::Exists($ExistingPath)) {
        $existing = [IO.File]::ReadAllText($ExistingPath)
        $normalizedExisting = $null
        try { $normalizedExisting = ConvertTo-NormalizedJsonText -Text $existing -RepositoryJsonc } catch {
            # A valid regenerated document can replace a damaged prior sidecar
        }
        if ($normalizedExisting -ceq $formatted) {
            # Directory publication replaces a staged album as a unit. Carry the
            # unchanged sidecar's exact bytes and timestamp into that staging area
            $comparison = if (Test-RegressionWindowsHost) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
            if (-not [string]::Equals([IO.Path]::GetFullPath($Path), [IO.Path]::GetFullPath($ExistingPath), $comparison)) {
                [IO.File]::Copy($ExistingPath, $Path, $true)
                [IO.File]::SetLastWriteTimeUtc($Path, [IO.File]::GetLastWriteTimeUtc($ExistingPath))
            }
            return
        }
    }
    . (Join-Path $PSScriptRoot 'atomic_text_file.ps1')
    Write-Utf8NoBomTextAtomically -Path $Path -Text $formatted
}

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
        [switch]$MissionMetadata,
        [switch]$RepositoryJsonc
    )

    $trimmed = $Text.Trim()
    if (-not $trimmed) { throw "JSON text is empty" }
    $formatterPath = Join-Path $PSScriptRoot "normalize_json.py"
    if (-not (Test-Path -LiteralPath $formatterPath -PathType Leaf)) {
        throw "JSON formatter not found: $formatterPath"
    }
    $python = Resolve-RegressionPythonCommand
    if (-not $python) { throw "Python 3 not found for JSON formatting" }

    $arguments = [System.Collections.Generic.List[string]]::new()
    foreach ($argument in $python.PrefixArguments) { $arguments.Add($argument) }
    $arguments.Add($formatterPath)
    if ($MissionMetadata) {
        $arguments.Add("--mission-metadata")
    }

    $executable = $python.Path
    if ($RepositoryJsonc) {
        if ($MissionMetadata) { throw 'RepositoryJsonc and MissionMetadata cannot be combined' }
        . (Join-Path $PSScriptRoot 'code-quality-files.ps1')
        $repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
        $executable = Get-CodeQualityNode -RepoRoot $repoRoot
        $arguments.Clear()
        $arguments.Add((Join-Path $repoRoot 'android/tools/code-quality/format-text.mjs'))
        $arguments.Add('--stdin-jsonc')
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $executable
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
