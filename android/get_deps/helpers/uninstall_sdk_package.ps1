#!/usr/bin/env pwsh
# Child of the supervised SDK cleaner; the parent owns the SDK mutation lock
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][ValidatePattern('^(build-tools|platforms|cmake|ndk|system-images);[A-Za-z0-9.;_+-]+$')][string]$PackageId
)
$ErrorActionPreference = 'Stop'
$name = if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) { 'sdkmanager.bat' } else { 'sdkmanager' }
$tool = Join-Path $SdkRoot "cmdline-tools/latest/bin/$name"
& $tool "--sdk_root=$SdkRoot" --uninstall $PackageId
exit $LASTEXITCODE
