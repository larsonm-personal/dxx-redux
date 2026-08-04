# BR-0085 implementation plan

- [x] Read repository instructions, the adversarial review process, and the complete BR-0085 finding.
- [x] Trace SAF descriptor ownership, URI opening, random-access I/O, duplication, and current test seams.
- [x] Stage nonseekable or unsuitable descriptors into immutable seekable storage and derive the verified length.
- [x] Reject invalid seek offsets before signed conversion and preserve independent duplicate positions.
- [x] Add focused native and provider-backed regression coverage at the existing SAF boundary.
- [x] Run scoped quality, host/JVM tests, all Android debug ABIs, and focused emulator validation.
- [x] Review the final diff, record exact validation, and archive BR-0085.
