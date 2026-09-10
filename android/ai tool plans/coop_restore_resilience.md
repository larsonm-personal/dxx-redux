# Cooperative restore resilience

Status: implemented and verified for D1 and D2

## Verification record

- Both Windows engine targets build successfully. D1/D2 `test_coop_recovery`, `test_coop_powerup_duplication`, and `test_coop_save_format` pass. Coverage includes consumed-slot reuse, replayed tags after a lost object generation, reordered packets, conflicting claims, invalid gear, disabled duplication, save/restore, reconnect, and host migration
- Android `:app:assembleDebug` and all seven `GameProcessExitDiagnosticsTest` JVM cases pass. These cover fallback/deduplication, missing processes without history, live-process suppression, normal quits, and incomplete report files
- `test_lan.ps1 -RestoreReportCase` passes `unexpected_exit`, `interrupted_restore`, `normal_quit`, and `fail_after_hide` for D1 and D2. Exit reports survive launcher restarts; normal quit leaves none. An injected failure after hiding the window returns to a responsive menu and retains its failure phase in the report
- D1 and D2 `-RestoreResilience` runs pass missing-gear, bounded unreadable-section, and clean save/reload cycles, with unchanged inventory, one recovery report per damaged fixture, none for the clean reload, and continuing bidirectional PDATA
- Latest-build standalone `-SpewPartialPickup` and `-SpewRecovery` runs pass for both games. Partial pickup preserves three remaining homing missiles in both peers' recovery ledgers; two consecutive client process restarts return valid gear exactly once without duplicate world objects. Rejoin requires a fresh inventory baseline and runs separately from inventory-seeding restore scenarios
- The original `touch.mg3` restores on a fresh Castaway level-8 host/client session. Both peers accept all 31 pickup records and 90 recovery records, discard only ID 81/signature 18117/object 254, and retain homing counts 8 (`touch`) and 10 (`Player68`) with primary weapon flags unchanged. Network updates continue. The original file's SHA-256 stays `B547F4671CB275EF799832CA2D5FEA5BAFFEAF07154BEEF25BCBBBE3B39A237D`
- Actual Advanced -> Crash Reports UI verification finds the Castaway recovery report beside Save/Share controls. Save downloads recovery and unexpected-exit reports byte-for-byte. Share opens Android's chooser with the correct recovery filename and Drive available; the shared cache file also matches the original bytes. No external upload was performed
- Evidence: `temp/coop-host-check.log`, `temp/coop-engine-build.log`, `temp/coop-android-build.log`, `temp/coop-report-d*-*.log`, `temp/coop-lan-d1.log`, `temp/coop-lan-d2.log`, `temp/coop-partial-d1.log`, `temp/coop-partial-d2.log`, `temp/coop-rejoin-d1.log`, `temp/coop-rejoin-d2.log`, `temp/coop-provided-restore.log`, `temp/coop-provided-emulator-*-report.txt`, and `temp/coop-share-ui.xml`
- Final scope audit: native session publication and launcher fallback use the existing Crash Reports directory/filter; normal exits suppress reports; incomplete files stay hidden. Restore wrappers restore window visibility and defer damaged-world closure to a safe event boundary. Gear validation commits usable records without crediting rejected ones; serialization normalizes stale bindings without running network frames. Shared disk packing is explicit, desktop builds pass, scoped formatting passes, and `git diff --check` reports no whitespace errors. Unrelated working-tree changes and the original personal save are preserved

## Objective and policy

Keep a cooperative game running when optional pickup or dropped-inventory metadata is inconsistent. Log and discard unusable gear records; do not reconstruct, refund, or invent gear to compensate. Preserve valid records and ordinary saved player inventory. Make unexpected engine exits produce durable, shareable diagnostics even when no signal crash occurs

## Evidence and limits

