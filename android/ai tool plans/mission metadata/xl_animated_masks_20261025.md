# Mission-owned animated binary coverage

Branch `xl-animated-masks` starts from PR133 head `0596884757380dc1f0c2c4dbc8ed1f7a20d70795`, after independent review and all seven workflows passed, including the production renderer and native/headed replay integrity. No branch merges are required.

## Scope

Extend the existing bounded wall/eclip and door/wclip adapters to binary animation coverage. Classify the complete original strip before fitting any frame. Accept opaque pixels, ordinary alpha-zero cutouts and XL purple super-hole pixels; reject non-key partial alpha anywhere in the source. Keep partial blending, object/vclip animation, ordinal resource directories and full mission admission separate.

Reuse the static color/mask fitter and production upload/composition paths per frame. Select each color and mask together using the existing authoritative phase or applied wall mapping. Preserve stock simulation timers, events, state, save bytes and SIM/FX RNG. Require the same sealed mission capability, owner/package generation, POG priority, classic/imported-D1/multiplayer exclusions and exact-frame-name precedence.

Account for color and mask texture slots and mip storage within the existing 128-frame and 32 MiB limits. Release both resources on eviction, transition, GPU reset or partial upload failure. Do not decode during ordinary frame selection.

## Evidence and tests

Use synthetic animated patterns to distinguish opaque coverage, ordinary cutouts revealing the base texture, and super-holes revealing geometry behind both textures. Exercise base/overlay/standalone rendering, rotations, filtering and mips, sparse later-frame holes, partial-alpha rejection, bounds, owner/stock transitions and failed allocations. Retain real eclip critical/stopped/restore and native door transition tests.

The measured original corpus has no established binary-transparent animated positive witness. BoilingPoint `door53#0.tga` (SHA256 `b8de6bca114401a30af53f445d50dc027c5475414833711b23c04203b8686c3a`) has four frames, 69,906 ordinary cutouts and 1,453 non-key partial-alpha pixels, so it remains rejected in this slice. Its original import/publication/native-resolution check remains useful, but is not a GPU rendering qualification. Static `misc075.tga` provides original binary-mask evidence, not an animation witness. See [the corrected exact-engine survey](xl_texture_followon_evidence_20261025.json).

Run bounded decoder/coverage contracts, Android builds, the controlled production GLES fixture, existing original-resource integrations and final-head CI. Distinguish the fixture's injected identity/reset hooks from full production mission loading. No private artwork is committed or uploaded.

## Initial validation

Nine contracts, three native original-resource integrations and all 18 focused JVM tests pass. Both Android engines and both GLES fixtures compile; scoped formatting, catalog validation and diff checks pass. The first JVM invocation omitted the workspace library environment and could not launch the SDL_mixer-linked native probe; rerunning with the normal activated environment passed without code changes. Actual GPU execution and final-head CI are pending.

The new controlled GPU cases cover eclip/wclip selection, per-frame ordinary and super-hole coverage, both texture slots and standalone rendering, four overlay rotations, three filter settings, reversed draw order, classic and foreign-owner rejection, sparse later-frame fitting, late partial-alpha rejection and pool-capacity failure followed by replacement-reset recovery. Existing save/critical-effect/door timing cases remain in the same runner. Pool exhaustion is a preflight rejection test, not an injected mid-upload driver failure. No original animated binary artwork or full production mission-loading GPU campaign has been qualified.
