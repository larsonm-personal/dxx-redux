# Owned XL sound folders

Extend the completed named-audio runtime with the mission and level resource
scopes used by the pinned XL loader. This is a prerequisite for ambient sound
objects, not admission of those objects or qualification of the Pumo campaign.

## Implementation

1. Compare the pinned alias, HOG, level, mission and addon lookup behavior with
   the existing resource ownership and mission transition paths. Use the signed
   incoming mission ordinal; filenames alone cannot distinguish repeated levels.
2. Prepare immutable WAV resources from the selected mission HOG or its owned
   `mods/<hog>/sounds/levelNN`, `slevelNN` and mission folders. Preserve source
   priority, reject foreign shadows and malformed selected resources, and retain
   the current live bank after failed admission. Global addon installation
   support remains a separate requirement until its ownership is implemented.
3. Include selected virtual scope and content in resource identity. Exercise
   normal and secret ordinals, repeated filenames, changes between levels,
   save/rewind, device-rate changes and return to stock gameplay.
4. Audit the original Pumo sound folders without modifying its distribution.
   Decode all selected dependencies, keeping resource evidence distinct from
   ambient spatial playback and campaign acceptance.

## Acceptance

- Independent comparisons against the pinned XL lookup code and meaningful
  native runtime controls, including failure atomicity and foreign mounts
- Normal Android playback through published mission assets, with actual PCM
  comparisons, transition and persistence controls
- Scoped formatting, automation catalogs and affected public/private CI owners
- Updated evidence and remaining gates; no mission-name exceptions and no
  autonomous navigation

Keep the main checkout and private originals unchanged. Publish a reviewable
stacked PR after validation under the active run's authorization; do not merge.

## Completed validation

The owned provider, incoming ordinal, production playback and persistence paths
are implemented. All 43/46 public and 103 private required tests pass, with
sixteen original-format outcomes and three complete modern source comparisons.
All 65 Android steps pass, including sixteen actual-output comparisons and
138,752 PCM samples. Scoped formatting, direct PowerShell analysis and both
automation catalogs pass. See [evidence](../../XL_compat/evidence/audio-scopes.md)
for receipts and boundaries.

The original Pumo audit resolves all 644 ambient objects to 41 selected WAVs
with matching Windows/Linux source and decoded-sample hashes. Its 261-file
external tree also contains 196 files matching 142 retail sound-bank entries.
Classic sound replacements need their own source selection and runtime controls
before ambient output can rely on the complete audio profile. Ambient object
lifecycle, global addons and campaign qualification remain open.