- The supplied debug log records successful save transfer and level synchronization, followed by `invalid per-player powerup state`, restore rejection, `jni startup main returned`, and process termination
- D2 restoration hides `Game_wind`, but the gear validation failure returns before restoring visibility. With no visible window, the main loop exits; JNI then intentionally kills the process. A signal crash handler cannot reliably report this path
- `touch.mg3` has a valid cooperative metadata checksum, 31 pickup records, and 91 recovery records. Recovery ID 81 claims a live four-pack of homing missiles owned by `touch`, signature 18117, object slot 254. The slot is deleted and no saved object matches. The restore binding validator rejects it
- Older recovery ID 48, owned by `Player68`, has the same signature but is marked taken. Rebinding through a reused network object mapping is a concrete suspect, not yet a proven event sequence
- The supplied quicksave is a different snapshot from the logged `coopsave.mg9` with 29 pickup records. Reproduce the demonstrated inconsistency without claiming that the exact original autosave has been inspected

## 1. Reliable reports for unexpected exits

### Implementation

- Extend the existing native fatal-report and CrashLog infrastructure with a nonfatal diagnostic writer callable from normal engine context. Reports must be visible and shareable through the existing Advanced crash/report list
- User-facing requirement: every unexpected-exit fallback and tolerated-gear recovery report appears in **Advanced -> Crash Reports**, using the same download/share controls as native crash reports. Do not introduce a separate diagnostics section or require users to find/download internal marker files or debug logs
- When no native dump exists, produce a standalone text crash-report substitute containing the available restore context, breadcrumbs, and process-exit evidence. Label it clearly as **Unexpected engine exit (no native crash dump)**, **Interrupted save restore**, or **Save restore recovered (inconsistent gear discarded)** as appropriate. State when no native stack trace is available; do not imply the recovered case crashed
- Record restore session/build identity, game, mission, level, save filename and checksum, restore phase, host/client role, result, rejected-record counts and reasons, window visibility, and recent breadcrumbs. Bound record detail volume and include aggregate counts
- Persist a restore-in-progress marker before replacing live state; update it only at meaningful phase boundaries. Finish it with success, recovered-with-discarded-gear, or failure. Use atomic file replacement and flush durable state before the wrapper intentionally terminates
- Track explicit normal shutdown reasons at their actual call sites. Before JNI kills the process after `main()` returns, emit an unexpected-exit report if no normal shutdown reason was recorded, including any unfinished restore marker
- On launcher resume/startup, reconcile an unfinished session with historical process-exit information. Create a fallback report if no completed report exists, keyed by session identity to avoid duplicates and PID reuse errors. Never treat the mere act of backgrounding or a still-running game as an exit
- Normal quits must not create crash reports. A tolerated gear problem should create a recovery diagnostic and log entry, not a blocking crash dialog
- Best-effort reporting failures must not stop restoration or trigger recursive fatal reporting. SIGKILL and storage failure cannot guarantee a complete last-moment report; the prewritten marker provides the fallback evidence

### Validation

- Exercise an unexpected return from the game loop with an unfinished restore and verify a shareable report survives process teardown and launcher restart
- Exercise termination during restore and verify marker-based fallback plus exit information; repeat launcher resume to verify deduplication
- Verify intentional quit produces no unexpected-exit report, and continued gameplay after discarded gear produces a recovery diagnostic without a modal interruption
- Verify each generated report is actually listed under Advanced -> Crash Reports and that the existing download/share action exports its complete text. The exported report must be useful by itself without separately downloading its internal marker or debug log

## 2. Prevent inconsistent recovery ownership

### Investigation and implementation

- Extend the existing real-code recovery harness with the ID 48 / ID 81 shape: old consumed client-owned gear, reused network mapping, new same-type host-owned gear, and reordered recovery/removal packets
- Add targeted diagnostics for binding changes, generation/revision, local signature, remote owner/index, state transitions, and removal decisions. Use these to confirm the suspected sequence before choosing its fix
- Audit both `coop_recovery_frame` and `coop_recovery_receive`, along with object mapping creation/deletion, `find_object`, pickup/removal, expiry, restore, and host migration
- Require a binding to identify the intended object generation, not just a reused slot and matching powerup type. Local signatures can differ between peers: do not compare an incoming remote signature directly with a local signature as a shortcut
- Retire consumed/departed bindings so historical records cannot attach to or delete newly created gear. Ensure each live object has at most one active recovery owner. If evidence is ambiguous, discard the suspect recovery claim rather than removing an unrelated object or assigning gear
- If generation identity must be carried in protocol/save metadata, update shared contracts and both games together. Android formats are pre-release; do not add compatibility machinery unless needed for the narrowly specified repair policy
- Normalize stale ledger entries before serialization so newly written saves are self-consistent. Work from the same object snapshot as the save writer; do not call side-effecting network/frame processing between serializing objects and their ledger

