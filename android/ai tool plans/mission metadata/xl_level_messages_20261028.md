# Owned XL level messages

Continue the authorized run through 2026-10-29 04:54:16 UTC on the isolated
xl_more_44_level_cues branch, based on xl_more_43_private_ci. PR #189 contains
the completed CI increment. Keep the root cmake checkout and private asset
repository unchanged; do not merge the stack.

## Scope and dependency

Pumo contains five MSG and twelve SND tables. The pinned XL source loads both
through LoadTextData, but message display and sound playback have different
runtime obligations. Implement owned message resources and action 29 first.
The shared text-table decoder will provide a tested foundation for sound-name
tables; decoding SND does not grant sound playback or effect-object admission.

Use actual source bodies from commit
2576a98b5de310e3ac7c9777df2677d1627b8bae. Pin consumed files and compare defined
reader/action behavior. Document explicit corrections for malformed text and
owned lifecycle rather than reproducing undefined access or stale pointers.

## Implementation and acceptance

1. Audit original Pumo MSG/SND bytes through the shared HOG index and pinned
   text/action source. Establish IDs, whitespace, newline escapes, timing and
   replacement behavior before changing runtime admission.
2. Add a bounded text-table decoder and independently compiled source oracle.
   Preserve authored text and identity. Reject ambiguous or unsafe encodings
   with a useful explanation. Keep original and generated fixture evidence
   distinct.
3. Prepare MSG resources owned by the selected mission and level. Dispatch
   supported wall/object message actions through the existing XL scheduler.
   Implement timed and modal display, replacement, cleanup and saved state.
   Keep stock Redux on its current path.
4. Verify production dispatch, visible message presentation, expiry, modal
   pause, delayed/nested events, disk and rewind, mission changes, malformed
   saves/resources and recovery. Use controlled actions and poses; no autonomous
   navigation. Extend the required public/private source and runtime selections.
5. Run relevant native and normal Android integration checks, scoped formatting
   and both catalogs. Publish a reviewable increment and inspect CI. Preserve
   every independent original-mission blocker and qualification boundary.

If source audit exposes a prerequisite that prevents faithful activation, finish
that prerequisite before admitting the action. The result must distinguish
resource decoding, active message behavior and original campaign qualification.

## Outcome

The separate message runtime, source oracle and controlled native/Android
integration are implemented. All 80 Android steps pass; the complete private
owner passes 94 tests plus sixteen format outcomes and three source comparisons.
The original Pumo audit resolves all 21 message actions across fourteen levels.
See [the evidence](../../XL_compat/evidence/level-messages.md) for exact artifacts,
the save-clock defect found by the strict checkpoint comparison, source-reader
corrections, platform checks and retained sound/effect/campaign requirements.
