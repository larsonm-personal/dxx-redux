# Parameterized sanitizer fixture, deliberately outside the ordinary test auto-discovery directory
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$HogDir,
    [Parameter(Mandatory)][string]$OutputRoot,
    [switch]$ConcurrentRewrite,
    [switch]$IsolatedDirectory
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/headless_process_pool.ps1')
$root = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
if (Test-Path -LiteralPath $root) { throw "Use a new output directory: $root" }
& (Join-Path $PSScriptRoot '../helpers/retain-recent-artifacts.ps1') -Artifacts $root
New-Item -ItemType Directory -Path $root | Out-Null
$exe = Join-Path $root 'config-read-test.exe'
Copy-Item -LiteralPath $Executable -Destination $exe
Get-ChildItem -LiteralPath (Split-Path $Executable) -File -Filter '*.dll' | Copy-Item -Destination $root
$pdb = [IO.Path]::ChangeExtension($Executable, '.pdb')
if (Test-Path -LiteralPath $pdb) { Copy-Item -LiteralPath $pdb -Destination $root }
# A valid config need not end in a newline
[IO.File]::WriteAllText((Join-Path $root 'descent.cfg'), 'DigiVolume=8', [Text.UTF8Encoding]::new($false))
if ($ConcurrentRewrite) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
public static class ConfigRewriteFixture {
    public static Task Start(string path, CancellationToken token) {
        return Task.Run(() => {
            byte[] bytes = System.Text.Encoding.ASCII.GetBytes(new string('\n', 1024) + "Unknown=" + new string('x', 65536) + "\nDigiVolume=8\n");
            while (!token.IsCancellationRequested) {
                try {
                    using (var file = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.ReadWrite)) {
                        Thread.Sleep(1);
                        file.Write(bytes, 0, bytes.Length);
                    }
                    Thread.Sleep(1);
                } catch (IOException) {}
            }
        });
    }
}
'@
    $rewriteCancellation = [Threading.CancellationTokenSource]::new()
    $rewriteTask = [ConfigRewriteFixture]::Start((Join-Path $root 'descent.cfg'), $rewriteCancellation.Token)
}
$task = [pscustomobject]@{
    FilePath = $exe
    Arguments = @('-hogdir', (Resolve-Path -LiteralPath $HogDir).Path, '-mission', 'd2', '-level', '1', '-route-confirm-json-out', (Join-Path $root 'result.json'))
    WorkingDirectory = $root
    TimeoutSeconds = 180
}
if ($IsolatedDirectory) {
    $userDirectory = Join-Path $root 'private-user'
    New-Item -ItemType Directory -Path $userDirectory | Out-Null
    $task.Arguments += @('-route-confirm-user-dir', $userDirectory)
    $originalConfigHash = (Get-FileHash -LiteralPath (Join-Path $root 'descent.cfg')).Hash
}
$configTestState = @{ Failed = $false }
try {
    Invoke-HeadlessProcessPool -Tasks @($task) -MaxParallel 1 -OnCompleted {
        param($item, $result)
        [IO.File]::WriteAllText((Join-Path $root 'stderr.log'), $result.StandardError)
        [IO.File]::WriteAllText((Join-Path $root 'stdout.log'), $result.StandardOutput)
        $configTestState.Failed = $result.ExitCode -notin @(0, 2) -or $result.TimedOut -or -not (Test-Path (Join-Path $root 'result.json'))
        Write-Host "exit=$($result.ExitCode) timeout=$($result.TimedOut)"
    }
} finally {
    if ($ConcurrentRewrite) {
        $rewriteCancellation.Cancel()
        $rewriteTask.GetAwaiter().GetResult()
        $rewriteCancellation.Dispose()
    }
}
if ($configTestState.Failed) { throw "Config reader failed; see $root" }
if ($IsolatedDirectory) {
    if (-not (Test-Path -LiteralPath (Join-Path $userDirectory 'RouteBot.plr'))) {
        throw 'Worker did not persist its pilot in the private directory'
    }
    if (-not $ConcurrentRewrite -and $originalConfigHash -ne (Get-FileHash -LiteralPath (Join-Path $root 'descent.cfg')).Hash) {
        throw 'Worker modified the shared config outside its private directory'
    }
}
