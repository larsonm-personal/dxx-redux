# PO2 engine feature compatibility study

## Goal

Correlate the advertised Plutonian Outbreak 2 features with the original archive contents and identify the Rebirth engine work needed for progressively better support.

## Work

- [x] Inventory and identify every PO2 mission, data, patch, sound, music, and documentation file.
- [x] Compare PO2 level and HOG metadata against stock D2/Rebirth capabilities.
- [x] Research the original executable patch and later D2X-XL compatibility implementation.
- [x] Trace each advertised feature to likely file formats and Rebirth engine subsystems.
- [x] Produce a prioritized compatibility assessment with uncertainties and recommended investigation steps.
- [x] Write a concise compatibility handoff beside the original `po2.7z` archive.

## Findings

### Archive structure

- The main mission descriptor contains eight regular levels and one secret level. The archive also contains the separate one-level Dark Matter and Vanhuta Tower missions. This does not exactly match the advertised ten-level count.
- Every main PO2 level has a large standard POG texture replacement. Most levels also have a standard HXM model replacement. Rebirth already knows these formats.
- The HXM files for levels 1 through 6 replace model 108, the player ship model, with four distinct model payloads. This is strong evidence for the four gradually introduced ships, at least visually.
- `pluton2.pmh` is a complete version 3 `HAM!` data set, not a Rebirth `XHAM` extension. Compared with stock D2 it contains 67 instead of 66 robots, 84 instead of 62 weapon records, 49 instead of 48 powerups, and 193 instead of 166 models. It changes 38 of the first 66 robot records and 39 of the first 62 weapon records.
- Rebirth has room for the PO2 robot, powerup, and model counts, but `MAX_WEAPON_TYPES` is 70. A direct load of 84 records would overrun the current fixed weapon table. The diagnostic direct-load run did not complete and was terminated.
- `pluton2.pti` uses Descent's TXB byte encoding. It decodes to a replacement game text table, including ten renamed primary weapons and ten renamed secondary weapons. It is original executable-patch data, not a D2X-XL runtime format.
- Both PO2 sound banks contain the same 183 indexed sounds as stock D2, but 43 payloads differ. The changed entries include long ambient water, lava, force-field, machinery, and environmental loops. The PO2 22 kHz bank is 9.6 MB compared with the 5.2 MB stock bank.
- The HOG includes standard HMP music and additional HMQ files. HMQ has the same `HMIMIDIP` format, but the engine song loader currently accepts only the HMP extension. Levels 2, 4, and 6 therefore miss their archive-only HMQ tracks, and level 8 selects the smaller HMP variant.
- Several levels contain D2X-XL `.clr` texture-light color tables. Rebirth ignores these, so D2X-XL colored-light presentation and the bundled generated lightmap caches are not reproduced.

### Feature correlation

- Custom textures, briefing screens, credits, cockpit art, and much of the scenic presentation use standard POG, PCX, TXB, and CTB assets and should already work.
- Custom robot models use standard HXM data and should load, subject to runtime testing of PO2's empty-face submodels. Robot AI, weapons, strengths, sounds, and other behavior are primarily in the unloaded PMH and therefore currently fall back to stock values.
- The four ship visuals are encoded by level-specific HXM replacements. The PMH has only one player-ship physics record, so any distinct per-ship handling beyond model changes was probably hardcoded in the custom executable.
- The selectable weapon-name table still has ten primary and ten secondary slots. The additional PMH weapon records are internal projectile or child-weapon definitions, not proof of 25 additional selection slots.
- The levels contain from one to nine objects that stock D2 data identifies as Guide-Bots, but the PO2 PMH removes the normal companion flag from robot 33 and does not mark any robot as a standard companion. The advertised wingmen therefore relied on custom executable logic. Rebirth also has one global `Buddy_objnum` and one global escort goal/owner state, so true simultaneous wingmen, especially in coop, need an escort-state redesign.
- The levels contain ordinary trigger networks and many visible changes should work. The briefing explicitly describes a live objective bar, but the archive has no objective script or configuration data. Objective sequencing and any goal-driven level mutation were therefore likely hardcoded in the patched executable.
- D2X-XL's own history explicitly documents PO2 fixes for empty-face polygon submodels, unusual door backing textures, and locating `descent.sng` in the mission mod folder. Its current mod loader recognizes mission-named `.ham` and `.s22` files, not PO2's `.pmh` and `.pti`. The available evidence supports partial compatibility work, not a complete reimplementation of the original executable.
- The original launcher, CD player, and executable-patch security are packaging and network-policy features, not mission content. They should not be reproduced as PO2-specific engine behavior.

## Recommended order

1. Add HMQ playback/fallback and mission-scoped S11/S22 replacement loading. These recover music and all 43 custom sound replacements without changing simulation rules.
2. Add a validated, mission-scoped full-HAM loader with complete rollback to stock data. Raise and audit weapon table limits to at least 84 before accepting PO2 PMH data. Audit object IDs, save/demo formats, multiplayer serialization, and every fixed-size consumer rather than changing the single constant alone.
3. Decode PTI through a narrowly scoped legacy text provider so PO2 weapon names and HUD labels do not replace global application localization.
4. Run every PO2 level with the PMH, HXM, POG, and sound bank active. Add regression coverage for model loading, level transitions, weapon creation, sound lookup, and restoration when leaving the mission.
5. Obtain the original patched executable or its source diff before implementing objectives, per-ship gameplay differences, or wingmen. Implement recovered behavior as generic mission data and scripting rather than filename-specific hardcoding.
6. Treat D2X-XL `.clr` colored lighting as an optional rendering enhancement after the original gameplay layer works.
