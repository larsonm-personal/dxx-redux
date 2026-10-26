# No-damage support and Observatory qualification

Objective: verify the serialized no-damage feature, admit by detected requirements,
and qualify the unchanged Observatory fixture only when all required feature and
resource gates pass. No mission-name exceptions and no automated level navigation.
Preserve stock behavior and existing local changes.

1. Audit all reference property consumers and existing native controls; cover
   remaining collisions, real shared-face crossings and pending fusion/guided state.
2. Run controlled matched reference cases with explicit inputs and measured
   outputs, retaining deviations separately from supported semantics.
3. Reconstruct the property from owned source on ordinary load/restore; verify
   save/restore, rejected selection, stock cleanup and multiplayer rejection.
4. Update admission and browser evidence only after feature gates pass. Verify
   unchanged Observatory resources, rendering and lifecycle on the normal APK.
5. Run focused stock regressions, format changes and record evidence/limitations.

Current state: feature-based no-damage admission and the missing ambient rebuild
path are implemented and verified on unchanged Observatory. Native semantic,
reference, persistence and stock-isolation controls pass; normal Android passes
45 steps and its captured scene is visibly rendered. Historical entries below
record the earlier failures and are superseded by the final acceptance evidence.

Artifacts: temp/xl-no-damage-20261025.

## Progress

Added reusable native shared-face controls in no_damage_boundary_test.h, required
by xl_observatory_descriptor. Original segments 33/34 are crossed in both directions
with native physics and original ship size. Paired property-enabled/neutral controls
assert identical position, physics and RNG, and immediate damage/primary/missile
gate changes after crossing. Authored objects are excluded only during this paired
geometry control, then restored; an initial pickup side effect was diagnosed and
removed from the comparison rather than loosening the assertions.

Final native harness build and xl_observatory_descriptor pass (6.06 seconds),
including previous firing, fusion, guided, wall, splash, bump and descriptor
transaction controls. Logs: build-final.log and boundary-final.log. Scoped quality
checks pass. Admission is unchanged and still rejects the original Observatory.

Remaining: matched reference execution, other collision gaps, pending weapon and
property persistence, production source reconstruction/admission and normal-APK
original-mission resource/rendering/isolation verification. Existing reference
executables and pinned source are available locally; prior reference lighting
tests show a reusable exact-source extraction pattern for controlled comparisons.

### Matched reference gates and contact dispatch

Added exact-source extraction with the audited hashes for controlweapon.cpp and
collide.cpp. The compiled reference functions match measured native results for
12 firing-gate cases and four player/clutter or player/player contact cases.
Comparison is part of xl_observatory_descriptor when XL_REFERENCE_WEAPON_CONTROL
is configured. Source adapters provide state and observable bump/audio sinks;
they do not replace the compared gate code or establish full reference-game parity.
The complete descriptor suite passes in gates-contacts.log (7.03 seconds).
Reference outputs are retained in reference-gates.log and native output in
descriptor-gates.log in the artifact directory. Scoped formatting passes.

The reference SetDebrisCollisions implementation was also compiled/executed.
Its actor/debris pairs are disabled at default zero debris lifetime; nonzero
persistent-debris settings enable them. This is a separate optional policy gap,
not evidence that the ordinary protected-segment path must enable debris contact.
No production admission was changed. Remaining work includes pending-weapon and
property persistence, remaining matched behavior cases and original-mission
resource/rendering/stock-isolation verification on the normal binary.

### Original-source persistence

Added private-only qualification to exercise original function-13 levels through
the real translator, resource preparation, owned source and native save loader.
Normal builds return false for this qualification, with no command-line switch.
Translation retains original properties separately from ordinary stock enums.

The unchanged Observatory fixture saves a protected player, charged queued fusion
shot and live guided missile. A real stock new game clears the source state;
restore reconstructs all 26 properties without manual rebinding. Player resources,
position, charge, queues, relative timer and guided signatures/ownership survive.
Rejected level candidates preserve restored properties/position. Subsequent native
frames retain the protected fusion shot, recharge on exit and emit two bolts with
normal costs; guided release and shield damage resume outside protection.

An initial crash was diagnosed with a symbolized Windows harness stack: the second
new game entered stock briefing UI after the first consumed the one-shot intro
skip. The headless fixture now sets that skip before each new game. No gameplay
or save implementation was changed to mask that harness error. The private Windows
diagnostic reports stacks for future native qualification crashes.

Final persistence/descriptor and Konflict controls pass 2/2 (14.56 seconds total),
inventory and original Observatory rejection controls pass 2/2 (5.31 seconds).
Formatting/lint passes. Logs: persistence-final.log, inventory.log,
format-persistence.log. Normal-build verification and remaining production/device
qualification follow; original mission admission is still blocked by default.

Normal Windows game and metadata targets built successfully after these changes.
The normal metadata executable rejects unchanged Observatory with exit 1 and
segment_function 13 diagnostics (normal-rejection.log). This confirms the private
qualification did not silently change normal admission. Native persistence is
verified; production promotion and normal-APK qualification remain unfinished.

### Production admission and final checks

Function 13 passes runtime requirement filtering and is normalized to an ordinary
stock enum while the owned XL source retains protection. No descriptor name or
archive hash is used for admission. Other unsupported functions remain rejected;
the hidden-secret rejection fixture now uses function 12 instead of Observatory.
The browser reports the no-damage feature as supported with singleplayer scope.

