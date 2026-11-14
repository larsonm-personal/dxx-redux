# Incoming XL resources tranche, November 13, 2026

User invocation starts a 15-hour tranche at approximately 07:30 PST, ending by
22:30 PST. Stop earlier only for weekly quota exhaustion or completion of the
tranche goal including its qualification and handoff. Work sequentially without
agents. Reserve 19:30-22:30 for final validation, CI fixes and delivery; publish
coherent locally tested checkpoints throughout implementation.

## Goal and baseline

Connect retained incoming HAM definitions to real incoming resource preparation
and a bounded successful publication/discard path. Equal-count target B must use
B's definitions, origins and mappings while A remains usable. Moves, malformed
input, discard and future live behavior need canonical controls. A disconnected
API or passing preparatory test alone is not completion. Pursue full G2 as time
allows, without claiming atomic world restore from a resource-only boundary.

Start: clean `0af3f5360c64fcc50da106fd18efe9d47d2ee3b7`, matching published PR 473;
immediate parent branch `xl_gameplay_20261112_07_handoff`. Refreshed origin and
live checks: selected regression owners pass, sampled readiness still fails 4/8.
All prior PRs remain open and non-draft. Three stashes and failed evidence are
preserved. Ubuntu responds normally; approximately 162 GiB free. No cleanup is
needed. New branch: `xl_gameplay_20261113_08_incoming_resources`.

## Ownership inventory before editing

| Owner                      | Current dependency                                                                              | Required preparation/publication boundary                                                                                           |
| -------------------------- | ----------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| Native HAM                 | `XlPreparedHam` owns decoded arrays and initialized model payloads                              | Feed the same retained definitions to dependency validation and installation, including non-graph ship/reactor/gauge/cockpit fields |
| Mission definitions        | `MissionDefinitions` copies live tables; `mission_base` has only count guards                   | Explicit incoming constructor and owned payloads; no equal-count inference or borrowed outgoing definitions                         |
| Source lookup              | HXM/POG/model/audio helpers use global mounted mission scope                                    | Read target through retained descriptor/archive scope and retain origin-bound bytes; do not install B to inspect it                 |
| Bitmap/sample dependencies | `ObjectDependencies::bitmap` pages live PIG; `sound` reads `GameSounds`                         | Establish explicit bank ownership or prove immutable shared-bank identity; avoid changing A during target dependency checks         |
| HXM/model publication      | Existing `install_hxm` allocates maps and sometimes replacement models                          | Prepare fallible work before publication; transfer retained native payloads exactly once and preserve repeated-load behavior        |
| POG/render preparation     | Existing POG install can allocate; clock/HUD checks inspect live tables                         | Use target mappings and preallocate required native/renderer ownership before committing bounded resources                          |
| Mission/world/save         | `mission_commit_prepared` invokes mutating native load; `birth-atomic` changes energy 91 to 100 | Keep full G2 open until geometry, objects, runtime, XL state, epoch and RNG validate and publish together                           |

## Execution and qualification

1. Reproduce the retained late-rejection defect using fresh derived output.
2. Trace target lookup and bank consumers; implement the smallest complete
   incoming resource context through existing native codecs and resource owners.
3. Extend canonical controls with equal-count different targets, retained byte
   identity, moves/discard, malformed inputs and a real successful publication.
4. Publish each coherent locally tested layer early in the authorized fork and
   monitor/fix applicable CI while continuing independent work.
5. Run Windows/Linux runtime and assertions-enabled owners, ordinary developer-OFF
   live-capture versus clean playback, relevant Android/reference owners, scoped
   formatting and rebuild/retest materially changed source.
6. Preserve catalog identities and join blocker evidence only by verified full
   identities. Interim single-player denominator 1,110, threshold 999; full
   playability numerator remains unknown.
7. Finish with exact-candidate terminal public/private results, explicit skips,
   source/binary/input identities, updated ledger, report, next plan and kickoff.
   Commit/push handoff and qualify its CI. No merges, auto-merge, queueing or
   upstream writes. Missing external prerequisites remain incomplete gates.

The continuation report, continuation kickoff and G2-G7 acceptance sections in
`xl_gameplay_execution_20261110.md` remain applicable. This tranche does not
inherit the original unlimited-duration instruction or shorten itself merely
because a partial helper passes.

## Bank ownership implementation boundary

After PRs 474-476 connect retained definitions, extensions and isolated mod/model
sources, dependency walks still page `GameBitmaps` and inspect `GameSounds`.
The next slice retains the selected qualified base PIG/sound bytes from their
captured startup origins, uses shared native header interpretation to build
independent records, and validates RLE payloads through the existing bounded
codec. Incoming graphs must not fall back to installed banks. Actual level
preparation chooses the target inventory palette; absent target palette context
must remain explicit. Base-source hashes still enforce the existing qualification
policy, and private archive reads do not change logical mission lookup.

Native `piggy.c` edits are justified by keeping header interpretation shared with
normal bitmap/sound registration. New resource ownership, source-origin policy
and dependency binding stay in `android/XL_compat`. This does not yet promise an
atomic replacement of installed renderer/audio banks or whole-world state. POG
clock/HUD checks, publication allocation and level/source context remain separate
boundaries to finish or explicitly constrain before tranche acceptance.

## Retained-owner shutdown ordering

The fresh Linux Debug water owner passes functional assertions but emits an
untracked native free at process exit. A GDB receipt shows the memory tracker's
exit sweep runs before the static prepared-resource owner destroys an uninstalled
`NativeModel`. Register prepared-resource retirement when it first enters the
global owner, after native memory initialization, so its native allocations are
released before that sweep. This addresses owner lifetime rather than suppressing
the tracker. Verify the same canonical water path and retain the failing trace.
