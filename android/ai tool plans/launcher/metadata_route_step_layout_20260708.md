# Metadata Route Step Layout Plan

## Goal
Make route steps in the metadata viewer easier to scan by keeping the first line short and moving route details into indented sub-paragraphs.

## Checklist
- [x] Split route step text into title and detail helpers.
- [x] Render the title as `N. action` with a slightly larger font.
- [x] Render route details as the first indented sub-paragraph.
- [x] Keep `Opens:` as the second indented sub-paragraph using the same style.
- [x] Run scoped formatting and Kotlin validation.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\SetupSections.kt` passed.
- `.\gradlew.bat :app:compileDebugKotlin --no-daemon --console=plain` passed from `android\` with JDK 21.
- The code-quality wrapper warned when the markdown plan path with spaces was passed in the same command; the Kotlin file still formatted and validated successfully.
