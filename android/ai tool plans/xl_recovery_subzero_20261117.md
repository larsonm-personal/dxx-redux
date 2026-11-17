# Original Subzero powerup lifecycle recovery

Checkpoint four of the November 17 recovery tranche, based on
41a73ed11d55eeccbdc223cf5fea9546458c5eee (#564). The last complete published
measurement remains 59/64, separately preserving original 8/8. Local Subzero
success below is not a new full-cohort measurement or campaign qualification.

## Source and admission

The pinned XL source is 11038eb53dd86f3bcdc027dddd3f93c60f02536e.
The reference executable now compiles the actual powerup branch of
CObject::Verify and CObject::UpdatePosition alongside the actual object reader.
The consumed updateobject.cpp SHA-256 is
c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac.
Existing source hashes remain checked before extraction.

Original subzero.rl2 SHA-256 is
41a119e267513dff3e0f5ce399e4eab666f8d380bf2ae32f7694c6e4cc094b1e.
Its 134-record stream ends exactly at byte 851653. Records 4 and 6 occupy
[830809,830898) and [830991,831080). Both are type 7, Fusion ID 16, render 5,
control/movement 255, with no control or movement payload and render data
[40,0,0]. Source verification selects powerup control and definition radius,
sets creation time to zero, and leaves movement 255 stationary. Source level
loading clears object storage before reading these payload-free unions.

Admission is limited to this single-player, flag-free, empty-contains shape.
Seven incompatible shapes retain their blocker; four malformed headers retain
their parse rejection. Classic probe translation still refuses the sentinel.
The native loader clears only the two otherwise uninitialized unions for the
qualified XL record. Raw movement 255 is preserved; native Verify owns the
control transition. The native changes are thin loading hooks.

The next two load blockers were genuine source-reader behavior: object 5 is a
Fusion pickup and object 7 is Vulcan ammo, both with animation index 255. The
source reader replaces indices outside [0,110) with their powerup definition's
clip, frame duration and initial frame. The Vulcan count is 98. Qualification
covers these stationary IDs, with 16 source-reader boundary profiles and six
negative header controls. No authored record bytes are rewritten.

## Save ownership found by stronger validation

After 120 ordinary frames and a controlled Fusion pickup, native restore rejected
effect 95 at frame 4 because its HAM definition has four frames. The installed
authentic replacement has a longer strip. Native gameplay and phase reconstruction
already use the owned strip's count; the save validator did not.

Native state.c now validates against the same installed frame count, retaining
all index, time, flag and world-target checks. Android preflight reconstructs
replacement counts from immutable target resources alongside the already checked
asset hashes. It neither changes authored definitions nor trusts extra serialized
counts. The companion-save check preserves its primary candidate's count map.
The legacy UUD adaptation remains unchanged unless an actual installed override
differs from its native definition. There is no save schema change.

The diagnostic saves a frame exactly at the replacement's upper bound and
requires rejection, then restores the genuine earned save. It also exercises the
shared Android sparse-effect reader with valid and invalid replacement frames.
No RNG compensation or new comparison exclusions were introduced. Raw receipts
are retained; disk-state comparisons use the existing native_continuation_semantic
equivalences for inactive effect sides and the Guidebot host-clock deadline.

## Local evidence

Private receipts are under E:/dxx-redux-builds/xl-recovery-20261117.

- powerup-contracts.log: four Debug source/layout/inventory/effect tests passed
  in 48.24 seconds. Exact original v23 and modern v27 sentinel source receipts
  are sentinel-source-v23.csv and sentinel-source-v27.csv.
- bp4-sentinel-01: ordinary developer-OFF original Subzero passed 120 unassisted
  frames. This is a single-case diagnostic, not a replacement full measurement.
  bp4-powerup-final-01 repeats that pass after rebuilding the final save-owner
  changes, without changing the readiness criterion or watchdog.
- sentinel-native-11: full original lifecycle passed in 54.828 seconds, including
  poisoned prior unions, definition fallback, cold restore, equal repeated
  120-frame future and SIM state, controlled pickup, earned-state restore,
  replacement-frame rejection and genuine-save recovery. Binary SHA-256:
  651e4f3ba1f880dc728ec894d1d8e862a09d247ed7ac27236d5fab8079eba5ab.
- rendered-bp4-01: 120 simulation and 120 rendered frames passed on Intel Arc
  140T, using original assets and the unchanged 45-second rendered watchdog.
  Binary SHA-256:
  cba36d612ded45268cf49f90f232c5815a132823a4f8fd051833f28e35c838b5.
- android-powerup-save-02: the real Android engine libraries passed all seven
  D1 and twenty D2 mod-save cases. This isolated native fixture did not install
  an APK or modify the existing app's saves. Full Android Subzero gameplay has
  not been qualified by this fixture.
- ci-powerup-owner-02: the new owner entry point passed in 56.203 engine seconds
  with its private 30-texture fixture and separately verified authentic Vertigo
  and XL resources. Binary SHA-256:
  9af79779f667d69f0a6a09780d137a9165d5df121db00f9ea930a55ef2d950b7.
  Its older HOG hash is
  228dc2c6f4b8fbd4ef108966c43ee127007295d4215ac233b8e4ca40abae700c;
  the Subzero member is byte-identical to the frozen cohort's member. The
  private fixture is explicitly labeled separately from the complete frozen
  package. All thirty texture files match their original counterparts.
- powerup-owner-python-tests.log: 53 asset-owner and controller tests passed.
  powerup-owner-python-final.log also passed after CI resource staging changed.
  Both catalogs passed: 428 top-level entries, 550 support scripts, 99 standalone
  JSON tests and 315 standalone PowerShell tests. The first runner-catalog attempt
  stopped at the C: free-space guard; the successful run used E: output.

The candidate asset owner runs this supplemental lifecycle after its unchanged
reviewed 113-test cohort and navigation checks. It must succeed before owner
completion. Readiness still uses the unchanged ordinary binary, frozen 64 levels,
resource pins, 120 frames, guards and ten-minute CI job. No controller cohort is
relaxed or substituted.

## Retained failures and limits

sentinel-native-01/02 exposed the two invalid animation references. Runs 03/04
retained raw save comparison differences; run 05 passed the initial lifecycle.
Runs 06-09 retained the earned-save rejection and narrowed it to native effect
validation. Temporary native diagnostic prints were removed before run 10.
Runs 10/11 passed the corrected save path and stronger controls.

android-powerup-save-01 exposed the legacy UUD regression and is retained; run 02
tests the corrected branch. ci-powerup-owner-01 omitted the required authentic
Vertigo resources and failed HXM admission; the corrected owner stages verified
resources into its own runtime directory. No input assets were modified.

sentinel-debug-01 reached its 120-second diagnostic watchdog without completing.
That is not a Debug lifecycle pass. Debug compilation and the source contracts
passed; optimized full lifecycle and rendered results are separate evidence.
The existing G2 rejected-save energy change and Android XL replay/startup-FX
limitations remain open. No campaign or general Android XL readiness is claimed.

## First published CI and compiler repair

#566 head bd57437b3d9ef6dc62f32c959bcd8101f01b314c, trusted run 44038224191,
preserved original 8/8 and passed Subzero's ordinary 120 frames in 8.063 seconds.
The expanded run was incomplete: 58 passed, four failed and two untested in
274.036 seconds of its 312-second probe budget. The unchanged 45-second start
guard left Evil Within secret 2 and Ironblade level 2 untested. This is not 60/64.

The private owner stopped at preview-build. Public Linux job 133886185592 exposed
an ambiguous ordered_json-to-json push_back in the new diagnostic, accepted by
MSVC but rejected by GCC. The next checkpoint explicitly constructs the
destination JSON type, matching existing observers. It changes no game behavior,
assertion or receipt field. The corrected diagnostic rebuilt successfully with
MSVC; Linux and private-owner validation require the new candidate's CI. Original
failed logs and SHA-verified artifacts remain under ci-44038224191.
