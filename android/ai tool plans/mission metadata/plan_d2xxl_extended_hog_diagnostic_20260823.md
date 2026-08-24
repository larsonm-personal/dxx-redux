# D2X-XL extended HOG import diagnostic

Goal: show a clear user-facing diagnostic when a selected mission archive contains a D2X-XL extended HOG, and verify it against every segregated D2X-XL mission download.

- [x] Identify the smallest inspection result that distinguishes an extended `D2X` HOG from an ordinary invalid archive.
- [x] Implement the diagnostic for ZIP, 7z, and RAR mission-import routing without importing incompatible content.
- [x] Add focused unit and integration coverage for the diagnostic and unchanged fallback behavior.
- [x] Build and install the debug APK.
- [x] Manually import every segregated D2X-XL mission archive on the emulator and record the displayed result.
- [x] Run scoped code quality and update this plan with final results.

Result: the launcher now reports `This level pack uses the D2X-XL extended HOG
format, which DXX Redux does not currently support` and leaves the archive
uninstalled. The visible launcher text was manually verified for:

- `anthology.7z`
- `BelialSystemXL.7z`
- `boilpnt.7z`
- `dinter_multilevel-2.0.7z`
- `lor-xl.7z`
- `pmines_v11.7z`

Negative controls from the same directory were also checked. `D2-XL.7z` uses
`DHF`, showed no warning, and imported successfully. `sphere-1.51.7z` also uses
`DHF`, correctly showed no extended-HOG warning, but retains a separate generic
failure because its descriptor uses unsupported D2X-XL `d2x-name` syntax.
