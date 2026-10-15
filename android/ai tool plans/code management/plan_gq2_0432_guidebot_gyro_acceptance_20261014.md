# Chunk0432 Guidebot and gyro acceptance, 2026-10-14

Diagnosis and plans only. No product or test edits, builds, sensor/device/runtime probes, staging or commit.

## Migration removals and current defaults

The assigned tests remove legacy layout-version and gyro conversion fixtures. Current TouchLayoutRepository declares version11; raw TouchLayout.fromJson and the human decoder require exactly that integer version. Repository load preserves an unreadable stored file and falls back to bundled defaults; current save emits current fields. The removed migrations are intentional under the disposable Android format policy. Do not restore legacy readers, conversion heuristics or historical-version test matrices as cleanup work.

GyroConfig raw/human parsing reads explicit per-axis fractions with0.1/0.1/0.3 defaults; the old single-deadzone numeric heuristic is gone. Current bundled Touch Default and Controller Menus explicitly author approximately0.22936/0.22936/0.6. Preserve these declared current values unless a separate product decision changes them; do not infer that every authored value should be reset to10percent. Historical BR-0516 conversion-at-load is no longer the current parser behavior. Its status is not changed here; reconcile its obsolete migration acceptance against current policy and actual current-schema round trips before closure. Selected raw/human code does not establish a lossless complete gyro calibration codec.

## Guidebot behavior

Preserve locked-wheel ring bounds of two-thirds through1.2times radius and separate center, actual Deploy-only locked strip, special spawn selection and shared secret/control segment filtering. Current Warp to Me name is consistent across binding1042, radial templates, bundled preset, native meta-action constant and D2 command consumer. Native JNI forwards to meta_action_dispatch; UI press sets a pending flag and the game thread consumes it before recording and requesting escort_warp_to_player. Selected native warp admission checks console object, multiplayer/cooperative ownership, buddy presence and clear placement. Full movement/network semantics and the entire pending request family are not freshly reviewed here. Existing BR-0244 retains the unsynchronized volatile publication/test-clear concern; a label or geometry fixture does not prove request acknowledgement.

## Gyro behavior and existing owners

Preserve configured versus in-game active state, master toggle routing, long-press binding indicators, demo/headlight independent indicator policy and bounded200-2000ms parsed button delay with actual delayed runnable. In-game gyro toggle changes runtime state without rewriting the persisted configured flag. Sensor output remains source-tagged gyro input in the mixer. Sensor manager computes per-axis clamped values after activation, calibration and deadzone processing; no sensor execution was performed.

BR-0422: conditional inactivity returns after zeroing diagnostics only; setConfig/remap and manager pause do not release old gyro axes. Current Activity stop separately calls mixer.releaseAll, so do not claim the complete stop path now lacks all release. Keep cleanup focused on inactive/remapped/disabled producer ownership, release gyro contributions atomically and preserve touch/controller inputs.

BR-0423: ADS_ONLY still reads toggledOn, initialized false, while MainActivity's offered gyro toggle changes only its separate runtime state. The current Kotlin application search found no assignment to toggledOn beyond declaration. A later fix must define one actual activation policy and wire that exact state; apply current disposable schema policy rather than reintroducing compatibility migration.

BR-0424: manager pause clears hasReference, resume does not restore saved reference, and MainActivity resumes the manager before reapplying config only when the loaded layout differs. Preserve explicit absolute calibration across registration pauses or define visible invalidation; rate sample and explicit recenter policies remain distinct. Coordinate axis release with0422.

Future paired D1/D2 acceptance must exercise actual sensor/activation/config/lifecycle paths and inspect source/native state, not only pure indicator helpers. Keep the removed legacy fixtures removed. Existing findings and current repairs remain separate; no new root or status change admitted.
