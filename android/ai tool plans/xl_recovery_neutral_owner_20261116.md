# Neutral segment ownership recovery

Warp Shock 1 authors segment 897 with owner 0 and group -1. The source defines
owner -1 as always neutral and owner 0 as neutral; owners 1 and 2 belong to teams.
Both the inventory gate and native adapter previously rejected owner 0. Accept
the two neutral identities while preserving them in the retained source. Keep
team and unknown owners rejected and keep the global XL multiplayer guard.

## Source audit and scope

Audited `include/segment.h`, `main/segment.cpp`, `main/producers.cpp`,
`objects/updateobject.cpp`, `gamemodes/entropy.cpp`, `render/renderlib.cpp`,
`network/multi.cpp` and `objects/powerup.cpp`. All eight match the qualified local
reference copy. Exact SHA256 values are retained in
`temp/xl-recovery-20261116/neutral-owner-01/reference-hashes.json`.

Capture, team textures, winner detection and ownership restrictions on station
benefits are Entropy-mode behavior. They are not enabled by admitting neutral
metadata in single-player. One consumer remains relevant outside Entropy:
equipment production copies the segment owner to the created pickup. The
existing retained maker already carries that owner and writes it to the native
object; preserve and test that behavior rather than normalizing owner 0 to -1.

No native engine code changes are required. The XL inventory and adapter gates
now agree. A read-only XL owner accessor exposes retained identity alongside the
existing group accessor, allowing loaded-world tests to observe the actual owner
before and after stock transitions and disk restore.

## Local evidence

Private root: `temp/xl-recovery-20261116/neutral-owner-01/`.

- Debug/reference and ordinary developer-OFF builds passed. The first Debug
  build caught an incorrect test accessor; the retained failed log precedes the
  corrected build. Existing C4805 in unchanged `changing_light_test_records.h`
  and shutdown MEM warnings are not claimed resolved.
- `inventory-02.log`: inventory suite passed, including all 256 group bytes in
  three layouts, both neutral owners and rejection of team/unknown owners. Both
  adapter paths retain owner/group identity. The first run caught the second
  adapter guard; it is retained as failure evidence.
- `layout.log`: 132 levels, 132 segments, 792 sides, 17,576 source comparisons and
  1,056 vertex-brightness maxima passed, including neutral owner/group records.
- `damage.log`: 230,496 extracted source leaf/caller/Set/Update comparisons passed
  across both neutral owner values, with separate wide-value controls.
- `boss`: 7.413 seconds, exit 0. A derived authored owner-0 room and a distinct
  owner--1 room retain their groups and identities through loading, boss travel,
  stock transition and disk restore; corrupted cross-group state is rejected.
  Multiplayer remains rejected. Reference spawning comparisons also passed.
- `equipment`: 9.269 seconds, exit 0. The 210 production samples now include both
  neutral owner values and compare the resulting native creator field with the
  actual source, in addition to timing, selection, allocation and RNG. All 13
  initialization cases passed. The final fixture encoder also exposes owner-byte
  offsets, allowing the existing version-27 fixture to author owner 0 on its
  ammo producer without adding a parser or changing original content.
- `equipment-v23`, `equipment-v25`, `equipment-v27`: final loaded-world controls,
  extracted source comparisons and warm/cold continuation all passed (9.300,
  9.711 and 9.754 seconds). Version 27 retains owner 0 in its cold receipt both
  before and after continuation and verifies the spawned item's creator against
  retained source identity. Versions 23 and 25 preserve owner -1 controls. Native
  object/world/XL state, births and SIM/FX RNG remain part of the comparisons.
- `repair`: 2.251 seconds, exit 0. All 5,760 repair samples and 42 actual-frame
  samples match extracted source for each neutral owner value.
- `warp1`: unchanged authentic Warp Shock 1 passed loading and 120 unassisted
  frames on the ordinary developer-OFF executable. No resource edits, route
  assists, criterion changes or failed-case replacements were used.

The full unchanged budgeted measurement in `neutral-owner-64-01` stopped at its
guard after 318.047 seconds: 52 passed, ten load failures and two untested;
original eight passed. Exit 2 and budget exhaustion are preserved. The two
untested cases separately passed in `neutral-owner-01/remaining`. Across these
runs every case was exercised and Warp Shock 1 is the only newly passing case,
but this is not a completed budgeted 54/64 result. Exact-candidate CI and relevant
Android checks remain pending. Timing headroom needs continued attention.

## Provenance and remaining work

The local binaries include the working-tree patch above parent
`082f4bb31745f085e406cadc9963fb8cd555f550`, not the unmodified parent identified by
the runner's target field. Initial source patch SHA256:
`f61e32df35d078d3bad95ef66e0a6cbace4802b81605e640cfe4d045cc429fbd`.
Initial Debug binary SHA256:
`0430a466648347ec64e3b58dee02567c97751d39b224bf6bcb795a1534d007a0`.
After the authored owner-0 equipment fixture, final source patch SHA256:
`f72d47b9c86ac54af76a0ea23b6eac1154ca4c54e3c4f6e7eb19840f1b705d80`;
final Debug binary:
`d153da9b4b25867db6d76fdaa7abf9413538004eca9c398bcc495e2bbee6bb74`.
That final change affects only test fixtures and checks; production bytes remain
the same as the full ordinary survey.
Ordinary binary SHA256:
`aa8540a0e0f80dfb68628020b70c610878f13d5b263eb1213df492de74cecedd`.

Finish complete budgeted measurement and candidate CI before acceptance.
This is a single-player neutral-owner qualification, not
Entropy support, campaign completion, or closure of the unrelated G2 save issue.
Continue the other failure families in the tranche's ordered recovery plan.
