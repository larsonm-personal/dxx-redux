# Gradle wrapper and VS Code synchronization

## Plan

- [x] Inspect the Gradle wrapper, Android Gradle Plugin, scripted update flow, and existing editor settings
- [x] Select the newest mutually compatible Gradle and Android Gradle Plugin versions for JDK 21
- [x] Extend the scripted updater and checked-in VS Code settings so imports use the repository wrapper and managed JDK
- [x] Add or update focused regression coverage for version detection and generated editor configuration
- [x] Run scoped formatting, updater tests, and Gradle import/build validation

## Results

- The maintained Android build was already at the current compatible targets: Gradle 9.6.1, Android Gradle Plugin 9.3.1, and JDK 21.0.12
- The startup warning came from ignored `temp/xcrash-source-review`, whose upstream wrapper still uses Gradle 6.5, rather than from the Android application
- Workspace Java import now excludes ignored `temp` trees, explicitly enables Gradle wrappers, and selects the managed JDK for the Gradle daemon
- `check-updates.ps1` now synchronizes the managed JDK path and Java runtime major into `.vscode/settings.json` after applying dependency updates
- Focused PowerShell regression tests, scoped code quality, Gradle 9.6.1 wrapper startup, and the Android `help` configuration task passed with JDK 21
- Follow-up correction restored the existing portability comment in `.vscode/settings.json`; it had been mistakenly removed while reformatting the Java runtime block
