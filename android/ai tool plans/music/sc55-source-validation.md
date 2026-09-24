# Nitro-shoe SC-55 source and song coverage

1. Verify latest upstream release, hash, declared license and sample credits;
   add a pinned direct download with truthful Info metadata
2. Audit all GM HMP/MIDI songs in local registered D1 and D2 data, including
   Vertigo where present, using shipping conversion, preset selection and TSF
   key/velocity regions. Distinguish missing presets, fallback kits and silent notes
3. Save reusable audit tooling and a per-song report; test the Android download,
   saved metadata and preview path with the real release

The upstream README declares CC BY 4.0 and credits borrowed Microsoft/Roland,
Creative and community samples. Display that declaration and provenance without
claiming independent clearance of every underlying sample. Add an upstream source;
do not bundle or rehost its binary.

Completed host validation (2026-09-23):

- Added v1.34, the newest upstream release, with source URL, full published sample
  credits and declared license. Verified the unchanged 10,375,822-byte file hash
- Added reusable `audit_soundfont_coverage.py` and native `soundfont_coverage`
  diagnostic using the shipping HMP converter, controller/preset logic and TSF
- Checked 27 D1 HMPs, 7 D2 HMPs and 7 embedded D2 MIDIs, including HMP repeats:
  393,833 note-ons, no uncovered or silent note/velocity mappings
- D2 game02 requests three absent GS bank-8 variations (programs 38, 116, 117,
  zero-based); TSF substitutes their GM bank-0 instruments. The absent SFX drum
  kit is not requested. Vertigo's checked HOG adds no MIDI assets
- Fixed rejection of two bounded empty loops in Fantasia. Regression fixtures
  distinguish inactive, reversed and out-of-bounds loops and verify the audit
  detects missing keys and program fallback
- Host music_soundfont, MIDI seek/timeline and loop-bound tests passed; both the
  bundled and SC-55 banks load/render. Android debug APK built successfully
- Android release download passed all 22 UI/preview steps, verified the SHA-256,
  persisted source/license metadata and reopened Info, then restored preferences
  and assets. Evidence: `temp/sc55-validation/device-retry/report.json`
- The first device run exposed an automation bug: exact Info lookup was followed
  by a substring click that could open a different Info button. Clicks now honor
  the resolved exact label; the same unmodified test passed after the fix
- Updated catalog regression passed all 25 steps on D1, covering bundled Info,
  GeneralUser and SC-55 confirmation/cancel paths. Scoped formatting/lint and
  `git diff --check` passed

Evidence: `temp/sc55-validation/coverage/coverage.md` and `coverage.json`, with
per-song TSVs, original/converted hashes and conversion logs beside them. This
is an instrument-availability audit, not a timbral accuracy or listening verdict.
