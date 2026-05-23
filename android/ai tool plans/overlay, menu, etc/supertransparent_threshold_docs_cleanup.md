## Supertransparent Threshold Docs Cleanup

### Goal
- document the origin of the `Test-IsSuperTransparentColor()` constants in the D2X-XL converter
- remove any stale door-specific special handling if it still exists

### Steps
- [x] confirm the upstream source for the exact key-color values and note what the import-side tolerance is based on
- [x] update the converter helper to replace the magic numbers with documented constants
- [x] remove stale door-specific debug residue if it is only dead legacy code
- [x] run code-quality checks

### Result
- exact RGB `120,88,128` comes from D2X-XL `superTranspKeys` and the older texmerge fallback shader
- follow-up pack testing showed the converter-side `13` tolerance broadened some door masks without helping `metl154`, so it was removed and the converter now matches D2X-XL's exact key-color mask generation again
- there was no live door-specific converter path left; only a stale `door` debug breakpoint inside disabled legacy shrink code in `d1/xmodel/tga.cpp` and `d2/xmodel/tga.cpp`