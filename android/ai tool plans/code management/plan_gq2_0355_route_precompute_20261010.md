# GQ2 chunk0355: Route metadata precompute, 2026-10-10

Diagnosis and implementation plans only, serially. Preserve concurrent changes; no product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits

- [x] Review assigned frozen L1-L750, enclosing tail and current delta with exact identities
- [x] Trace scheduler/cancellation, metadata cache/source ownership, actual Setup consumers and maintained fixtures
- [x] Reconcile existing owners and actionable simplification/inherited decisions
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0354 publication audit PASS2891 SHA8b06c8953055c428043c63a93e5d00392fb106c019d131936cc324a977324c78. GQ2 354DONE/273TODO/627;1169 terminal;276 fixes;12 investigations. Assigned RouteMetadataPrecomputeCoordinator.kt L1-L750 of945 frozen/current lines; current equals frozen SHA878c17ee737ace738b89a2b66d011df8e764fa5122613e5aaa2b1e462bc8447d. Path absent at GQ2 base and1996 reference. Enclosing tail review does not imply another queue credit. Whole-file addition and assigned range must be recorded separately


## Source, consumer and owner reconciliation

Assigned L1-L750 and enclosing L751-L945 reviewed freshly, five ranges. Whole current945-line file equals frozen; base/1996 absent and current delta empty. Whole addition+945/-0 is the enclosing diff; only750 assigned lines receive0355 credit. Tail195lines belong to DONE0314. Source checkpoint8aa4f12eb0218d164c37abd9faee91072a106b445d018bd1c9987a0a8c8b087e, scope32e10ab3dc78fafb8a05f08ba69237d9599941a9e8367020f1ad6155ebf0aa06, independent audit PASS982 SHA41e4285679861594bbe859c14e17985cbfcdc115054a9c5ba64ec775a7cde872.

Reused39 exactly matching consumer ranges from audited0314 snapshot cdc22faf40a8ba850b7538da4d71559656531398e60a0ee37829cbf41c429bdc and report3f0bd0090834e4f50f423ceaa8056ced3b57df8b48ab884233301b354c00867a. Its audit6ff9f9196679e20cc3eb6beb0c6c9a5d141289979b5a4aa0285e67559472f3e4 records PASS. Prior source ranges were replaced by five fresh reads; nine stale or moved consumer bindings were excluded. Twelve fresh consumer bindings cover prior0314 plan, current Setup IO scope/construction/pause/destroy/launch/cache-clear, automation projection and five current owner definitions. This yields56 contexts, not56 freshly read files. Reused maintained tests are source evidence, not new executions.

Setup creates a SupervisorJob plus Dispatchers.IO scope and passes it directly to the coordinator. Discovery therefore does not establish main-thread blocking. Preserve launch ownership checks and CancellationException rethrow in the actual handoff caller. stopForGameLaunch marks launch pending and reports cleanupPending while requesting worker termination; it intentionally retains zero grace and returns before full cleanup. The old attempt must lose publication/cleanup authority immediately without delaying engine startup.

Current stop/cancelRunner null the sole runner after cancellation; start tests isActive rather than completed retirement. Preemption callbacks wait for completion and use runner identity, but lifecycle stop/start, viewer close and cache-clear need the same explicit generation discipline around blocking fingerprint/sidecar/native work. Preserve BR0484 request-local NonCancellable cleanup and FIFO repairs while completing process-generation/deadline acceptance. BR0506 launch publication and GQR0102 immutable source generation remain existing owners; no new duplicate lifecycle root.

Concrete simplification: unify notifyContentImported and setComputeFaster cancel-and-completion-restart plumbing in one private owner operation, preserving import focus/priority policy and distinct monitor messages. Retain the stopping job until retirement or transfer its cleanup authority to a tracked retiring generation; reject obsolete completion handlers. Keep gameLaunchPending/viewer guards and zero launch grace. Do not solve stale cleanup by adding a sleep or restarting from an unqualified completion callback.

The main scheduling loop repeats terminal checks and track scans for eligible music, sidecar readiness, next work and progress. Plan one per-discovery scheduling snapshot after explicit route-cache invalidation, with current ledger status and per-track success/failure/pending outcomes. Derive queue and counts from that snapshot; keep NEXT route precedence, focused/enabled music ordering and partial-attempt fairness. isCompleted currently removes missing ledger cache entries, so simply memoizing or reordering calls as if they were pure would change behavior. The snapshot must remain tied to source/database/attempt identity.

