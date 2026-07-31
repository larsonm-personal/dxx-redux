function Read-Json5File($path) {
    $rawText = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    if ($rawText.Length -gt 0 -and $rawText[0] -eq [char]0xFEFF) {
        $rawText = $rawText.Substring(1)
    }
    $rawText = [regex]::Replace($rawText, '(?m)//.*$', '')
    $rawText = [regex]::Replace($rawText, ',\s*([}\]])', '$1')
    return $rawText | ConvertFrom-Json
}

function Test-JsonProperty($value, $name) {
    if ($null -eq $value) {
        return $false
    }
    if ($value -is [System.Collections.IDictionary]) {
        return $value.Contains($name)
    }
    return $value.PSObject.Properties.Name -contains $name
}

function Get-JsonPropertyValue($value, $name) {
    if (-not (Test-JsonProperty $value $name)) {
        return $null
    }
    if ($value -is [System.Collections.IDictionary]) {
        return $value[$name]
    }
    return $value.PSObject.Properties[$name].Value
}

function Get-JsonProperties($value) {
    if ($null -eq $value) {
        return @()
    }
    if ($value -is [System.Collections.IDictionary]) {
        return @($value.Keys | ForEach-Object {
                [PSCustomObject]@{ Name = $_; Value = $value[$_] }
            })
    }
    return @($value.PSObject.Properties | ForEach-Object {
            [PSCustomObject]@{ Name = $_.Name; Value = $_.Value }
        })
}

function Test-ExtractRegressionInfrastructureFailure($failureStep) {
    return $failureStep -in @('emulator_offline', 'setup_timeout', 'adb_staging_failed')
}

function Get-ExtractRegressionEvidenceRank($result) {
    if ($null -eq $result) {
        return 0
    }
    if ((Get-JsonPropertyValue $result 'test_mode') -eq 'full') {
        return 2
    }
    return 1
}

function Test-ExtractRegressionResultShouldReplace($existing, $candidate) {
    if ($null -eq $existing) {
        return $true
    }
    if ((Get-JsonPropertyValue $candidate 'status') -eq 'pass' -and
        (Get-JsonPropertyValue $existing 'status') -eq 'fail') {
        return $true
    }
    return (Get-ExtractRegressionEvidenceRank $candidate) -ge
    (Get-ExtractRegressionEvidenceRank $existing)
}

function Add-CanonicalProperty($target, $source, $name, [ref]$handled) {
    if (Test-JsonProperty $source $name) {
        $target[$name] = Get-JsonPropertyValue $source $name
        $handled.Value[$name] = $true
    }
}

function ConvertTo-CanonicalSourceFiles($sourceFiles) {
    return @($sourceFiles | ForEach-Object {
            $entry = [ordered]@{}
            if (Test-JsonProperty $_ 'name') {
                $entry.name = $_.name
            }
            if (Test-JsonProperty $_ 'sha256') {
                $entry.sha256 = $_.sha256
            }
            foreach ($property in (Get-JsonProperties $_ | Sort-Object Name)) {
                if ($property.Name -ne 'name' -and $property.Name -ne 'sha256') {
                    $entry[$property.Name] = $property.Value
                }
            }
            $entry
        })
}

function ConvertTo-CanonicalLastTestResult($result) {
    if ($null -eq $result) {
        return $null
    }

    $handled = @{}
    $canonical = [ordered]@{}
    foreach ($name in @(
            'status',
            'failure_step',
            'level_reached',
            'files_verified',
            'classification_confirmed',
            'test_mode'
        )) {
        if (Test-JsonProperty $result $name) {
            $canonical[$name] = Get-JsonPropertyValue $result $name
            $handled[$name] = $true
        }
    }
    foreach ($property in (Get-JsonProperties $result | Sort-Object Name)) {
        if (-not $handled.ContainsKey($property.Name)) {
            $canonical[$property.Name] = $property.Value
        }
    }
    return $canonical
}

