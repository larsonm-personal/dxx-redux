# APK-only GitHub releases

1. Upload only the signed universal APK; keep local metadata/checksums for UploadOnly recovery
2. Refresh generated notes with the verified commit, versionCode, certificate and APK hash while preserving custom notes
3. Remove obsolete metadata/checksum release assets only after successful APK replacement
4. Extend release integration scenarios and run them on PowerShell 5.1 and 7, plus scoped code quality
5. Build from clean, pushed source, overwrite Android 1.0.0 and publish the separate legacy 1.0.0 edition
6. Verify remote tags, assets, hashes, notes and release appearance

GitHub automatically adds source ZIP/tarball links; its release API does not expose them as deletable assets
The working checkout has unrelated app edits; use an isolated checkout of committed source unless the user requests otherwise
