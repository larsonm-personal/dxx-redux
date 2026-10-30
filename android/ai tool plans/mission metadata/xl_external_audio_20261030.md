# XL external linked audio and ambient-object foundation

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Initial branch: `xl_more_68_external_audio`, directly from merged
`origin/cmake` at `7bb819eeb1e6e6840a20b0c4ffd8eb3d86cede05`.
The new ten-hour block starts October 30 at 05:02:37 UTC and runs through
15:02:37 UTC. Commits, pushes and stacked draft PRs remain authorized; no merges
or autonomous level navigation. Keep implementation owned by `XL_compat` with
minimal core hooks and a separate Redux runtime path.

## First increment

1. Execute actual pinned external object-sound creation, lookup and frame
   consumers. Establish sample identity, duplicate behavior, looping, ambient
   gain, custom controls and disabled/stopped lifetime behavior. Reproduce any
   source defects before choosing an explicit correction.
2. Bind external linked owners to prepared, owned WAV resources rather than
   transient paths or unchecked sample numbers. Preserve source spatial and
   priority behavior while protecting ordinary sample identities.
3. Extend saved-owner validation and earned playback restoration. Reject
   malformed, foreign and mismatched resource/owner/channel bindings atomically.
   Keep effect-object admission blocked until its actual runtime is ready.
4. Verify actual mixed output, moving/disabled/deleted/reused object lifetimes,
   pressure, pause, disk/rewind and stock transitions with shared native/Android
   controls and pinned-source comparisons. Publish evidence and inspect CI.

## Following work in this block

Continue into sound-effect object loading, activation and lifetime ownership once
the external-owner foundation is qualified. Original resource resolution alone
does not qualify gameplay. Advance admission only when the actual consumers,
saved state and original requirement inventory support it. Static explosion
ambience and classic replacement installation remain explicit dependencies.

Inspect CI for duplicated setup/validation or avoidable repeated work. Preserve
required membership, independent oracles, failure detection and timeout bounds;
measure a proposed speed improvement and retain equivalent coverage.

Before ending the block, audit published branch/PR state, local verification,
CI results, retained limitations and all changes against this plan. Do not claim
campaign completion from controlled fixtures or mark unfinished work complete.

## First increment verified

`xl_more_68_external_audio` implements prepared external resource ownership,
separate ordinary/external identities, source-qualified creation and atomic
saved-state validation. The source lookup defects are reproduced before their
explicit repairs. Windows/Linux compare 1,440 fresh creations per level format;
native persistence and exact Redux isolation pass. Android passes all 101 steps
with 119 output checks, including eight external cases, and restores all 26
protected files. Pumo's 644 ambient references bind to 41 prepared owned WAVs at
both rates. See [external audio evidence](../../XL_compat/evidence/external-audio.md).

Sound-effect object loading and enabled-state consumers remain blocked and are
the next gameplay dependency. Before that increment, measure reducing the PCM
oracle's repeated preparation of unreferenced samples. Keep every captured-sample
comparison, source oracle, test member and failure bound.

## CI cost increment verified

`xl_more_69_audio_test_cost` removes conversion of unreferenced samples from the
shared PCM oracle. Windows/Linux checks and all 101 Android steps still pass,
with unchanged 119 output cases and restored protected files. The Android run
takes 318.795 seconds versus 444.955 before; its linked phase falls from 74.151
to 34.976 seconds. Timings and measurement limits are recorded in
[audio test cost](../../XL_compat/evidence/audio-test-cost.md). No runtime engine
behavior, source oracle, CI membership or timeout changes.

The next object-runtime survey confirms that Pumo's 644 sound objects are all
type/id/control/movement `20/2/0/0`; 611 start enabled and 33 disabled. Its wall
enable/disable actions have 56 sound-object target references alongside 92
particle-object references. Activation must therefore preserve authored target
order and independent unsupported particle requirements. Source volume loading
also scales the serialized integer by `65536 / 10` with float rounding; decoding
the raw payload does not yet qualify that runtime behavior.

## Ambient-object increment in progress

Branch `xl_more_70_ambient_objects`, in the same worktree, builds on #219.
Qualify the actual source loader's volume and filename conversion first, then
implement authored sound-object state separately from Redux object unions.
Retain real XL object identity and indices, with a non-rendering native shadow
and narrow collision/lifecycle hooks where required. Do not widen Redux network
or replay admission as a side effect. Verify the source's ordered effect-target
activation, initial disabled objects, permanent linked ownership, removal/reuse,
and atomic disk/rewind state before admitting sound objects. Particle, lightning,
waypoint and other independent requirements remain blockers.

Extend existing source, native and Android integrations rather than adding
duplicate runners. Exercise actual loaded objects and trigger consumers, actual
PCM and saved state, plus stock transitions. Survey original Pumo requirements
again after the implementation; resource preparation alone is not playability.

### Local progress, not publication qualification

The branch now has XL-owned ambient definitions/lifetimes, source float volume
conversion, normalized object identity with a non-rendering native shadow,
deferred spatial owners, enable/disable targets, and schema-24 saved activation.
Stopped loops are checked against their authored resource at trailer restoration.
Only `d2/main/fvi.c` changes outside XL code: narrow checks prevent effect objects
from entering Redux's collision table. Redux type/network/demo bounds are intact.

Focused Windows and Linux controls pass with actual 23/40 and 27/40 loads. They
check eight PCM outputs, initial disabled objects, ordered wall targets, mixed
wall/object targets and their controller back-pointers, malformed state,
earned trailer restoration, a positive ordinary-object collision control,
signature reuse and real deletion. The existing three effect tests pass,
including atomic rejection/recovery for still-unsupported particles. Source
reader controls compare 1,055 volumes and four overflow rejections. The original
Pumo reader comparison passes all 14 levels, 3,182 object records and 795 effect
payloads. These results do not establish original mission playability.

Before marking ready for review: add actual-source activation comparisons, attached
object-trigger controls, Android PCM/disk/rewind coverage, malformed full-trailer
cross-binding controls, and relevant full native/stock regressions. Audit legacy
sound-layout coverage and original requirement outcomes. Update evidence and
EXTENSIONS only with those results. Publish the current checkpoint as a draft
at the user's request to start CI; these checks remain outstanding. Artifacts are under
`temp/xl-ambient-audio-20261030`; the latest Linux verification has a retained
`verify-ambient.sh` runner. Existing Android fixture hashes need updating after
the final fixture is fixed, since the package now contains two ambient levels.

### Initializer comparison and first CI follow-up

The current checkpoint is published as draft PR #220, stacked on #219. Its first
public XL CI run found an obsolete inventory assertion that rejected every
negative effect target. The corrected assertion checks supported early-stop
admission and preservation of the negative target in the owned source; separate
particle/lightning rejection controls remain intact.

The pinned `SetSoundSources` function now consumes effect-object storage in its
source adapter. Actual pinned list insertion checks ordering, and ten controls
cover enabled/disabled states, effect-ID filtering, zero/positive volumes and
literal `.wav` appending. Native receipts from actual 23/40 and 27/40 loads match
the initializer's arguments and the separately qualified creation semantics.
This composes with the source reader's volume/name checks and the existing
external creation/PCM controls; it does not replace them.

Windows source/inventory/effect contracts and the full named/ambient audio
integration pass. The corresponding Linux contracts and full audio integration
pass in 54.64 seconds, including 53.91 seconds for the runtime member. These
runs include scopes, persistence and stock isolation, but do not yet qualify
Android ambient objects, attached activation or legacy ambient runtime layouts.
Artifacts use the `ambient-source-*` prefix under the existing artifact root.
