# Expanded readiness recovery plan

## Baseline and scope

Design only; no engine behavior or admission rules changed for this plan.
The corrected local measurement is 53/64, with eleven load failures and no
simulation failures. All cases ran in 284.016 seconds; the original eight passed.
This is a broader coverage deficit, not evidence of eleven regressions.

Evidence: `temp/xl-readiness-expanded-20261116/local-64-02/results.json`,
per-case engine logs and `qualification.json`. Native source:
`392b22c261f255c199d334e718fbf3154889e99c`; measurement implementation:
`d56d17080c202aebf7985685c1619c08e2fda877`. The frozen cohort and original inputs
remain unchanged. PR 547 is now merged, so the next admitted candidate can
exercise the deployed expanded private controller. Hosted 64-case results are
not yet established by the evidence used here.

Local reference source inspected: `C:/local/d2x-xl` at
`11038eb53dd86f3bcdc027dddd3f93c60f02536e`. Before implementing a behavior,
verify the relevant reference file hashes against the existing qualified pins.

## Observed failure map

Rows are disjoint by level; mechanisms can overlap. These are observed blockers,
not proof that no later blocker exists. Admission logs truncate some issue lists.

| Family                                    | Original cases                                 | Current evidence                                                                                                           | Main uncertainty                                                                                     |
| ----------------------------------------- | ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Image and animation resources (3)         | Boiling Point 1, Boiling Point 3, Warp Shock 4 | Particle TGA rejected; scoped door strip playback unqualified; missing wall-animation dependency, respectively             | Actual image header/selected resource, door clock ownership, and correct wall-clip bank/index        |
| Equipment and smoke-trigger semantics (2) | Boiling Point 2 and 5                          | Equipment IDs 7/8/9/24 and further requirements; level 5 also has wall type 22 and object type 21 triggers                 | Full producer masks, source eligibility rules, trigger state effects and truncated requirements      |
| Ambient sound and segment ownership (2)   | Sphere 3, Warp Shock 1                         | Sound record 536 or 270 has negative volume or empty filename; Warp Shock also has segment owner/group 0/-1 at segment 897 | Which sound field fails, whether source treats it as inactive, and single-player ownership semantics |
| Wall camera-trigger semantics (2)         | Sphere 5, Warp Shock 3                         | Trigger type 16; Sphere records 11-14, Warp Shock record 33                                                                | Wall-trigger dispatch versus camera-object binding; debug/release and bookkeeping behavior           |
| Signed boss profile (1)                   | Sphere 4                                       | Object 184 requires boss profile -21                                                                                       | Negative profile qualification, level-ending policy and retained boss state                          |
| Serialized object layout (1)              | Boiling Point 4                                | Offset 830889, object 4: movement layout 255                                                                               | Authored sentinel, decoder alignment/version problem, or malformed original record                   |

Important distinctions:

- Missing optional ambient output messages precede some rejections but are not
  the fatal cause reported for those levels. Do not treat every warning as a
  readiness blocker or assume more asset provisioning solves these failures.
- `admission.cpp` explicitly rejects empty sound names/negative volume and a
  list of equipment IDs. Inspect actual decoded fields and source consumption
  before classifying them as malformed content or implementing new gameplay.
- Reference `trigger.h` names 16 CAMERA, 21 SMOKE_SPEED and 22 SMOKE_DENS.
  Reference `CTrigger::Operate` has explicit empty action cases for smoke types
  20-24, followed by bookkeeping. CAMERA has no action case and reaches the
  default diagnostic path. This is not permission to drop these records or to
  treat debug and release behavior as interchangeable.
- `runtime/boss_profile.h` already supports positive 21 and some negative
  profiles. It preserves the raw sign for level-ending behavior. Profile -21
  may be a bounded qualification gap; it does not automatically require a new
  boss AI implementation, and simply adding it to the allowlist is insufficient.
- The TGA diagnostic does not establish RLE compression as the cause: indexed
  color, image type, bit depth or resource selection can produce the same error.
- The movement byte 255 does not prove a new movement mode exists. Verify record
  boundaries against the source reader before consuming or skipping any bytes.

## Ordered work

### 1. Establish complete, reproducible diagnostics

Run the deployed 64-case private CI measurement on a fresh admitted candidate
and retain the exact source/controller/input identities and real hosted timing.
Keep the original eight-case result separate.

Produce a complete machine-readable blocker list for all eleven failures using
the existing native inventory and admission machinery. Record every feature,
record index, actual field value, asset origin and rejecting stage. Preserve
the short public receipt; keep detailed original-content diagnostics private.
For resource failures, include selected filename, header, clip ID and retained
bank counts. For Boiling Point 4, compare byte offsets and object boundaries with
the qualified source reader. Do not introduce a second format parser.

