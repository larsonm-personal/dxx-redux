# Shared release build selection and checks against the packaged code
function Read-ReleaseBuildMode {
    while ($true) {
        $choice = (Read-Host 'Clean or unclean build? [unclean] (reuse checked build outputs)').Trim().ToLowerInvariant()
        if ($choice -in @('', 'u', 'unclean')) { return 'Unclean' }
        if ($choice -in @('c', 'clean')) { return 'Clean' }
        Write-Host 'Enter clean or unclean' -ForegroundColor Yellow
    }
}

function Assert-ReleaseApkIdentity {
    param(
        [string]$Apk, [string]$Dexdump, [string]$Commit, [int]$VersionCode,
        [string]$BuildDate, [string]$BuildTime
    )
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($Apk)
    $dexPath = Join-Path ([IO.Path]::GetDirectoryName($Apk)) ('.identity-' + [guid]::NewGuid().ToString('N') + '.dex')
    try {
        $expected = @{
            GIT_SHORT_HASH = $Commit.Substring(0, 12); GIT_COMMIT_COUNT = "$VersionCode"
            BUILD_DATE = $BuildDate; BUILD_TIME = $BuildTime; BUILD_TYPE = 'release'
        }
        $classes = @()
        foreach ($entry in $archive.Entries) {
            if ($entry.FullName -notmatch '^classes\d*\.dex$') { continue }
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dexPath, $true)
            $dump = (& $Dexdump -e $dexPath) -join "`n"
            if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect packaged DEX identity' }
            $classes += [regex]::Matches($dump, "(?s)Class descriptor\s+: 'Lcom/dxxredux/app/BuildInfo;'.*?(?=\n\s*Class #|\z)")
        }
        if ($classes.Count -ne 1) { throw 'APK must contain exactly one BuildInfo class' }
        foreach ($field in $expected.Keys) {
            $pattern = '(?s)name\s+: ''{0}''\s+type\s+: ''Ljava/lang/String;''\s+access[^\n]+\n\s+value\s+: "{1}"'
            $match = [regex]::Match($classes[0].Value, ($pattern -f $field, [regex]::Escape($expected[$field])))
            if (-not $match.Success) { throw "Packaged BuildInfo mismatch: $field" }
        }
        # Keep this format synchronized with app/build.gradle's generated native header
        # The marker lives in each engine's replay-metadata translation unit
        $marker = "dxx-build:$Commit`:$VersionCode"
        foreach ($abi in @('armeabi-v7a', 'arm64-v8a', 'x86_64')) {
            foreach ($engine in @('d1', 'd2')) {
                $name = "lib/$abi/libdxx-redux-$engine.so"
                $entry = $archive.GetEntry($name)
                if (-not $entry) { throw "APK lacks $name" }
                $stream = $entry.Open()
                $buffer = [IO.MemoryStream]::new()
                try {
                    $stream.CopyTo($buffer)
                    $native = [Text.Encoding]::ASCII.GetString($buffer.ToArray())
                    if (-not $native.Contains($marker + [char]0)) { throw "Packaged native build identity mismatch: $name" }
                } finally {
                    $buffer.Dispose()
                    $stream.Dispose()
                }
            }
        }
    } finally {
        $archive.Dispose()
        if (Test-Path -LiteralPath $dexPath) { Remove-Item -LiteralPath $dexPath }
    }
    Write-Host 'Verified packaged Kotlin and both native engines for all three ABIs'
}
