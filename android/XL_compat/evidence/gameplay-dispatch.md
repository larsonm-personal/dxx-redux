# Engine gameplay sound dispatch

The ordinary engine sound APIs now select the owned XL mixer when level
extensions are active. Stock levels retain Redux's implementation. This covers
immediate and positioned samples, moving-object and fixed-position owners,
initialization, deferred startup, synchronization, destruction and registry
queries. Existing pause/resume and stop hooks use the same XL owners and voices.
Legacy return tokens stay nonnegative on success; saved owner serials remain
64-bit. A one-shot returns no owner even when it plays, and never falls through
to a second Redux playback.

The reference is `2576a98b5de310e3ac7c9777df2677d1627b8bae`, particularly
`audio/objectaudio.cpp`. The already-qualified logical translation, spatial
math, ownership and mixing policies remain the basis of these engine hooks.
The source's low-volume acceptance and negative physical `PlaySound` IDs are
preserved. New logical linked requests retain their original IDs for demo
events; recording an existing owner uses the source's reverse-table alias.

Most changes are under `android/XL_compat`. The core additions are dispatch
branches in `d2/main/digiobj.c` and deferred-start notifications in
`d2/main/gameseq.c`. D1 code and low-level SDL channel APIs are unchanged.

## Controlled verification

The shared native/Android fixture executes actual engine APIs. Nine captured
output cases cover immediate/once/3D playback, negative physical IDs, positive
volumes below Redux's historical cutoff, low-memory translation, object and
position one-shots, moving owners and deferred segment startup. Each case
requires exactly one XL voice and no Redux channel before capturing PCM.
Additional controls reject invalid requests, reuse duplicate owners, inspect
live registry counts, destroy only the requested source, and stop both runtimes.

An 81-byte demo event control checks linked one-shots, persistent creation,
recording an already-live owner, duplicate suppression and destruction. It
checks logical sample identity, fixed-point volume and full object signatures.
This qualifies those event writes, not complete XL legacy-demo playback.

Native disk-through-stock tests also execute Redux immediate and linked APIs,
check their separate registry and SDL channels, then restore the exact XL state.
The initial derived level's ambient source is checked before isolating named
trigger output. Scope transitions require only the incoming ambient owner and
no outgoing named event or voice.

Broader verification exposed three failures rather than silently dropping them:

- The water arena reached a geometry guard restricted to the old pilot flag.
  Shared geometry now accepts every active XL source. This does not enable a
  rendering profile or expand mission admission. The arena still tests its
  separate experimental-profile identity and stock transitions.
- Countdown and disarm corruption controls compared full save trailers while
  audio advanced. The retained before/after trailers differ only in audio
  playheads/completion. These controls now freeze the SDL callback during the
  atomicity check, keeping all audio fields in the byte comparison.
- Registry exhaustion used to depend on repeated identical sounds allocating
  separate Redux owners. It now fills distinct fixed positions and verifies
  fluid allocation failure and recovery at the actual XL capacity.

Final Windows native audio and water-arena runs pass. The initial Linux selection
passed 49 of 52 cases and exposed the three failures above. After repair, all
seven targeted final checks passed in 114.60 seconds: those three cases, wide
object/wall/trigger controls and the complete audio runner (55.06 seconds).
The other 46 unaffected cases were not repeated. Final Android verification
passed all 101 steps in 348.171 seconds, checking 1,001,984 samples across 60
output cases at 48 kHz, including nine engine dispatch cases and the demo-event
control. All 26 protected files were restored with matching hashes. The debug
build, both automation catalogs, scoped formatting and diff checks also pass.

Artifacts are under `temp/xl-gameplay-audio-20261029` in
`C:/local/dxx-redux/temp/xl-batch2-20261027`. Failed private fixtures are retained
with `-dispatch-before` suffixes under the private contracts build directory.
Public CI contracts were not rerun locally as substitutes for these retail
runtime checks.

## Remaining fidelity work

Generic engine dispatch does not establish the source's explicit sound classes
or sample choices at every gameplay caller. The subsequent
[combat increment](combat-audio.md) implements ordinary weapon fire and
weapon/actor/water explosion selection with actual event checks. The
[actor/afterburner increment](actor-afterburner-audio.md) adds the corresponding
robot/boss and afterburner policies and controlled production event checks.
Ambient-source distribution, external sound objects, wall changes and classic
replacement installation also remain separate requirements. Queued multiplayer
voice callers remain deferred. Admission has not been expanded.
