# XL particle effects and Pumo first-level qualification

This twelve-hour tranche starts October 30, 2026 at 16:28:40 UTC (09:28:40
Pacific) and ends October 31 at 04:28:40 UTC (October 30, 21:28:40 Pacific).
Work is in the main IDE checkout, `C:/local/dxx-redux`, initially on
`xl_more_78_particles`, based on merged `origin/cmake` at `c38e4e085`.
The previous worktree remains available for reference and retained evidence;
new implementation belongs in this main checkout. Commits, pushes and stacked
draft PRs continue under the existing authorization. No merges, subagents or
autonomous 3D navigation.

## Intended result and constraints

Advance faithful particle-effect support toward a useful original Pumo Mines
opening-level test. Its current thirteen inventory findings concern twelve
particle objects, not thirteen distinct missing systems. Clearing those findings
alone does not establish valid resources, runtime behavior or mission playability.
Retain feature-based admission and specific rejection messages until the real
consumers are implemented and verified. Do not substitute invisible effects or
mission-name exceptions for support.

Keep owners, source comparisons and diagnostics in `android/XL_compat`. Reuse
existing effect activation, noncollision and lifecycle dispatch where faithful.
Keep D1/D2 and Android application changes small and explain each necessary hook.
Redux remains a separate behavior path; XL settings and random effects must not
perturb ordinary gameplay or its simulation RNG.

## Execution

1. Audit actual pinned XL particle loading, object-to-emitter conversion,
   emission/update, rendering, image dependencies, activation and persistence.
   Inventory the original opening-level particle types and parameters. Record
   source settings, intentional corrections and unimplemented cases explicitly.
2. Establish executable independent source comparisons before admitting new
   runtime behavior. Implement the complete dependency chain in coherent
   increments: owned authored definitions, emitter/particle lifecycle, resources,
   visible rendering and trigger activation. Preserve deletion/signature reuse,
   level transitions and atomic save/restore behavior.
3. Exercise meaningful native and Android integrations, including actual output,
   enabled/disabled transitions, malformed input, persistence, pause and stock
   isolation. Use existing test owners where practical. Reuse verified source
   adapters rather than duplicate broad suites. Preserve independent oracles.
4. Reinspect original Pumo assets and requirements after implementation. If the
   opening level is admitted, verify loading and controlled real consumers and
   prepare a human-testable build with precise limitations. Human exploratory
   navigation remains the user's role. If further blockers emerge, fix them or
   report them honestly; do not call a successful load correct gameplay.
5. Measure and remove clear test duplication or avoidable setup costs in CI and
   local owners. Keep failure detection, assertions and required coverage. Do not
   trade smaller runtime for weaker qualification. Run expensive acceptance
   serially when contention would invalidate timing or cause artificial timeouts.
6. Publish coherent reviewed increments and inspect their CI. Near the deadline,
   stop starting features and finish verification, evidence, publication and the
   completion audit. Record remaining product work without overstating support.

## Evidence and handoff

Use `temp/xl-particles-20261030` for local artifacts after producer retention.
Maintain Git-visible source provenance, tested scopes, negative controls,
limitations and reproduction commands in `android/XL_compat/evidence`, and keep
`EXTENSIONS.md` aligned with actual behavior. Report branch, checkout, commits
and PR links explicitly. Final audit covers intended changes, minimized core
diff, meaningful native/Android checks, optimization evidence and published-head
CI. The twelve-hour work block does not imply complete Pumo campaign support.

## Initial preparation checkpoint

The main checkout is on the new branch. `particle_effect.h` decodes authored
parameters and derives emitter definitions; the existing pinned effect-reader
owner now compares actual source conversion and creation on Linux and Windows
x64. A negative fixed-math control exposed and corrected rounding of negative
directions. Pumo's twelve opening-level emitters are fire (serialized subtype 3),
not waterfall, and all start disabled. Original particle images are available
inside the user's downloaded XL installer, extracted without executing it.
See [particle evidence](../../XL_compat/evidence/particle-effects.md).

No runtime particle admission has changed. Real geometry, random particle
creation/update, visible rendering, activation, persistence and original-level
qualification remain the next work. No source changes outside XL_compat have
been needed at this checkpoint; the plan itself lives under the usual plan path.

## Geometry and fire CPU checkpoint

Pinned face-density and random surface sampling comparisons now pass, including
106 original Pumo face emitters across all fourteen levels. Authored fire CPU
creation and update now match the active source bodies under controlled random
inputs and supplied world lookup/collision outcomes. Shared angle math was
extracted from spatial audio without copying its table; the existing audio
source comparisons still pass. See particle evidence for exact coverage and
adapter limits. Real mine lookup/collision, emitter scheduling, resources,
rendering and runtime lifecycle still remain. No particle admission has changed.