### Validation

- Run the extended recovery harness for D1 and D2 with slot reuse, duplicate/reordered packets, deletion before a collection report, restore, reconnect, and host migration
- Assert no old record removes new gear, no record grants gear twice, and a save/restore round trip preserves valid ownership

## 3. Tolerate and discard bad gear

### Implementation

- Replace all-or-nothing optional gear application with validation into a temporary accepted-record set. Commit the valid subset and return a structured outcome with discarded counts and reason codes
- Recovery records with missing/deleted objects, wrong powerup type, ambiguous bindings, invalid identity/state/gear values, or conflicting IDs are discarded. Conflicting claims are dropped conservatively; they must not produce inventory credits or resurrect pickups
- Apply the same log-and-skip policy to invalid duplicated-pickup records. If duplication is disabled, discard its tracking records. Independent records remain usable
- Do not let one malformed optional record poison the rest of the ledger, saved player inventory, or world restore. If an optional gear section cannot be safely parsed or allocated, clear that section and report the omission
- Keep bounds/checksum validation separate from gear semantics. Never walk untrusted counts or offsets. Skip an unreadable optional section only when its boundaries and the independently required save data can be established safely; a damaged core save remains a controlled restore failure
- Remove the generic fatal restore result for discarded optional gear. Complete remaining player mapping, inventory revisions, absent-player handling, ownership synchronization, timer reset, and restore-status cleanup normally
- Make restore window visibility cleanup unconditional on every return after it is hidden in both `d1/main/state.c` and `d2/main/state.c`. For an actual core restore failure, return to a visible safe menu/session state rather than resuming partially replaced world state or accidentally ending the engine
- Keep discard decisions consistent across peers through the existing authoritative restore transaction. Validate against a common saved-object identity before peer-specific remapping where possible; otherwise carry the host's accepted/discarded result. A host and client must not retain conflicting ownership claims
- Bound and deduplicate logging per restore. Include record ID, expected/actual object identity, reason, and totals. Do not show a modal question or require acknowledgement to continue

### Validation

- Recreate the supplied save's missing signature 18117 / recovery ID 81 in a synthetic regression fixture. Verify it is discarded, the remaining valid ledger survives, no four homing missiles are refunded, and restoration succeeds
- Cover a mixed valid/invalid ledger, duplicate claims, disabled duplication with leftover pickup records, and an unreadable bounded optional gear section
- Run a fresh two-peer LAN restore integration test. Verify both peers complete the wait, retain visible responsive game windows, agree on accepted gear, can move and collect valid gear, and remain connected
- Save again and reload after recovery; the discarded record must not return
- Exercise a core restore failure after the window is hidden and verify visible recovery plus a durable diagnostic
- Use the supplied save locally to confirm tolerant restore against real Castaway data. Keep the user's original untouched and use a synthetic checked-in fixture instead of committing personal save data

## Delivery order and checks

1. Add targeted diagnostics and reproduce the stale binding in the recovery harness
2. Implement tolerant record application and unconditional restore cleanup, with regressions
3. Fix the confirmed ownership lifecycle issue and normalize serialization
4. Finish unexpected-exit reporting and launcher fallback, then run the complete two-peer restore/reporting checks

Build and test both games through the relevant host CMake targets, including `test_coop_recovery`, `test_coop_powerup_duplication`, and save-format tests. Compile changed Android Kotlin/native code, run scoped mixed-language formatting once, and run Android integration tests serially. Preserve unrelated routing and save-sharing work in the working tree

Completion means bad optional gear is discarded with useful diagnostics, valid gameplay continues on both peers, newly saved ledgers remain consistent, and unexpected non-signal exits leave a report the user can share
