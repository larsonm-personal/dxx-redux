# Match known formatter entry points to a checkout without inspecting source-file text
function Test-DxxFormatterProcess {
    param([Parameter(Mandatory)]$Record, [Parameter(Mandatory)][string]$RepositoryRoot, $Lock, [string]$StartTicks)
    $comparison = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    $root = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('/', '\'))
    $scripts = @('android/run-code-quality.ps1') + @('run-clang-format', 'run-ktlint', 'run-psscriptanalyzer', 'run-shellcheck', 'run-shfmt', 'run-cmake-format', 'run-cmake-lint' | ForEach-Object { "android/helpers/$_.ps1" })
    $name = [IO.Path]::GetFileNameWithoutExtension([string]$Record.Name)
    $isPowerShell = $name -in @('pwsh', 'powershell')
    if ($isPowerShell -and $Lock -and $StartTicks -and
        $Lock.pid -eq $Record.ProcessId -and [string]$Lock.process_start_ticks -ceq $StartTicks -and
        [string]::Equals([string]$Lock.repository_root, $root, $comparison)) { return $true }
    $cwd = if ($Record.PSObject.Properties['WorkingDirectory']) { [string]$Record.WorkingDirectory } else { '' }
    $argv = if ($Record.PSObject.Properties['Arguments']) { @($Record.Arguments) } else { @() }
    if (-not $argv.Count) {
        $argv = @([regex]::Matches([string]$Record.CommandLine, '"(?<arg>[^"]*)"|(?<arg>\S+)') | ForEach-Object { $_.Groups['arg'].Value })
    }
    $candidates = @()
    if ($isPowerShell) {
        for ($index = 1; $index -lt $argv.Count - 1; $index++) {
            if ($argv[$index] -ieq '-File' -or $argv[$index] -ieq '-f') {
                $candidates += $argv[$index + 1]
                break
            }
            if ($argv[$index] -ieq '-Command' -or $argv[$index] -ieq '-c') {
                $ast = [Management.Automation.Language.Parser]::ParseInput($argv[$index + 1], [ref]$null, [ref]$null)
                foreach ($statement in $ast.EndBlock.Statements) {
                    if ($statement -is [Management.Automation.Language.PipelineAst]) {
                        foreach ($command in $statement.PipelineElements) {
                            if ($command -is [Management.Automation.Language.CommandAst]) {
                                $candidate = $command.GetCommandName()
                                if ($candidate) { $candidates += $candidate }
                            }
                        }
                    }
                }
                break
            }
        }
        foreach ($candidate in $candidates) {
            $path = ([string]$candidate).Replace('\', [IO.Path]::DirectorySeparatorChar)
            if (-not [IO.Path]::IsPathRooted($path)) {
                if (-not $cwd) { continue }
                $path = Join-Path $cwd $path
            }
            try { $path = [IO.Path]::GetFullPath($path) } catch { continue }
            foreach ($script in $scripts) {
                if ([string]::Equals($path, (Join-Path $root $script), $comparison)) { return $true }
            }
        }
        return $false
    }
    $nativeFormatter = $name -match '^(clang-format(?:-[0-9.]+)?|shellcheck|shfmt|cmake-format|cmake-lint)$'
    $javaFormatter = $name -eq 'java' -and @($argv | Where-Object { [IO.Path]::GetFileName([string]$_) -eq 'ktlint.jar' }).Count -gt 0
    if (-not $nativeFormatter -and -not $javaFormatter) { return $false }
    $rootPrefix = $root + [IO.Path]::DirectorySeparatorChar
    foreach ($path in @($cwd) + $argv) {
        if (-not $path -or -not [IO.Path]::IsPathRooted($path)) { continue }
        try { $path = [IO.Path]::GetFullPath($path) } catch { continue }
        if ($path.Equals($root, $comparison) -or $path.StartsWith($rootPrefix, $comparison)) { return $true }
    }
    return $false
}