Exit: every visible blocker has an owner and reproducer; truncated lists are
expanded, and parse uncertainty is explicit. Start the Boiling Point 4 audit
early because later feature counts cannot be trusted until decoding is correct.

### 2. Qualify existing semantics before building new systems

Use small, separately reviewable changes for:

1. Wall camera-trigger dispatch and smoke-trigger behavior. Compare actor gates,
   operated state, delays, one-shot/alternate/disabled/autoplay flags, chained
   activation and save/restore against the reference. Preserve camera bindings
   and trigger indices. Do not implement a camera renderer just because a wall
   action is named CAMERA, or fabricate smoke behavior absent from the source.
2. Empty/negative sound records and neutral segment ownership. Verify decoded
   original values and source behavior. If inactive data is legal, preserve its
   authored identity and lifetime while reproducing the source's consumption
   rules. Keep malformed data rejected. Treat multiplayer ownership separately
   from any demonstrably inert single-player state.
3. Boss -21. Reuse the existing boss machinery and raw signed profile. Prove
   activation, attacks/spawning, damage/death, trigger effects, non-ending versus
   ending behavior, save/load, continuation and simulation RNG equivalence.

These have promising reuse opportunities, not guaranteed quick fixes. They
touch five otherwise separate cases, plus some Boiling Point 5 blockers; further
blockers may become visible after each change.

### 3. Complete the resource and animation paths

- Boiling Point 1: establish the selected particle image and actual encoding.
  Correct resolution or extend the existing owned image decoder only for
  source-supported formats. Test dimensions, payload bounds, alpha, orientation,
  atlas frames and malformed input. Do not transcode the authentic asset merely
  to make the measurement pass.
- Warp Shock 4: trace the wall clip through incoming mission definitions and
  retained resource banks. Distinguish a wrong index/count/bank from genuinely
  absent content. Preserve transactional staging, failed-load discard and
  independent ownership; do not patch live global tables or insert a dummy clip.
- Boiling Point 3: implement and qualify scoped door-strip playback using the
  retained texture/animation owners. Cover opening, closing, reversal, linked
  walls, frame selection, collision state and restore. Include rendered evidence
  on host and Android, not only headless loading.

Exit: each original asset resolves and behaves correctly, with negative controls
for invalid assets and recovery after a failed stage. Keep policy in
`android/XL_compat`; native edits should be necessary codec/state-owner changes
or thin integration hooks.

### 4. Finish producer semantics and the serialization outlier

For Boiling Point 2/5, audit every equipment bit and the reference producer's
eligibility rules before deciding which pickup features must be implemented.
Prove selection order/RNG, scheduling, generated-object identity, pickup effects,
limits and mode restrictions, save/load and continuation. If the source filters
reserved entries, reproduce that rule with evidence; do not arbitrarily clear
the authored mask. Qualify any genuinely emitted unsupported pickup behavior
before admitting it. Level 5 is a multi-blocker integration case.

Finish the early Boiling Point 4 decoder audit in the existing reader. Require
exact record endpoints and evidence for sentinel/dead-object/version behavior.
Never recover by scanning for a plausible next record, guessing a payload size
or silently skipping the object. Re-inventory the entire level after the fix,
then schedule newly exposed requirements by their shared owner.

These are the highest-uncertainty workstreams. Do not promise a fixed number of
PRs or tranches before complete masks and record boundaries are understood.

### 5. Validate convergence without changing the target

For each change, first run the affected original cases and meaningful negative
controls. Before accepting a checkpoint, rerun all 64 and the original eight on
the ordinary developer-OFF target, with Windows/Linux coverage and real private
CI. Use targeted source comparisons and save/replay tests for stateful changes;
use rendered host/Android checks for image/door changes. A 120-frame pass cannot
substitute for exercising a trigger, killing a boss or collecting a pickup.

Retain authentic assets, frozen membership, the 120-frame criterion, per-case
watchdogs and honest failures. Record blocker counts as well as passed levels:
removing one of several blockers is useful progress even before a level passes.

Successful loads may cost more than today's early rejections. Re-measure the
ten-minute job as coverage improves. If necessary, optimize verified staging or
build reuse; do not drop hard cases, shorten the proof or label untested cases
as passes. Keep slower behavioral qualification outside the fast readiness job.

Exit: 64/64 on the unchanged cohort locally and in trusted private CI, original
8/8 preserved, and behavioral evidence for every newly admitted capability.
This establishes expanded load/tick readiness, not campaign completion or full
rendering/playability qualification.
