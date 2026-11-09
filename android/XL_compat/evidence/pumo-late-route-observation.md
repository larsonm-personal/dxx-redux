# Pumo late-route observation boundary

This implements the diagnostic P1 packet from
`android/ai tool plans/xl_pumo_late_progression_20261107.md`, above plan commit
`62b8533e67a32c58dc24be24cc1d3188d81e8205` and runtime parent #398 at
`c0263f8eaead6f76f801f8f35906836acf21d3f7`. It does not qualify new earned Pumo
progression or identify a missing gameplay feature.

## Observation contract

The native ordinary-player probe observes the complete trigger table, not only
the first 64 latched activation bits. It compares stable activation/scheduler
state and separately observes accepted wall operations. The developer-only
observer runs once at the existing XL operation boundary; duplicate native/XL
preview-result logging is not operation evidence. Rejected, delayed-pending and
idle dispatches do not count as successful operations. No gameplay dispatch,
clock, debounce, actor, countdown, save schema or resource admission rule changes.

Receipts label post-load, post-frame, post-operation and pre-exit snapshots. An
accepted exit is observed before synchronous teardown, preserving old-level state
for a transition in that frame. Destination arrival remains independently checked.
The observer counts are diagnostic process/level observations, not persisted game
state or a claim that every attempted event succeeded.

Earned-phase receipts bind a newly generated run nonce to the executed plan,
executable and consumed entry-save SHA-256 identities. The owner removes stale
generic results before launching, rejects absent/empty/truncated results, checks
exact action and transition counts, source/destination and final player state,
and verifies retained checkpoint bytes. Each quick save also has a distinct
process-tagged history file and receipt; existing latest-level aliases and earned
gate assertions remain.
The initial Majha direct owner retains its historical contract separately.

## Original qualification remains blocked

At initial asset-free qualification the selected cloud checkout had no original
Pumo/retail input root or prior
private earned saves. Earlier original Majha, Metillis, Iquaxel and Sophodo runs
were local opt-in native CTest runs. Their documented private evidence location
is `temp/xl-overnight-20261104`; those artifacts are absent here. No desktop is
currently connected. No original requirements ledger or main 7->10 / alternate
14->5 route has been rerun for this candidate.

The existing trusted `nightly-tooling-sweep.yml` candidate mechanism can qualify
reviewed D2-XL controls against an exact open PR head without exposing assets.
Its pinned private fixture staging includes Neptune/Boiling Point, not Pumo.
`private-ci.md` lists Pumo as a proposed additional asset, not an installed one.
No asset manifest, cohort, protection, readiness score or trusted workflow is
changed to turn this diagnostic work into an original Pumo pass.

P0 requires an authorized full root with `missions/pmines.hog`,
`missions/pmines.mn2`, `data/d2x-xl.hog`, `mods/pmines`, and retail data. Run the
unchanged earned fixture chain in disposable profiles before route packets.
Original hashes and model/definition ownership guards remain intact. No original
archive, user pilot, APK or device is modified.

## Validation ownership

- `xl_pumo_receipt_contract`: asset-free tests of the actual CMake receipt
  consumer, including missing/stale/truncated receipts, wrong nonce/input/save
  hashes, wrong counts/levels, dead/cheat state, and process failure
- `xl_trigger_capacity`: actual native `check_trigger` / XL operation producer
  controls on an explicitly derived 254-trigger Neptune level, including indices
  63/64/66/79, repeated success, debounce/disabled rejection and idle/reset behavior
- Existing Majha through Sophodo owners: original earned-route acceptance remains
  mandatory, and is not replaced by either diagnostic owner

Native simulation remains unrendered, with real GL admission. This work supplies
no actual-render, audio, Android import, phone or connected human traversal claim.

Local validation on the implementation candidate: standalone XL CMake build and
all 20 asset-free CTests passed, including the newly registered receipt contract; all 36 private-asset policy unit tests passed; automation-catalog validation
passed (99 standalone JSON, 546 support scripts, 315 standalone PowerShell).
The required aggregate catalog test reached the Linux cleanup controls but was
blocked by the host's absent `/proc/self/task/64/children` interface (exit 125,
not a gameplay timeout). Its supervision is unchanged. The first attempts exposed
missing local CMake/PATH and dependency-base configuration; those were configured
and the affected checks rerun. Scoped mixed-language quality and whitespace checks
passed. Native producer and exact-head hosted results are reported separately.
