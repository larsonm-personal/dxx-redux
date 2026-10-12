# Implement save-safe mod toggling

Requested: implement the approved mod-save compatibility design, preserving existing saves and installed-but-disabled mods

## Work

- [x] Audit real stock/UUD definition differences and add precise rejection evidence
- [x] Add native source-definition metadata and supported effect/door adaptation without changing upstream save layouts
- [x] Preserve one-shot and destroyed-monitor state; reject unsupported extra-asset dependencies and keep exact replay/coop policy
- [x] Handle affected existing saves conservatively and protect secret companion transitions
- [x] Add actual engine and launcher save/toggle/resume regression coverage across D1, D2 and D1-in-D2
- [x] Build both engines and APK, run coverage, catalog checks and scoped formatting

Existing logging/menu changes and unrelated release work in the workspace must be preserved. Do not edit `android/outstanding_bugs.md`

## Audit

UUD2TP changes effects 25 (destroyed monitor target), 40 (previously unused slot), 76 (object animation assignment), and adds 105/106 (extra lava). Effect 25 changes its destroyed texture from 373 to 910 and its one-shot from 38 to 40. Existing saves may therefore contain both unused effect overrides and real mod texture references; skipping every out-of-range effect is insufficient

UUD2SP changes effect 3 frame time from 3276 to 3277 and several door durations. UUD1 also changes door durations. Preserve active door phase across supported timing changes

## Implementation

Android saves use versions 20 (D1), 43 (D2), and 44 (D1-in-D2). Upstream desktop versions remain unchanged. A bounded, checksummed native compatibility trailer records effective effect/door definitions, audited composed patch identities, level and gameplay source hashes, and used surface, object-texture and wall-animation dependencies. Hashing happens at asset/load boundaries, never in periodic mod usage reporting or memory rewind capture

Ordinary changed loops use the current definition and saved loop clock. Active or stopped animations preserve phase. UUD2's active monitor effect 40 maps to stock effect 38; intermediate overlay 911 maps to 386, completed overlay 910 maps to 373, and orientation bits survive. The stock effect 38 loop cannot overwrite the remapped one-shot. Moving linked doors require matching geometry and consistent timing ratios

The legacy recovery path requires the recognizable 40/105/106 record set or a monitor one-shot with its matching surface. Ambiguous records, active legacy doors, and unprovable legacy companion transitions ask the player to re-enable the original mods and re-save. Installed disabled archives are never mounted as a recovery shortcut

D1-in-D2 validates saved source records before level entry and checks live effect definitions after native D1 publication, respecting its deferred asset lifecycle. Its existing native/custom asset identity remains authoritative. Additional D2 mission HAM identities are captured while their owning archive is mounted. Coop and replay permit no presentation adaptation; memory rewind keeps its compact exact format

Companion dependencies are preflighted with the primary. D2 restores the primary successfully before replacing its active secret companion, so compatibility rejection cannot delete or replace the companion. Rejection details are exposed in introspection and displayed after failed-restore cleanup has produced a complete menu

## Validation and commands

- `android/tests/test_android_mod_save.ps1`: actual Android D1/D2 engine fixtures, including D1-in-D2 assets, destroyed monitors, completed one-shots, door phase, extra world/model dependencies, source corruption, legacy records, exact coop policy, and compatible/incompatible companions
- `android/tests/test_mod_save_toggle.ps1`: real launcher import, installed-but-disabled toggle, cold resume, re-enable, reordering, resave and exact resume in D1, D2, D1-in-D2 and Vertigo level 2. Also checks the error dialog, return to one complete menu, and byte-for-byte save preservation after a rejected source checksum
- `android/tests/test_startup_restore_failure.ps1`: updated to retain the legacy post-world EOF case; both engines returned to one complete menu and touch Quit exited
- Both engine libraries and the x86_64 debug APK built successfully; automation and suite catalog checks passed; scoped mixed-language formatting/lint passed

Audited source values can be regenerated with `python android/helpers/generate_mod_save_supported_data.py`. The frozen stock effect evidence identifies its retail HAM SHA256 and is exercised against actual loaded engine definitions

This implements the recommended presentation-pack transition, not arbitrary gameplay-mod migration or live asset hot-swapping. The overnight fuzz design remains a separate task. Compatible secret companion preflight is covered by the native fixture; a full launcher secret-portal round trip is not part of this regression run