The foundation is committed as `3b7c6cbb6`, pushed on `xl_more_78_particles`, and
published as draft [PR #229](https://github.com/larsonm-personal/dxx-redux/pull/229)
against `cmake`. The next layer uses `xl_more_79_particle_world` in the same main
checkout. Pure world consumers pass actual-source comparisons with supplied
compiled geometry; a fresh Windows/x86 native engine build includes the new
compiler. Real loaded-mine compilation/execution still needs verification.
At the latest CI poll, PR #229's XL contracts and server Rust workflows passed;
the gameplay, engine smoke and LAN workflows were still running. Full CI is not
yet recorded as complete.

## Loaded geometry and test-cost checkpoint

The native world compiler now passes actual-source setup and query comparisons
for stock D2 level one and Neptune: 321 segments and 6,099 queries. The source
adapter independently builds side topology, fixed normals, centers, radii and
spatial grids; every loaded grid candidate list is compared. Windows/x86 native
execution and Windows/x64 and Linux oracles agree. A changed exported normal
fails, stale geometry rejects, and native state and both RNG call counts remain
unchanged. Original Pumo face/particle inventory comparisons still pass, but its
loaded geometry and gameplay remain unqualified.

The existing effect gate now initializes once for all three format versions,
retaining six stock/XL rejection controls and three supported recovery loads.
A three-sample before/after local experiment measured median 6.361 versus 5.603
seconds before adding grid comparisons, about 12 percent less time for this
owner. Required originals validation now requires its source oracle argument.
No workflow or new CTest owner was added; no core D1/D2/application source changed.

Next is actual emitter scheduling and its engine ownership, then resources and
visible rendering, activation and save/load. This geometry checkpoint does not
enable particle admission. PR #229's XL contracts, server Rust and public LAN
workflows have passed; engine smoke and Linux gameplay were still running at the
latest authoritative poll.

The world layer is published as draft PR #230, commit `e50f32159`, on
`xl_more_79_particle_world`, stacked on #229. New emitter work is on
`xl_more_80_emitters` in the same main checkout. Its CPU implementation now passes
2,880 actual-source frame comparisons, 24 drained rings and six zero-capacity
rejections on Linux and Windows/x64, through the existing fire test owner.
World outcomes are supplied in these emitter comparisons; real engine binding
is still required. Source `KillObjectSmoke` stops emission and releases the object-system
association while old particles drain; re-enabling therefore needs a fresh owned
emitter, not a reset of the old live particles.

The five PR-triggered workflows returned for foundation PR #229 now all report
success, including engine smoke and Linux gameplay/save/replay. PR #230 has six
queued workflows, including tooling checks. Full published-head CI audit remains
part of the tranche completion work.

Before binding emitters, preserve actual manager lifecycle: the normal source
uses `PHYSICS_FPS=-1`, `SMOKE_SLOWMO=0`, and a 25-ms SDL-tick gate; system updates
check object signature/deletion, stop old emitters and clean drained systems.
`CDataPool::Pop` prepends new systems to the used list, so manager update order
is newest-first. Audit the effect-object iterator creation order and pause/save
behavior before choosing the engine clock and ownership implementation.

## Emitter ownership checkpoint

The visible `xl_more_80_emitters` branch now connects the fire emitter to native
ownership and wall/object activation. Its existing native effect owner passes
disabled startup, source update cadence, draining/fresh re-enable, recycled
signatures, pause, point jitter, atomic binding rejection and simulation RNG
isolation. Effects remain rejected at admission; these tests inject controlled
objects into loaded Neptune and do not claim original Pumo support.

The source comparison now composes emitter and particle updates with independently
compiled real mine geometry: 720 frames and six drained rings each for stock D2
and Neptune, matching on Linux and Windows/x64. It reuses the existing world
receipt and one engine startup. Only capacity 7 is repeated with real geometry;
the existing supplied-world matrix owns the remaining capacities and rejection
controls. Point jitter separately passes 1,152 actual-source position/RNG
comparisons on both platforms.

One small core hook is necessary: `d2/main/game.c` adds an include and guarded
call after gameplay. The existing trigger-frame hook runs before object movement,
which would delay effects activated by movement until the next frame. All particle
implementation stays in XL_compat, with no D1 or Android application changes.
Evidence records the level-relative SDL clock, source's two authored visits,
newest-first system order and intentional correction of old-system destruction
unbinding a newer replacement.

The Windows/x86 native build, Windows/x64 and Linux source owners, shared audio
source controls, and the registered native effect gate pass. Next is resource
ownership and visible fire rendering, then persistence and Android qualification.
The twelve-hour tranche remains active; this checkpoint does not enable Pumo.
