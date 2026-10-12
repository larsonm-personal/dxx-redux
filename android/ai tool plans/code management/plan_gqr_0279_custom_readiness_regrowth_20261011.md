# Proposed GQR-0279: custom music readiness regrowth, 2026-10-11

Diagnosis and implementation plan only. Proposed GQF-0293/GQR-0279 require final canonical admission with chunk0362; no product changes or runtime acceptance here.

Provisional impact rating: 47 (H/M/B/C/R = 23/0/7/10/7); proposed owner: GQR-0279. Common music subsystem responsiveness, concrete synchronous provider call, moderate platform validation, zero original-file saving.

## Problem and provenance

SetupSections.kt MusicInfoSection2680-2685 constructs CustomAudioSetManager during composition, reads/parses registry in its constructor and executes hasUsableTrack's referenced-URI callback. The callback opens a provider descriptor on the Compose thread. Slow provider or registry work therefore blocks rendering/input; each recomposition can repeat it. This code is absent at base7877ad30. Archived BR0444 fixed the same storage/provider-on-Compose family on2026-08-03; admit REGROWTH linked to that historical repair, retaining its FIXED status. GQF0097/GQR0084 is resume-save discovery and is not this root.

## Implementation boundary

Allowed product paths: SetupSections.kt, CustomAudioSetManager.kt, SafUriPermissions.kt only if a shared probe contract is necessary. Allowed validation: existing CustomAudioSetManagerTest.kt plus one focused Compose/Android readiness integration and its required test-catalog registration. Update canonical ledgers and this plan when implemented.

Render one immutable readiness snapshot keyed by canonical owning set, admitted registry/source generation and refresh. Move registry reads/parse/stats/provider opens to lifecycle-owned IO. Bound and deduplicate unique probes, close exact acquired descriptors and verify provider cancellation behavior rather than infer a hard deadline from a coroutine timeout. Represent loading/unavailable explicitly. Publish only for the current owner; cancellation/dismissal/set switch must retire prior work.

Coordinate BR0432 accepted level-track policy: movie-only, disabled, missing or revoked tracks must not advertise ready. Preserve copied/reference behavior, shared grants, previous accepted playlist and actual launch policy. This owner changes dispatch/lifetime; BR0432 owns playable-track admission and normalized mode semantics. Do not copy AdvancedSettings wholesale or introduce layout migrations/native format parsing.

## Acceptance and prerequisites

Use production Compose route with dispatcher assertions and a controllable provider. Block registry/stat/provider work while UI continues responding; force recomposition and verify bounded unique work per generation. Switch set/source/refresh while a probe is blocked, cancel and dismiss, complete old work late and require no stale publication. Cover provider refusal, revoked URI, inaccessible/missing files, movie-only/mixed/disabled sets and retry. Verify every acquired descriptor closes and old work actually retires; cancellation signal support must be observed at the provider boundary. Keep historical provider-delay/fault acceptance unexecuted until separately authorized.

Implementation checks when authorized: scoped Kotlin quality, relevant JVM tests, focused Android route and D1/D2 launch/source readiness integration. No tests or product edits in the current diagnosis tranche.
