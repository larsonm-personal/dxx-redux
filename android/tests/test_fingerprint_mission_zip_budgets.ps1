$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'game_data\fingerprint_mission_zip_music.ps1') -BudgetTestOnly

$testRoot = Join-Path (Resolve-Path (Join-Path $repoRoot 'android\temp')).Path `
('fingerprint_budget_test_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null

function New-TestZip {
    param([string]$Path, [int]$Size)
    $stream = [IO.File]::Create($Path)
    try {
        $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
        try {
            $entry = $zip.CreateEntry('padding.dat', [IO.Compression.CompressionLevel]::Optimal)
            $output = $entry.Open()
            try {
                $buffer = New-Object byte[] 65536
                $remaining = $Size
                while ($remaining -gt 0) {
                    $count = [math]::Min($remaining, $buffer.Length)
                    $output.Write($buffer, 0, $count)
                    $remaining -= $count
                }
            } finally { $output.Dispose() }
        } finally { $zip.Dispose() }
    } finally { $stream.Dispose() }
}

try {
    $outputRoot = Join-Path $testRoot 'output'
    $tempRoot = Join-Path $testRoot 'temp'
    New-Item -ItemType Directory -Path $outputRoot, $tempRoot | Out-Null

    $ordinary = Join-Path $testRoot 'ordinary.zip'
    New-TestZip -Path $ordinary -Size 16
    $script:ArchiveBudget = @{ Entries = 0; DeclaredBytes = 0L; ActualBytes = 0L; Started = [DateTime]::UtcNow }
    $count = Extract-ZipAudio -ArchivePath $ordinary -OutputDir $outputRoot -SourcePrefix 'ordinary.zip' `
        -SourceMap @{} -TempRoot $tempRoot
    if ($count -ne 0) { throw 'ordinary non-audio ZIP returned an unexpected track count' }

    $ratio = Join-Path $testRoot 'ratio.zip'
    New-TestZip -Path $ratio -Size 8388608
    $script:ArchiveBudget = @{ Entries = 0; DeclaredBytes = 0L; ActualBytes = 0L; Started = [DateTime]::UtcNow }
    $rejected = $false
    try {
        $null = Extract-ZipAudio -ArchivePath $ratio -OutputDir $outputRoot -SourcePrefix 'ratio.zip' `
            -SourceMap @{} -TempRoot $tempRoot
    } catch {
        $rejected = $_.Exception.Message -match 'expansion ratio'
    }
    if (-not $rejected) { throw 'high-ratio fingerprint ZIP was not rejected' }
    Write-Host 'fingerprint mission ZIP budget tests passed'
} finally {
    $resolved = [IO.Path]::GetFullPath($testRoot)
    $allowed = [IO.Path]::GetFullPath((Join-Path $repoRoot 'android\temp')) + [IO.Path]::DirectorySeparatorChar
    if ($resolved.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
}
