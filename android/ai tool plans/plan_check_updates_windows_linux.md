# Dependency updater Windows/Linux repair

- Trace the pasted installer failure and inspect the shared host helpers
- Keep Bash selection in Get-DepPlatform.ps1 and require a native host Bash
- Detect Windows processes using the JDK before downloading a replacement, with actionable failure output and no process termination
- Preserve staged publication and add diagnostics for Windows rename failures
- Exercise the real installer with local fixtures on Windows and Linux, then run scoped code quality checks

The pasted failure is a Windows directory rename denial after successful extraction
A running Gradle 9.7.1 daemon currently uses C:\local\jdk-21\bin\java.exe
The existing tool_versions.conf edits belong to the user and must be preserved

Validation completed:

- Local JDK archive installation tests pass in Windows Git Bash and Linux WSL, including rollback and retry after both publication moves fail
- Windows platform/process preflight tests and dependency runtime update tests pass
- Linux portable temporary-file and transport-error tests pass
- Scoped mixed-language formatting and lint pass

The real JDK remains installed at its existing version; close its Gradle daemon before retrying an update
