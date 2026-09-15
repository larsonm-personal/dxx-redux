# Co-op briefing palette restoration

Status: Implemented and validated

## Problem

Co-op loads the mine before showing its briefing. PCX briefing backgrounds replace
gr_palette directly; returning to SCREEN_GAME does not restore it, and multiplayer
window activation deliberately skips the single-player palette restore path.
D2 ShowLevelIntro copies back its saved gr_palette array, but does not reload
gr_current_pal or invalidate color lookup results computed during the briefing.
The renderer therefore interprets gameplay color indices with a briefing palette.
D2 last_palette_loaded still names the level palette, so a normal cached load
would not repair the active palette, fonts or lookup caches.

## Change

Restore the level palette before exposing the mine after a released briefing.
Force D2's palette reload to also remap fonts and reset its gauge color cache;
reload D1's palette.256 through its existing palette loader. Upload the restored
palette and invalidate palette-indexed game textures in both engines.

## Validation

Record the palette/fade-table hash before presentation, and verify the restored
palette and active display palette match it. Extend the paired briefing tests to
require restoration on both host and client. Exercise D1 and Descent Maximum
(fixed), D2 level 13, including host skip/force-launch of a client still reading.
Run Android and Windows builds and scoped formatting checks.

## Results

- Android assembleDebug passed for all configured ABIs
- Windows D1/D2 builds passed
- Scoped code-quality checks passed
- Two-peer Maximum (fixed), level 13 passed with host skip and forced client launch
  Both devices logged `changed=1 matches=1`: briefing and active gameplay palettes
  differed before restoration, and the palette/fade table and active display
  palette matched gameplay after restoration
- Two-peer D1 briefing force-launch passed, including palette restoration on both
  participants and normal unpaused gameplay after release

Reproduction (Maximum must be installed on both test devices):

```powershell
.\android\tests\test_lan.ps1 -Game d2 -MissionFile max_f -InitialLevel 13 -BriefingPalette
.\android\tests\test_lan.ps1 -Game d1 -Briefings
```

Logs: `temp/coop-palette-maximum13-verified-live.log`,
`temp/coop-palette-d1-live.log`, `temp/coop-palette-display-build.log`, and
`temp/coop-palette-windows.log`