Route source identity remains path/length/mtime plus target and cache generation; music catalogs already use content-backed identities. Retain archived BR0088/0089 fixes, staged-byte verification, atomic publication/quarantine and current database rematch. GQR0102/0017/0021 retain complete source/policy/database generation and typed publication. A cache filename plus isFile is not native cache validation. Do not duplicate native cache-format parsing in Kotlin.

Sidecar failures add the source to publishedMusicSidecars; failed tracks can count terminal and finish the mission. Existing publication plans must distinguish attempted, successful, authoritative-empty, cancelled and retryable-failure outcomes. Retry after storage recovery or changed policy/database/source without discarding prior valid bytes; exact identity prevents old work overwriting newer output. Preserve MissionZipMusicNames atomic nonempty publication and cancellation rethrow.

GQR0212/0045 retain bounded catalog/status/log/fingerprint byte/work/live-memory/deadline admission before materialization and inside blocking work. Monitor local locking is distinct from the ledger's OS file lock; launcher/game producer correlation remains existing diagnostic/publication policy. Existing cache maintenance and stage markers qualify historical no-cleanup/no-invalidation descriptions. Complete active reader/writer leases and confined paired-game pruning without claiming current maintenance tests prove multiprocess safety.

Acceptance must exercise actual coordinator/worker/Setup handoff, quick pause/resume/viewer transitions, import and compute-speed preemption, clear during blocking work, late completion and later successful work. Assert no stale publication/deletion, truthful pending/retired and success/failure counts, immediate authorized game launch and exactly current source/database outputs. Retain ordering/classifier/monitor/maintenance/fingerprint fixtures and ordinary D1/D2 cache behavior. The audited local fixtures do not prove multiprocess retirement or cancellation inside blocking native work; no runtime execution is credited and deferred probes stay deferred.

Disposition ISSUES/NO_INHERITED_EFFECT, terminal RETAIN/GQR0212 REFERENCE56 (32/0/7/10/7). No new finding/fix/investigation/status/rating or inherited saving. Android scheduler cleanup stays outside1996 engine files. Full consumer/owner analysis complete; final report/publication/audit pending.


## Chunk0355 canonical publication and independent audit complete

GQC-1183 ISSUES/GQD-1063 NO_INHERITED_EFFECT; RETAIN/GQR0212 REFERENCE56. Assigned750 lines and enclosing195lines reviewed fresh, complete enclosing addition+945/-0 and empty current delta verified. Tail remains credited only to DONE0314.56 contexts:5 fresh source,39 exact prior0314 consumer selections and12 fresh consumers. Concrete identity-checked cancel/restart owner and per-discovery scheduling snapshot plan; preserve explicit cache invalidation, zero game-launch grace, supplied IO scope, native authority and existing cache/sidecar repairs. Existing admission/source/generation/worker/publication owners retained; no new finding/fix/investigation/status/rating or inherited saving.

Independent audit PASS3275 SHAfb38b208b5a4c3b0d7770ace9d2a31570f2efed68f9a02b8c0bac5e7839b5d0c. Reconstructed all five canonical files exactly; preserved every old terminal/fix payload and definition, all other queue rows including0314 and unchanged BR bytes. Verified source/assigned-versus-enclosing identity, manifest/blob/base/original/delta, source checkpoint audit and exact prior0314 snapshot/report/audit chain,56 contexts including5 unchanged relocated owner rows and concrete implementation semantics. Initial audit stopped at an incorrectly transformed expected baseline hash before writing; corrected to the actual publication baseline identity, then all checks passed.

Snapshot SHAe9f8d8d7c8c57ac5d44e020acaf67f9ee8614a9ab5b3bf12cad4852e1d2493d2; report SHAfe1b3a39096f0cb2716d7ce17dd1b902b737633e15c173dcfc812e835ab2da6d; scopec3d8b80080e3cf03ccc40e308f9ee31bbac1952bc2f3a7def292cffd12d381c6. Snapshot/audit plan hashes precede this closure.

Product diff changed externally between publication and final audit. Every credited source/consumer file and range still matched during that audit; no diagnosis tool edited product/test/helper files. Preserve concurrent work and require final-head supplements rather than claim unchanged whole-worktree coverage.

GQ2 now355DONE/272TODO/627 numbered;1170 terminal;276 fixes;12 investigations. Next0356 SetupActivity.kt assigned diff hunks1-66,newL9-L1241. Remaining numbered scopes, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation remain required. Diagnosis/docs only; no builds/tests/devices/probes/generators/formatters/staging/commits. Goal active.
