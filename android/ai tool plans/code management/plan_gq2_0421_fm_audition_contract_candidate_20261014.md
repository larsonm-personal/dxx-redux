# Chunk0421 FM audition contract candidate, 2026-10-14

Diagnosis-only candidate;no canonical ID or status/rating admission. Local proposed score44 MEDIUM23/0/4/10/7 unpooled,subject to final owner dedup and qualification. Separate from existing GQF0312/GQR0298 retired renderer-default oracles.

The maintained fm_quality/run.ps1 configures fm_quality/CMakeLists.txt,which invokes prepare.py with fatal-command handling. prepare.py's first float-driver pass requires exactly one literal48000, -10); inside render_comparison. Current and frozen production test_music_synth.c use48000, gain_db); instead:the expected old literal has zero occurrences and the current gain call one. replace_once therefore cannot accept that producer under its own guard. No generator,CMake,compiler or media run was executed.

The same project still generates tsf_render_short(s->sf,...) while current music_synth owns a music_fluid object. Its variant targets include current music_synth.cpp but not music_fluid.cpp and do not declare/link libfluidsynth;the maintained extract target does. These are additional source integration obligations after the concrete guard mismatch;do not invent compiler diagnostics or claim a platform build run.

Conditional future repair belongs to this standalone audition harness. Reconcile guarded transformation with current gain argument and synth API while preserving the approved legacy/current,float/native and shipping comparison meanings. Include the current shared FluidSynth implementation/dependency graph and runtime library placement where needed through maintained central CMake helpers,without duplicating pins or modifying shipping defaults. If a legacy outlet is retained,implement it explicitly for the current typed owner rather than restoring old TSF ownership in production. Keep guards strict and keep generated source in the build directory.

Future acceptance must exercise the actual documented run.ps1/configure/build/CTest path and intended generated variants,then verify realFM selection,bank/HMP-HMQ identities,ordered register events,raw repeatability,gain/native float/PCM boundaries,fixed causal delay and no fitted gain/timing against an identity-qualified approved audition. Preserve clipping/dither/loudness/finite checks and distinguish host elapsed timing from device performance. Test source-fragment drift remains a clear failure;do not bypass guards or use stale binaries to make the command appear successful. Keep missing private references explicit rather than inventing an auditory reference.

Later GQ2-0522 CMake and GQ2-0525 preparation reviews must reuse any admitted owner. Existing supply-chain/incremental-build,private corpus and runtime acceptance remain separate. No production/test/helper edit,build/test/generator/audio/network/device operation or inherited saving by diagnosis.


## Final local ownership decision, pending canonical publication

Propose GQF-0313 OPEN / GQR-0299 TODO for this one harness-integration cause, PRIMARY44 MEDIUM23/0/4/10/7 unpooled. These IDs are proposals only until the canonical mutation and independent replay pass. The strict old-gain literal mismatch is the first demonstrated source barrier; typed Fluid rendering and source/link/runtime dependencies are obligations in the same maintained harness repair, not separate findings or invented compiler failures.

All312 current root titles reviewed; exact audio/fixture root rows and current/archived BR source-identifier census reconciled. Existing0024 verified dependency acquisition and0123 incremental identity are distinct from applying obsolete source transformations/API ownership. Existing0312 covers fresh/reset assertions, not standalone audition generation. BR0016 aggregate registration and BR0543 runner lifetime remain separate acceptance owners. The earlier fm-quality-listening-experiment.md and fm-clean-default.md record successful September23/clean-resampler work; preserve that historical evidence and the chosen clean output, without claiming the current command still reproduces those results. Do not revert production to make the old audition compile. Later0522/0525 reuses this proposed owner after admission.


Canonical admission0421 passed independent publicationPASS5383;proposed0313/0299 and0314/0300 are nowOPEN/TODO,each44;existing0312/0298 extension retained47. Implementation/actual-command/device/negative-control acceptance remainsunchecked. Allplansauthoredonly.
