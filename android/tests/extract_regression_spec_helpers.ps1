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
            'game',
            'classification',
            'expected_mission',
            'expected_level1',
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
    if (-not $generated) {
        $generated = $header.Generated
    }
    if (-not $generated) {
        $generated = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    }

    $canonical = ConvertTo-CanonicalRegressionSpec $spec
    $json = ($canonical | ConvertTo-Json -Depth 10) -replace "`r`n", "`n"
    $content = "// Auto-generated regression spec for: $sourceName`n// Generated: $generated`n$json`n"
    [System.IO.File]::WriteAllText($path, $content, [System.Text.UTF8Encoding]::new($false))
}

function Set-RegressionSpecLastTestResult($path, $lastTestResult) {
    $spec = Read-Json5File $path
    if (Test-JsonProperty $spec 'last_test_result') {
        $spec.PSObject.Properties.Remove('last_test_result')
    }
    if ($spec -is [System.Collections.IDictionary]) {
        $spec['last_test_result'] = $lastTestResult
    } else {
        $spec | Add-Member -NotePropertyName 'last_test_result' -NotePropertyValue $lastTestResult
    }
    Write-CanonicalRegressionSpec -path $path -spec $spec
}
