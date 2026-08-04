# BR-0086 implementation plan

- [x] Read the repository instructions, adversarial review process, and BR-0086 finding.
- [x] Trace every SAF manifest reader, writer, and existing publication/test helper.
- [x] Make native parsing bounded and strict so malformed or incomplete documents reject the entire archive.
- [x] Publish launcher and regression-test manifests as complete same-directory atomic replacements.
- [x] Add focused parser/publication coverage without duplicating regression-output tests.
- [x] Run scoped formatting, unit/native build checks, and the focused emulator regression.
- [x] Review the final diff, record exact validation, and archive BR-0086 through the review process.
