# BR-0087 SAF manifest I/O lifetime and prior remediation closeout

## Plan

- [x] Read repository instructions, review process, and the complete BR-0087 finding
- [x] Trace PhysicsFS archiver input ownership across SAF success and failure paths
- [x] Release the manifest I/O exactly once after successful eager parsing
- [x] Add focused native lifetime coverage and run relevant Android builds
- [x] Archive BR-0087 with exact validation evidence
- [x] Audit prior remediation plans and active findings for completed but untested or unarchived work
- [x] Run missing validations and archive only complete prior fixes