Normal Windows game/metadata and normal Android x86_64 debug APK builds succeed.
Native Observatory descriptor/persistence plus Konflict controls pass 2/2
(admission.log); inventory/UI and original-source translation pass 2/2
(inventory-final.log). Rebuilt stock upstream compatibility, classic trigger demo
and mixed-rate audio pass 3/3 (stock-final.log). Both automation catalogs pass.
Scoped mixed-language quality checks pass (format-verified.log).

The first Android failure was a test assertion comparing string representations
100 and 100.0; shield assertions now use introspection's explicit decimal form.
The complete 43-step semantic/lifecycle test passes. Protected damage leaves 100
shields, ordinary damage reduces them to 90, and protection survives both direct
quickload and quickload after a real stock new game. Stock clears XL runtime and
lighting counters; the XL menu marker draws. The wrapper verifies original input
hashes and restores device files with identical before/after SHA256 inventories.

The initial protected-face screenshot was mostly black and is not used as proof
of scene parity. A final fixed camera view is checked separately. No automated
level navigation or full playthrough is claimed.

### Final visual inspection exposed a missing prerequisite

The 45-step Android run passes (59.55 seconds), but the second inspected fixed
view is also black. Source inspection found all 328 vertex and 660 side colors
zero. The reference ReadColor registration predicate leaves bColored false and
GatherStaticLights reconstructs ambient in this case. Our runtime instead shades
from the zero serialized array. This is not a camera-navigation problem, and the
render counters did not establish visual success.

Do not mark this goal complete or claim Observatory is fully supported. Next work
is detected ambient reconstruction with reference comparisons and negative/stock
controls; until then admission needs an explicit unsupported requirement gate.
The D1 gather implementation may be reused only after auditing its D2 assumptions.
Detailed evidence is in XL_compat/evidence/no-damage-reference.md. No full mission
playthrough, universal rendering parity or multiplayer support is claimed.

### Ambient reconstruction closes the visual blocker

ReadColor registration is detected at the reference float precision, including
side colors but excluding texture-only colors. When no color registered, the
normal XL loader gathers ambient using existing method-0 geometry calculations,
owned brightness resources and texture colors. D1 guards now also admit an actual
singleplayer XL source; stock D2 stays excluded. Generated colors are cleared on
stock load and rebuilt on restore. Post-commit construction failures clear the
new XL state rather than leaving partial activation.

A running pinned reference loaded unchanged Observatory. Its 224 sources match
locations/materials, and all 328 ambient colors match within 1.47e-7. The checked-in
reference fixture is required by the descriptor CTest and compared after load and
restore. Pinned ReadColor itself passes 16 matched decoding/registration cases.
Native descriptor/Konflict, inventory, D1 Dragon companion, water arena and rebuilt
stock upstream controls all pass. Normal Windows game/metadata targets build.
The normal Android APK passes 45/45 and its inspected screenshot shows the textured
mine rather than black. Full pixel parity, optional persistent debris and broad
XL fusion-selection policy remain outside this bounded feature qualification.
See the semantic audit for source hashes, exact scope and artifact locations.

### Completion audit against the five planned stages

1. Property consumers and boundary behavior: required native controls cover damage,
   firing/resource accounting, queued fusion/guided state, walls, splash, actor
   collisions, and bidirectional shared-face movement. Final added controls prove
   robot damage/physics/awareness remain and incoming projectiles cross in both
   directions with arrival-based damage/disposal behavior (coverage-audit.log).
2. Independent reference cases: unchanged pinned firing/contact functions match
   12 gate and four contact cases; ReadColor matches 16 registration cases. The
   running reference ambient capture matches all 224 sources and 328 colors.
3. Ownership/persistence: original-source normal loader reconstructs 26 protection
   properties and ambient after a stock mission; queued weapons and resources
   survive. Failed selections preserve active state, and multiplayer rejects.
4. Production integration: no mission-name/hash admission exception; browser
   evidence distinguishes baked colors from ambient reconstruction. Original HOG
   and descriptor are hash-checked by test fixtures only. Normal OpenGL builds
   run both stock and XL; Android has controlled behavioral and visual evidence.
5. Regressions: final native suite passes 4/4, inventory/UI 2/2 and rebuilt stock
   upstream compatibility 1/1. Normal Windows game/metadata and Android APK build.
   Automation catalogs passed after test registration. Scoped quality checks pass.

No mission traversal is required or claimed. Optional persistent-debris settings,
other lighting modes, full guided-flight or pixel-by-pixel parity, and XL's broader
manual weapon-selection refund remain explicitly outside this feature's evidence.

Final normal-APK acceptance: run 76214e2953c343d5b3a4113ffaebdac2 passes 45/45
in 50281 ms, with 328 reconstructed ambient vertices and visibly textured mine
geometry in the inspected capture. APK SHA256 is
41317a2a01c9f84b0edae8918ed77aab81477cd5bdd12bda637af9a6d0c46931.
Device files restored identically. Final code-quality checks and diff whitespace
checks pass. The five-stage bounded support objective is complete; broader XL
features and the explicitly listed parity limits remain separate future work.
