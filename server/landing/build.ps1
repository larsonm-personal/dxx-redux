# Render only public HTML; keep deployment-specific configuration out of Git
param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'config.local.json'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'dist')
)
$ErrorActionPreference = 'Stop'
$config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
$fields = @('title', 'github_url', 'group_url')
foreach ($name in $config.PSObject.Properties.Name) {
    # Older deployment configs may still include the now-unused testing link
    if ($name -notin $fields -and $name -ne 'testing_url') { throw 'Unknown landing configuration field' }
}
foreach ($name in $fields) {
    $value = $config.$name
    if ($value -isnot [string] -or [string]::IsNullOrWhiteSpace($value)) { throw "Missing landing field: $name" }
    if ($name -ne 'title') {
        $uri = $null
        if (-not [Uri]::TryCreate($value, [UriKind]::Absolute, [ref]$uri) -or
            $uri.Scheme -ne 'https' -or $uri.UserInfo) { throw "Expected an HTTPS link: $name" }
    }
}
$template = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'index.template.html') -Raw
$html = [regex]::Replace($template, '\{\{(TITLE|GITHUB_URL|GROUP_URL)\}\}', {
        param($match)
        [System.Net.WebUtility]::HtmlEncode($config.($match.Groups[1].Value.ToLowerInvariant()))
    })
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = Join-Path $OutputDirectory 'index.html'
if (-not (Test-Path -LiteralPath $output) -or [IO.File]::ReadAllText($output) -cne $html) {
    [IO.File]::WriteAllText($output, $html, [Text.UTF8Encoding]::new($false))
}
Write-Output 'Landing page rendered'
