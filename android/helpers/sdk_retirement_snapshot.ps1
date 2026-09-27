# Evidence retained before sdkmanager can remove a package's ownership metadata
function Get-DxxSdkDirectoryIdentity([string]$Path) {
    Assert-DxxPlainSdkPath $Path
    if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
        if (-not ('DxxSdkDirectoryIdentity' -as [type])) {
            Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class DxxSdkDirectoryIdentity {
    [StructLayout(LayoutKind.Sequential)]
    private struct Info {
        public uint Attributes;
        public System.Runtime.InteropServices.ComTypes.FILETIME Creation, Access, Write;
        public uint Volume, SizeHigh, SizeLow, Links, IndexHigh, IndexLow;
    }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFile(string path, uint access, uint share,
        IntPtr security, uint disposition, uint flags, IntPtr template);
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetFileInformationByHandle(SafeFileHandle file, out Info info);
    public static string Read(string path) {
        using (var file = CreateFile(path, 0, 7, IntPtr.Zero, 3, 0x02200000, IntPtr.Zero)) {
            if (file.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error());
            Info info;
            if (!GetFileInformationByHandle(file, out info)) throw new Win32Exception(Marshal.GetLastWin32Error());
            return String.Format("windows:{0}:{1}:{2}:{3}:{4}", info.Volume, info.IndexHigh,
                info.IndexLow, info.Creation.dwHighDateTime, info.Creation.dwLowDateTime);
        }
    }
}
'@
        }
        return [DxxSdkDirectoryIdentity]::Read($Path)
    }
    if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
        # .NET CreationTime can reflect ctime on Linux, which changes during deletion
        $value = & /usr/bin/env TZ=UTC /usr/bin/stat -c '%d:%i:%w' -- $Path
        if ($LASTEXITCODE -ne 0 -or @($value).Count -ne 1 -or $value -notmatch '^\d+:\d+:.+$') { throw "Cannot identify SDK directory $Path" }
        return "linux:$value"
    }
    throw 'SDK retirement snapshots support Windows and Linux hosts'
}

function Get-DxxSdkSnapshotEntries([string]$Path) {
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Path)
    $count = 0
    while ($pending.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force -ErrorAction Stop) {
            if (++$count -gt 100000) { throw 'SDK package exceeds retirement snapshot entry limit' }
            if ($item.Name -eq '.git' -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Unsafe SDK package entry: $($item.FullName)" }
            $relative = $item.FullName.Substring($Path.Length + 1).Replace('\', '/')
            if ($item.PSIsContainer) {
                $pending.Push($item.FullName)
                [pscustomobject]@{ Relative = $relative; Kind = 'Directory'; Length = 0L; Sha256 = '' }
            } else {
                $length = $item.Length
                $stamp = $item.LastWriteTimeUtc.Ticks
                $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
                $item.Refresh()
                if (-not $item.Exists -or $item.Length -ne $length -or $item.LastWriteTimeUtc.Ticks -ne $stamp) { throw 'SDK payload changed while recording retirement' }
                [pscustomobject]@{ Relative = $relative; Kind = 'File'; Length = $length; Sha256 = $hash }
            }
        }
    }
}

function New-DxxSdkRetirementSnapshot([string]$Path) {
    $identity = Get-DxxSdkDirectoryIdentity $Path
    $entries = @(Get-DxxSdkSnapshotEntries $Path)
    if ((Get-DxxSdkDirectoryIdentity $Path) -cne $identity) { throw 'SDK directory changed while recording retirement' }
    [pscustomobject]@{ DirectoryIdentity = $identity; Entries = $entries; Recovering = $false }
}

function Assert-DxxSdkRemainingPayload([string]$Path, $Snapshot) {
    if ((Get-DxxSdkDirectoryIdentity $Path) -cne $Snapshot.DirectoryIdentity) { throw 'SDK directory identity changed after retirement started' }
    $expected = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
    foreach ($entry in $Snapshot.Entries) {
        if ($expected.ContainsKey($entry.Relative) -or $entry.Kind -notin @('File', 'Directory')) { throw 'Invalid SDK retirement snapshot' }
        $expected.Add($entry.Relative, $entry)
    }
    foreach ($item in @(Get-DxxSdkSnapshotEntries $Path)) {
        if (-not $expected.ContainsKey($item.Relative)) { throw "New file or directory appeared during SDK retirement: $($item.Relative)" }
        $original = $expected[$item.Relative]
        if ($item.Kind -cne $original.Kind -or $item.Length -ne $original.Length -or $item.Sha256 -cne $original.Sha256) { throw "SDK payload changed during retirement: $($item.Relative)" }
    }
}