function ConvertTo-CanonicalRegressionSpec($spec) {
    $handled = @{}
    $handledRef = [ref]$handled
    $canonical = [ordered]@{}

    foreach ($name in @(
            'source_type',
            'disc_image_type',
            'disc_id',
            'source_files',
            'source_specs',
            'game',
            'classification',
            'expected_mission',
            'expected_level1',
            'mission_selection_required',
            'mission_files',
            'expected_files',
            'audio_tracks',
            'total_extracted',
            'import_mode',
            'last_test_result'
        )) {
        if ($name -eq 'source_files' -and (Test-JsonProperty $spec $name)) {
            $canonical[$name] = ConvertTo-CanonicalSourceFiles (Get-JsonPropertyValue $spec $name)
            $handled[$name] = $true
        } elseif ($name -eq 'last_test_result' -and (Test-JsonProperty $spec $name)) {
            $canonical[$name] = ConvertTo-CanonicalLastTestResult (Get-JsonPropertyValue $spec $name)
            $handled[$name] = $true
        } else {
            Add-CanonicalProperty $canonical $spec $name $handledRef
        }
    }

    foreach ($property in (Get-JsonProperties $spec | Sort-Object Name)) {
        if (-not $handled.ContainsKey($property.Name)) {
            $canonical[$property.Name] = $property.Value
        }
    }

    return $canonical
}

function Get-RegressionSpecHeader($path) {
    $header = @{
        SourceName = $null
        Generated = $null
    }
    if (-not (Test-Path -LiteralPath $path)) {
        return $header
    }

    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        if ($line -match '^// Auto-generated regression spec for:\s*(.*)$') {
            $header.SourceName = $matches[1]
        } elseif ($line -match '^// Generated:\s*(.*)$') {
            $header.Generated = $matches[1]
        } elseif ($line -match '^\s*\{') {
            break
        }
    }
    return $header
}

function Write-CanonicalRegressionSpec($path, $spec, $sourceName = $null, $generated = $null) {
    $header = Get-RegressionSpecHeader $path
    if (-not $sourceName) {
        $sourceName = $header.SourceName
    }
    if (-not $sourceName) {
        $sourceName = [System.IO.Path]::GetFileName((Split-Path $path -Parent))
    }
    $canonical = ConvertTo-CanonicalRegressionSpec $spec
    $json = ($canonical | ConvertTo-Json -Depth 10) -replace "`r`n", "`n"
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        try {
            $existingCanonical = ConvertTo-CanonicalRegressionSpec (Read-Json5File $path)
            $existingJson = ($existingCanonical | ConvertTo-Json -Depth 10) -replace "`r`n", "`n"
            if ($existingJson -ceq $json -and $header.SourceName -ceq $sourceName) {
                return
            }
        } catch {}
    }
    # The header records when the regression oracle was generated. Callers
    # that update test evidence use the shared writer too, but those updates
    # must not churn the generation timestamp.
    if (-not $generated) {
        $generated = $header.Generated
    }
    if (-not $generated) {
        $generated = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    }

    $content = "// Auto-generated regression spec for: $sourceName`n// Generated: $generated`n$json`n"
    [System.IO.File]::WriteAllText($path, $content, [System.Text.UTF8Encoding]::new($false))
}

function Set-RegressionSpecLastTestResult($path, $lastTestResult) {
    $spec = Read-Json5File $path
    $existing = Get-JsonPropertyValue $spec 'last_test_result'
    if (-not (Test-ExtractRegressionResultShouldReplace $existing $lastTestResult)) {
        return $false
    }
    if (Test-JsonProperty $spec 'last_test_result') {
        $spec.PSObject.Properties.Remove('last_test_result')
    }
    if ($spec -is [System.Collections.IDictionary]) {
        $spec['last_test_result'] = $lastTestResult
    } else {
        $spec | Add-Member -NotePropertyName 'last_test_result' -NotePropertyValue $lastTestResult
    }
    Write-CanonicalRegressionSpec -path $path -spec $spec
    return $true
}

function Get-ExtractRegressionOracleStatus {
    param([string]$RepoRoot)

    $cdRoot = Join-Path (Join-Path $RepoRoot 'game_data') 'CD images'
    $sourceDirs = @()
    $specPaths = @()
    $missingSpecDirs = @()

    if (Test-Path -LiteralPath $cdRoot) {
        $sourceDirs = @(Get-ChildItem -LiteralPath $cdRoot -Directory -ErrorAction SilentlyContinue |
                Where-Object {
                    @(Get-ChildItem -LiteralPath $_.FullName -File -ErrorAction SilentlyContinue |
                            Where-Object { $_.Extension.ToLowerInvariant() -in @('.cue', '.iso') }).Count -gt 0
                    } |
                    Sort-Object FullName)
        $specPaths = @(Get-ChildItem -LiteralPath $cdRoot -Recurse -Filter 'extract_regression.json5' -File -ErrorAction SilentlyContinue |
                Sort-Object FullName)
        foreach ($dir in $sourceDirs) {
            $specPath = Join-Path $dir.FullName 'extract_regression.json5'
            if (-not (Test-Path -LiteralPath $specPath -PathType Leaf)) {
                $missingSpecDirs += $dir.FullName
            }
        }
    }

    return [pscustomobject]@{
        CdRoot = $cdRoot
        SourceDirs = $sourceDirs
        SpecPaths = $specPaths
        MissingSpecDirs = $missingSpecDirs
        HasSources = $sourceDirs.Count -gt 0
        HasSpecs = $specPaths.Count -gt 0
        Ready = ($sourceDirs.Count -gt 0) -and ($missingSpecDirs.Count -eq 0) -and ($specPaths.Count -gt 0)
    }
}

