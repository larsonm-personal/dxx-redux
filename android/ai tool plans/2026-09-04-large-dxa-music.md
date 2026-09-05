# Large DXA music metadata and preview

- [x] Identify the 16 MiB default ZIP source limit rejecting Enemy Within's 385 MiB DXA
- [x] Use the existing archive entry size limit for music DXA streams, retaining separate ZIP preamble validation
- [x] Cover catalog discovery and preview reads from extracted and archive-backed large DXAs
- [x] Run scoped code quality and relevant Kotlin tests

Validation: both new large-DXA tests failed before the fix; all 98 selected music, ZIP reader, budget, and mission import tests passed afterward via `:app:testDebugUnitTest`

Scoped code quality passed; Kotlin-only changes required no native CMake rebuild
