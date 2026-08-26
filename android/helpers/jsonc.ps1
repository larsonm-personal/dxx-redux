function ConvertFrom-JsoncText {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [string]$SourceName = "JSONC input"
    )

    $withoutComments = [Text.StringBuilder]::new($Text.Length)
    $inString = $false
    $escaped = $false
    $index = 0
    while ($index -lt $Text.Length) {
        $current = $Text[$index]
        if ($inString) {
            [void]$withoutComments.Append($current)
            if ($escaped) {
                $escaped = $false
            } elseif ($current -eq '\') {
                $escaped = $true
            } elseif ($current -eq '"') {
                $inString = $false
            }
            $index++
            continue
        }
        if ($current -eq '"') {
            $inString = $true
            [void]$withoutComments.Append($current)
            $index++
            continue
        }
        if ($current -eq '/' -and $index + 1 -lt $Text.Length) {
            $next = $Text[$index + 1]
            if ($next -eq '/') {
                $index += 2
                while ($index -lt $Text.Length -and $Text[$index] -notin @("`r", "`n")) {
                    $index++
                }
                continue
            }
            if ($next -eq '*') {
                $index += 2
                $closed = $false
                while ($index -lt $Text.Length) {
                    if ($index + 1 -lt $Text.Length -and
                        $Text[$index] -eq '*' -and $Text[$index + 1] -eq '/') {
                        $index += 2
                        $closed = $true
                        break
                    }
                    if ($Text[$index] -in @("`r", "`n")) {
                        [void]$withoutComments.Append($Text[$index])
                    }
                    $index++
                }
                if (-not $closed) {
                    throw "Unterminated block comment in $SourceName"
                }
                continue
            }
        }
        [void]$withoutComments.Append($current)
        $index++
    }

    $clean = $withoutComments.ToString()
    $withoutTrailingCommas = [Text.StringBuilder]::new($clean.Length)
    $inString = $false
    $escaped = $false
    for ($index = 0; $index -lt $clean.Length; $index++) {
        $current = $clean[$index]
        if ($inString) {
            [void]$withoutTrailingCommas.Append($current)
            if ($escaped) {
                $escaped = $false
            } elseif ($current -eq '\') {
                $escaped = $true
            } elseif ($current -eq '"') {
                $inString = $false
            }
            continue
        }
        if ($current -eq '"') {
            $inString = $true
            [void]$withoutTrailingCommas.Append($current)
            continue
        }
        if ($current -eq ',') {
            $lookahead = $index + 1
            while ($lookahead -lt $clean.Length -and [char]::IsWhiteSpace($clean[$lookahead])) {
                $lookahead++
            }
            if ($lookahead -lt $clean.Length -and $clean[$lookahead] -in @('}', ']')) {
                continue
            }
        }
        [void]$withoutTrailingCommas.Append($current)
    }
    return $withoutTrailingCommas.ToString()
}

function Read-JsoncFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $raw = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    if ($raw.Length -gt 0 -and $raw[0] -eq [char]0xFEFF) {
        $raw = $raw.Substring(1)
    }
    $json = ConvertFrom-JsoncText -Text $raw -SourceName $Path
    return $json | ConvertFrom-Json -ErrorAction Stop
}

function Read-StrictJsonFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $raw = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
    if ($raw.Length -gt 0 -and $raw[0] -eq [char]0xFEFF) {
        $raw = $raw.Substring(1)
    }
    $jsonDocumentType = 'System.Text.Json.JsonDocument' -as [type]
    if ($jsonDocumentType) {
        $document = [System.Text.Json.JsonDocument]::Parse($raw)
        $document.Dispose()
    } else {
        Add-Type -AssemblyName System.Web.Extensions
        $serializer = [Web.Script.Serialization.JavaScriptSerializer]::new()
        $serializer.MaxJsonLength = [int]::MaxValue
        $serializer.RecursionLimit = 1024
        $null = $serializer.DeserializeObject($raw)
    }
    return $raw | ConvertFrom-Json -ErrorAction Stop
}
