# Owned XL texture-change actions

Wall and supported object type-19 actions now change the addressed side's base
and overlay textures through the XL runtime. They use the existing scheduler,
master chains and object lifecycle. Redux keeps its existing alternate. This
does not qualify an original campaign or remove unrelated effect/object gates.

## Reference behavior

The source comparison executes `CTrigger::DoChangeTexture`, `CSide::SetTextures`
and `CTrigger::PrintMessage` from the pinned XL commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Independent side storage and audio
adapters supply the environment. The existing trigger-source test owns these
2,112 additional comparisons; no separate source process is added.

The packed value's low word selects the base texture. A negative high word
retains the overlay; otherwise assignment truncates it to the source's 14-bit
overlay field. The two orientation bits remain unchanged, including when the
overlay is cleared. The low word is not an unchanged-value sentinel. Only
addressed sides change: the connected side is not implicitly updated. Empty
and duplicate target lists retain the source behavior. The normal message is
suppressed by `TF_NO_MESSAGE`.

`audio.Update()` invalidates the reference's sound router after every operation,
including an empty list. The current integration invalidates only Redux's
separate sound-distance cache. A controlled blocked route is cached in both
domains; the authored texture change must make the sound route recompute while
the gameplay cache retains its earlier result. A future owned XL spatial router
must preserve the same invalidation at its own consumer.

## Admission and actual consumers

All target coordinates and used texture IDs are validated before applying an
action. Admission includes textures that only become visible later, together
with their effect and destruction dependencies. A native counterfactual removes
the type-19 actions from otherwise identical geometry: an unavailable effect
on the future-only material must reject the authored-action version and admit
the control. Bad base/overlay IDs, targets and object-action resources reject
with record/feature information without changing the active sides.

Cleared rotated overlays exposed consumers that treated orientation bits as an
actual texture. XL rendering, destructible-overlay checks, player/weapon
contact and water-material classification now test the texture portion. Stored
orientation remains intact. Direct FVI rays check opaque and super-transparent
materials at all four orientations. Actual player and flare contacts distinguish
a cleared rotated overlay from a real water overlay. UVs and authored light
records remain unchanged by the action, matching the source assignment.

Native 23/40 and 27/40 fixtures exercise wall crossing, immediate/master actions,
multiple targets, negative-high-word retention, 14-bit assignment, disabled and
empty actions, one-shot flags, separate wall/object autoplay effects and actual
owner removal. A six-second delayed action is saved after one second, disturbed,
restored through stock gameplay and checked immediately before and at its earned
deadline. Texture IDs, rotation, UV/light records and pending phase survive.

Android uses the same event/collision controls and the normal game renderer.
A fixed controlled pose observes the addressed portal; no navigation is used.
GPU readback compares stable overlay frames, an authored clear, a zero-overlay
control and restoration of the original orientation. The scene must show a
visible material change. Rewind, ordinary disk saves, stock recovery and a
second level layout repeat those observations.

## Original Pumo result

The unchanged HOG SHA-256 remains
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`.
Fresh inspection removes exactly fifteen type-19 rejections: one in `pmlev1`,
seven in `pmlev2` and seven in `pmlev3`. The member inventory, decoded level
inventories and every other blocker are identical. All fourteen levels remain
blocked. Nine levels retain other unsupported wall-action kinds; twelve retain
effect-target requirements. This is a requirement delta, not original event or
campaign qualification.

## Validation

The public owners pass all 43 Windows and 46 Linux contracts, with test phases
of 10.98 and 7.18 seconds. Both Windows native layouts pass after formatting,
including the future-dependency, separate sound-cache and actual contact checks.
The full private Linux owner passes all 104 contracts in 325.935 seconds,
including sixteen original-format outcomes and three full modern source-reader
comparisons. The native texture owner itself takes 6.85 seconds. Windows and
Linux independently generate identical derived HOG bytes; the descriptor now
uses explicit LF output on both hosts as well.

The final Android build passes all 67 steps in 134,942 ms, run
`4820a2ad57844bc091c74c2bfacbf334`. It checks 43,008 pixels across seven scenes,
with 1,024 visibly changed pixels per observation and 24 saved-state
verifications. The before/after hashes of all 26 protected application files
match. Both automation catalogs and direct PowerShell analysis pass. The mixed
formatter/linter passes for all changed code.

After normalizing the descriptor to LF, both native layouts pass again; the
focused Linux owner takes 8.59 seconds. The portable fixture passes Android's
same 67 steps in 122,048 ms, run `93929bc5f07a473e904fa59f55bcb703`, with the same
43,008 pixel checks and identical protected-data hashes. The independently
generated HOG hash is `6e561cc28ed0b0641e8501ac8c6033baa4dc974f01205d6a3fd18008cbebe22f`;
the descriptor hash is `cf87743d75f566c8dbc227d25b7c094d84d9779027f594586ee81cde0998e8ae`.

Reproduction owners are `xl_trigger_flag_reference`, `xl_texture_actions`, and
`android/tests/test_xl_texture_actions.ps1`. The new native owner runs both level
layouts in one engine process and is required by the private selection, bringing
it to 104 contracts. Public membership remains 43 Windows / 46 Linux. Artifacts
and the exact original blocker delta are under `temp/xl-texture-actions-20261029`.