function Get-ExtractRegressionPwshPath {
    try {
        $current = Get-Process -Id $PID -ErrorAction Stop
        if ($current.Path) {
            return $current.Path
        }
    } catch {}

    $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
    if ($pwsh) {
        return $pwsh.Source
    }
    return 'pwsh'
}

function Invoke-ExtractRegressionOracleRecovery {
    param(
        [string]$RepoRoot,
        [switch]$Force
    )

    $pwsh = Get-ExtractRegressionPwshPath
    $gameDataDir = Join-Path $RepoRoot 'game_data'
    $extractScript = Join-Path $gameDataDir 'extract_all_cds.ps1'
    $generateScript = Join-Path $gameDataDir 'generate_regression_specs.ps1'
    if (-not (Test-Path -LiteralPath $extractScript -PathType Leaf)) {
        throw "CD extraction helper not found: $extractScript"
    }
    if (-not (Test-Path -LiteralPath $generateScript -PathType Leaf)) {
        throw "Regression spec generator not found: $generateScript"
    }

    $extractArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $extractScript)
    if ($Force) {
        $extractArgs += '-Force'
    }
    & $pwsh @extractArgs
    if ($LASTEXITCODE -ne 0) {
        throw "extract_all_cds.ps1 failed with exit code $LASTEXITCODE"
    }

    $generateArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $generateScript)
    if ($Force) {
        $generateArgs += '-Force'
    }
    & $pwsh @generateArgs
    if ($LASTEXITCODE -ne 0) {
        throw "generate_regression_specs.ps1 failed with exit code $LASTEXITCODE"
    }
}

function Ensure-ExtractRegressionOracles {
    param(
        [string]$RepoRoot,
        [string]$Context = 'CD extract regression tests',
        [switch]$NoPrompt
    )

    $status = Get-ExtractRegressionOracleStatus -RepoRoot $RepoRoot
    if ($status.Ready) {
        return $true
    }

    if (-not $status.HasSources) {
        Write-Host "FAIL: $Context needs CD image source files under $($status.CdRoot)" -ForegroundColor Red
        return $false
    }

    Write-Host "CD extraction regression oracles are missing or incomplete" -ForegroundColor Yellow
    Write-Host "  Source CD folders: $($status.SourceDirs.Count)" -ForegroundColor Yellow
    Write-Host "  Existing extract_regression.json5 files: $($status.SpecPaths.Count)" -ForegroundColor Yellow
    Write-Host "  Missing specs: $($status.MissingSpecDirs.Count)" -ForegroundColor Yellow
    foreach ($dir in ($status.MissingSpecDirs | Select-Object -First 8)) {
        Write-Host "    $dir" -ForegroundColor Yellow
    }
    if ($status.MissingSpecDirs.Count -gt 8) {
        Write-Host "    ... $($status.MissingSpecDirs.Count - 8) more" -ForegroundColor Yellow
    }
    Write-Host "Recovery will run game_data/extract_all_cds.ps1 and game_data/generate_regression_specs.ps1" -ForegroundColor Cyan

    if ($NoPrompt) {
        return $false
    }

    $answer = Read-Host 'Regenerate CD extraction oracles now? [y/N]'
    if ($answer -notmatch '^(y|yes)$') {
        Write-Host 'Skipping oracle regeneration' -ForegroundColor Yellow
        return $false
    }

    Invoke-ExtractRegressionOracleRecovery -RepoRoot $RepoRoot
    $status = Get-ExtractRegressionOracleStatus -RepoRoot $RepoRoot
    if (-not $status.Ready) {
        Write-Host "FAIL: CD extraction oracles are still incomplete after regeneration" -ForegroundColor Red
        return $false
    }
    return $true
}
