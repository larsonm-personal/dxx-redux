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
object-trigger controls, Android PCM/disk/rewind coverage, and relevant full
native/stock regressions. Audit legacy
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

### Full-trailer rejection and authored target normalization

The shared ambient integration now sends 14 malformed complete trailers through
the real restore entry point. Four stopped-owner mutations remain valid to the
generic audio validator but violate the object's authored resource, gain, range
or loop bounds. Ten more corrupt the ambient lifetime table or its schema. Every
rejection preserves a byte-for-byte recapture of the complete owned trailer;
valid restoration then recovers earned activation and playback. Both native
formats pass on Windows and Linux.

Source review also found a loader distinction that was previously missing:
`CTriggerTargets::Check` removes out-of-range object indices before dispatch.
The later `CheckTriggerInfo` pass additionally removes negative sides other than
`-1` from wall triggers, swapping in the last target. Object-attached lists skip
that second pass. This differs from reaching a valid ordinary object during
enable/disable dispatch, which stops the remaining action targets.

An XL-owned helper now applies those source loader rules to translated wall
actions and loaded object-attached actions. The raw inventory remains unchanged.
4,096 comparisons against the actual pinned source cover order, index bounds,
negative sides and both list kinds. The real ambient fixture now contains invalid
indices and a `-2` wall target, so native integration also checks the normalized
loaded lists and subsequent dispatch. Windows full audio integration passes;
Linux full audio integration passes in 54.76 seconds. These changes do not yet
replace the pending source activation or actual attached-removal controls.
Artifacts use `ambient-trailer-*` and `ambient-target-*` in the existing root.

### Activation comparison checkpoint for CI

Loaded receipts now compare four wall actions and two actual attached-object
removals per format against the pinned XL enable/disable consumers and
post-action flags. The checks cover mixed targets, ordinary-object early stops,
permanent reset, alternate/disabled flags and activation after owner removal.
Windows full audio integration passes. Linux effect/source and full audio
integration pass in 55.57 seconds. Artifacts use `ambient-activation-*` in the
existing root.

Android diagnostic commands for ambient PCM and disk/rewind checks are included
in this checkpoint, but the automation script does not yet invoke them and
on-device verification remains pending. Fixture hashes and transition assertions
now match the four-level package. The Android debug x86_64 APK builds successfully
after adding the existing named-sound implementation to the metadata library's
link inputs. Scoped formatting, diff checks and both automation catalogs pass.

Publish this checkpoint on `xl_more_70_ambient_objects` / draft PR #220 at the
user's request to start CI. The working checkout is
`C:/local/dxx-redux/temp/xl-batch2-20261027`; the main checkout remains on `cmake`.
Android ambient automation/runtime checks, full native/stock qualification,
legacy ambient runtime layouts and refreshed original mission evidence remain
outstanding. This checkpoint does not establish original mission playability.

### Ambient qualification completed within the recorded scopes

The shared ambient integration now runs loaded 15/33, 15/34, 17/36, 23/40 and
27/40 profiles. Older fixtures encode the source's default-enabled sound records
and legacy object/trigger/reference layouts while retaining the original dry
geometry. They omit the separately unsupported legacy CLR override. Actual
source object and attached-trigger readers match the derived legacy records;
all five profiles compare initializer owners and real wall/removal activation
against pinned source consumers. The existing audio test owns these checks.

Windows full audio integration passes. The fresh Linux lane passes all 106
required tests in 389.08 seconds, with 16 format witnesses and three modern
source levels in its acceptance receipt. Its audio member passes in 73.23
seconds. The initial sweep's sole failure was an incomplete synthetic inventory
in the high-trigger endpoint test; supplying its actual segment/object bounds
restores that test without weakening source validation or endpoint assertions.

Android passes all 144 steps in 467.970 seconds, including 145 PCM output checks,
24 ambient cases, 1,812,992 checked samples, and both disk/rewind paths. All 26
protected files are unchanged after restoration. The earlier listener-pan test
setup error was corrected by synchronizing against the restored real viewer
before recording earned state. Timeouts remain unchanged. Both catalogs and
scoped formatting pass.

The current Pumo inventory retains all 644 ambient records across 14 levels and
no longer reports ambient-sound blockers. Thirteen levels retain independent
decoded blockers; the end level's empty inventory blocker list is not proof of
resource admission or gameplay. See
[ambient-object evidence](../../XL_compat/evidence/ambient-objects.md) for exact
counts, binary hashes, limitations and retained artifact names.

Next in the remaining ten-hour block: audit the static-explosion audio consumer
and its required object/render lifecycle, then continue toward faithful classic
WAV replacement installation. Keep particles/lightning and independent gameplay
requirements explicit. The measured PCM-oracle cost reduction in #219 is green
in all of its workflows; further CI trimming still requires equivalent coverage
and measured evidence. End-of-block audit remains outstanding.

### Classic replacement consumer

Continue in the same checkout on `xl_more_71_classic_replacements`, stacked on
the published ambient branch. The static-explosion audit found dependencies on
powerup animation, additive rendering, lighting and collision behavior in
addition to `SetSoundSources`. Keep that consumer together with its object
lifecycle rather than admitting an audio-only approximation.

Install the already-owned classic WAV selections into the shared immutable bank.
Verify source startup decisions, output conversion, gameplay entry points,
normal/secret folder changes, disk/rewind restoration, backend reopen and Redux
transitions. Retain malformed/foreign/custom-bank rejection and unchanged native
sample storage. Extend existing integration owners rather than adding duplicate
build jobs. Pumo original resource audits remain component evidence, not a
playability claim.

The replacement tranche now passes full Windows audio integration, all 106
required native tests (379.02 seconds), and both Android scope/classic scripts
(65 and 35 steps). Android compares 237,568 PCM samples in 26 output checks,
including 14 replacement cases, and restores all 234 protected files unchanged.
Existing owners, source oracles and timeout bounds are retained. Pinned startup
comparisons reproduce the mixed-bank empty-buffer defect; production explicitly
uses the owned WAV bytes and actual device rate. Native checks also cover finite
completion, permanent loops, stale saves after valid same-path WAV edits, and
recovery when the original bytes return.

Original Pumo complete-bank and converted-output receipts match across Windows
and Linux for all 1,988 selections and 196 WAVs at both output rates, with an
independent Python audit. This does not qualify original level callbacks or
gameplay. See [classic playback evidence](../../XL_compat/evidence/classic-playback.md).
Publish this tranche from the same disclosed checkout; no D1/D2 core changes.

Next, measure repeated stock conversion during partial replacement preparation
as a possible CI/load-time improvement, retaining fresh content validation and
all existing checks. Then return to the static-explosion object lifecycle and
other remaining single-player consumers. The ten-hour deadline and final audit
remain unchanged.
