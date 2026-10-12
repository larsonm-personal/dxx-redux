# Save-safe toggling of recommended mods

Status: implemented for the audited recommended presentation packs; see [implementation and validation](plan_mod_save_implementation_20261011.md)

## User-visible behavior

A single-player save from an ordinary stock D1, D2 or Vertigo level should resume after enabling, disabling or reordering the supported recommended presentation packs. Installed-but-disabled packs remain disabled. Progress, inventory, destroyed monitors, opened doors and secret-level state survive. A looping animation may restart at the equivalent point in the current animation cycle

Changes take effect at the next engine launch, as today. This does not propose hot-swapping live assets. If the saved world actually requires added assets or altered gameplay definitions, reject the restore before replacing the running world and identify the missing/incompatible pack. Do not silently enable a pack or overwrite the save

## Why the current restore fails

`d1/main/state.c` and `d2/main/state.c` serialize sparse effect runtime overrides against the loaded definitions. D2's validator rejects saved effect IDs outside `Num_effects`, nonpositive current frame counts, and saved frame indices beyond the current clip's frame count

UUD2TP is more than replacement pixels: its audited HAM patch repairs effect 40 from an unused slot into a three-frame animation, changes effects 25 and 76, and adds effects 105 and 106. Disabling it changes the definition table against which a save is checked. The supplied logs confirm rejection at the effects stage, but do not identify the precise field or effect. Preserve this distinction until a save or a detailed rejection diagnostic confirms it

## Compatibility boundary

| Change                                                                            | Intended policy                                                                                                                |
| --------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| PCM sound replacements; fonts and menu images                                     | Compatible with gameplay saves; do not serialize sample caches or GPU state                                                    |
| High-resolution texture pixels, mipmaps and sampling                              | Compatible when underlying texture identities and gameplay flags are unchanged                                                 |
| Supported UUD repair to a looping effect                                          | Rebuild its presentation from the current definition and saved simulation time                                                 |
| Added UUD effect unused by the saved world                                        | Drop the obsolete visual override when disabling; initialize it normally when enabling                                         |
| One-shot effect or destroyed monitor                                              | Preserve its world mutation; translate animation state only when the target and final surface have valid semantic counterparts |
| Level uses added lava, textures or doors                                          | Require the defining assets unless an explicitly verified semantic fallback exists; never map by nearest numeric ID            |
| Arbitrary HAM/HXM changes, robot/weapon behavior, geometry or mission replacement | Outside this initial guarantee; retain strict validation                                                                       |
| Coop, deterministic replay and rewind within a running session                    | Keep exact definition identity initially; do not silently relax simulation compatibility                                       |

Mixed packs must be assessed by the definitions they actually change, not a blanket "cosmetic" label. UUD2SP changes assignments and door timing as well as PCM. Its timing changes need the same audit of in-progress doors and effects before claiming all transitions safe

## Native implementation

1. Add precise, bounded rejection diagnostics first: stage, record index, saved effect ID/frame/flags/segment/side/destination, and current definition count/frame count. One log per rejected restore, with no PCM hashing
2. At asset preparation, compute and cache native definition identities. Separate gameplay-relevant structure from presentation fields. Use mission owner/descriptor identity plus stable asset identities; record the effective composed definitions after mod precedence and mission overlays, not just filenames or enabled checkboxes
3. Save a versioned compatibility block containing the effective identity and the definition information needed to interpret referenced runtime records. Keep a native-owned schema shared by both engines; Kotlin can display the engine's compatibility result but must not parse HAM or invent effect mappings
4. Parse and structurally validate the complete save before applying compatibility changes: bounded counts, unique IDs, complete records, flags, integer ranges and real segment/side/object references. Validate source records against saved source definitions, not immediately against today's `Num_effects`. A missing live effect is not by itself proof that a malformed record can be ignored
5. Resolve mappings and build a restore policy before replacing the world. A supported transition requires known semantic correspondence and valid references throughout walls, surfaces, active doors, one-shot targets and secret companions. Exact identity uses today's exact restore path
6. Restore gameplay state first. For changed compatible loops, use the current `effect_get_loop_state`/`reset_special_effects_to_time` machinery and saved effect-loop time. Reapply valid stopped state. For a compatible one-shot, preserve its segment, side and destination, and map elapsed phase to the current clip with checked arithmetic. Never restart a completed destruction effect or restore an intact monitor over a saved destroyed one
7. Ignore an obsolete override only when source validation and reference analysis prove it has no remaining gameplay meaning. Initialize newly available effects from current definitions. Rebuild derived presentation caches; never restore stale bitmap pointers or sample buffers
8. Apply the same policy to secret-level companion saves as an atomic pair. Refuse the whole transition if either world has an unmet dependency. A failed attempt must not write either source file or an exit/periodic autosave of the partial world
9. Report `exact`, `presentation_adapted`, or `incompatible` with bounded reasons and adapted-record counts. Show a brief notice only when useful, e.g. "Loaded with updated texture animations" or "This save uses UUD2 extra textures; enable UUD2 Texture Fixes to continue"

## Existing saves

The user specifically requested preserving saves across this change, so compatibility with the affected current format is in scope despite the repository's default disposable Android formats policy

Do not infer a legacy save's source definitions merely from the currently installed mods. For the known recommended pack revisions, add a constrained native recovery path that tests recorded runtime state against the audited stock/UUD source definitions and rejects ambiguous gameplay mappings. Installed disabled archives may supply read-only source-definition evidence, but must not be mounted into the active game or enabled implicitly

If the old format cannot prove a safe mapping, preserve the original and explain the recovery path: re-enable the original pack set, load it, and save with the new compatibility block. Keep legacy handling limited to the specific released formats and supported source revisions needed for this issue; do not build a general migration framework

## Acceptance

- Save stock -> enable each pack -> resume; save modded -> disable -> resume; enabled -> reorder -> resume; all mods remain installed in the disabled cases
- D1, D2, D1-in-D2 and Vertigo level 2; multiple texture palettes and secret/base companion transitions
- Compare player progress, inventory, objectives, world surfaces, door position, active one-shots and mission time before/after; test a destroyed monitor and a door mid-opening
- Exercise all three UUD effect changes plus additions, including a fixture that really uses the extra lava/door textures and must refuse a missing dependency
- Restore the same unchanged asset set and confirm exact replay behavior remains unchanged
- Corrupt/truncated/duplicate/out-of-range effect records must still fail; error handling returns to one usable menu and both keyboard and touch Quit exit cleanly
- Preserve save bytes on all failure paths; loading with supported presentation adaptation followed by resaving must work with mods both on and off

The implementation audit settled the constrained legacy recovery policy and added detailed rejection evidence. Ambiguous old saves require re-enabling the original mods and saving once with source metadata
