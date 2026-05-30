# D2TP SP combined patch alignment plan

## Goal
- Compare Xfing's extracted D2TP+SP combined pack against the separate UUD2SP and UUD2TP HAM patch outputs
- Identify every overlapping semantic HAM patch field and record the combined-pack value chosen for each
- Update the separate patch generators or patch assets so overlapping fields produce identical values without weakening launcher conflict detection
- Re-run focused compatibility, composition, and formatting validation

## Tasks
- [x] Inspect current touched files and the extracted combined pack layout
- [x] Extract and compare UUD2SP, UUD2TP, and combined HAM patch/write scopes
- [x] Apply combined-pack overlap values to the separate patch sources
- [x] Update or add regression checks for the aligned overlaps
- [x] Run focused validation and record results

## Validation
- `:app:testDebugUnitTest --tests com.dxxredux.app.ModManagerDetailsTest`
- `verify-uud2sp-ham-patch-dxa.ps1`
- `verify-uud2sp-uud2tp-ham-composition.ps1` with semantic hash `0d150357c459916ebe74dab8a578c40f509b7208367b3f60cb421dc76ed91a03`
- Direct ktlint on `ModManagerDetailsTest.kt`
- Direct PSScriptAnalyzer format/lint on touched Xfing PowerShell scripts
- Regenerated default `UUD2SP1_4.no_ham.dxa` and `uud2tp-textures.dxa`
