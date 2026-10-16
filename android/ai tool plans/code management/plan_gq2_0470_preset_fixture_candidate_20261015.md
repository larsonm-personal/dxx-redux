# Chunk 0470 candidate: verify the owned preset before deletion, 2026-10-15

Diagnosis and implementation plan only. Not yet allocated a canonical GQF/GQR ID. Proposed P2/high test correctness and fixture ownership; candidate local priority47/MEDIUM, H/M/B/C/R23/0/4/10/10, subject to final canonical replay. No product/test changes or runtime reproduction.

## Concrete witness

test_gamepad_menu_navigation_unified.jsonc writes the fixed controller-test.ngs under the per-game write directory, enters Load Preset, restores defaults, reopens it, and confirms deletion with X/A. It asserts listbox type and finally zero items, but never selects or checks the fixture's name/count before deletion.

Both reset implementations leave .ngs files. Both native load_preset functions enumerate .ngs in the virtual root, sort the names, and start at index0; delete uses the selected name. Paired PHYSFSX_findFiles uses PHYSFS_enumerateFiles without restricting to this test's file. Shared Android setup mounts the per-game write directory into the virtual root and selects it for writes. Therefore a pre-existing writable aaa-unrelated.ngs can sort before controller-test.ngs and be deleted by the successful confirmation before the zero-item assertion fails. The fixed fixture write can also replace an existing same-name file. This is a source-derived ordinary schedule, not an executed device deletion.

## Ownership and implementation

Keep native preset enumeration, confirmation, deletion and settings format knowledge in the engines. Correct the Android test's target and fixture lifetime; do not add a broad native filter or clear every preset.

Reserve one attempt-owned fixture identity before writing. Refuse collision with an unowned existing pathname, or use an isolated disposable write namespace whose actual engine mount is verified. A disposable imported data set alone is insufficient: per-game write directories remain mounted independently. Use supported native preset selection/introspection to select exactly the owned name; assert its selected text and expected list membership before both cancel and confirmation. Record the initial unrelated inventory; after cancel require the owned file still exists, and after confirm require only that file to disappear while unrelated entries survive. Replace the global zero-items assumption with this precise invariant.

Install owned cleanup before fixture creation, retire the launched executor before removing its files, and remove only the created fixture on success, failure and ordinary cancellation. Report collision, staging, selection and cleanup errors rather than falling through. Coordinate checked ADB under BR0190, staging BR0593, exact observation BR0588 and lifecycle BR0543 without inventing a general framework.

BR0193 already owns guaranteed restoration of shared SAF/pilot/probe state. Retain its isolation and restoration rules; this candidate specifically requires verified target identity before the native destructive action, which restoration after a failed assertion cannot substitute for. BR0646's GOG empty audio baseline is a separate repaired source-registration fixture and should not be reopened. BR0284 request replacement is not target selection. Final census should preserve these boundaries.

## Required implementation acceptance

Through the actual maintained D1/D2 runner, seed writable unrelated presets sorting before and after the owned fixture and a colliding fixed old controller-test name. Exercise defaults, X/B cancellation, X/A confirmation, missing fixture, unexpected list entry/reorder, selector failure, staging failure and interruption before/after creation and confirmation. Require no deletion or byte change to any unrelated file, fail before confirmation on target mismatch, only owned cleanup, exact prior unrelated inventory and nonzero status when mandatory cleanup fails. Prove ordinary paired menu/controller coverage remains, including current native confirmation semantics and desktop builds if native hooks change.

Use a focused source/transport fixture for the entry contract and the real Android integration for behavior; a fake list with the expected selected index is insufficient. No acceptance is credited until the implementation tranche executes it. Deferred malformed-media/resource-pressure probes remain separate.

## Inherited boundary

The assigned change is an Android script. Its consumer context in d1/d2 explains behavior but does not make those context files assigned inherited changes. Prefer an Android fixture/select/assert/cleanup correction using existing engine-owned operations. No 1996-original edit or line saving is proposed.
