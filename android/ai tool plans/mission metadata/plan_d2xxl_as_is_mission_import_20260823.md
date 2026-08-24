# D2X-XL download as-is mission import

Goal: let users select original downloaded ZIP/7z mission packages directly, without requiring manual repacking, wherever the contained mission uses formats already supported by DXX Redux.

- [x] Confirm current admission and extraction behavior for standard `DHF` missions in D2X-XL-style directory layouts.
- [x] Define the smallest safe handling for generated D2X-XL cache content while preserving mission, documentation, music, and mod assets.
- [x] Implement and test any required launcher changes for direct ZIP/7z import.
- [x] Run focused JVM tests and an as-is archive integration check.
- [x] Update the analysis report and this plan with the verified support boundary.

Result: the launcher accepts original ZIP/7z downloads containing standard
`DHF` missions, stores the source archive unchanged, and internally stages all
non-cache content needed to launch it. A generated top-level D2X-XL `cache/`
tree is skipped. The original `bahagad.7z` passed an emulator import and both
contained campaigns completed metadata analysis. Extended `D2X` HOGs and the
ACE-wrapped Panic package remain intentionally out of scope.
