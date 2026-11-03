# Original Pumo rendered GuideBot pilot

The November 2 pilot tests whether the existing physical route runner can add
traversal evidence to the delivered original-Pumo playtest. It does not replace
ordinary-player combat, activation, save/load or campaign progression checks.

## Authored route and staging

Original fourteen-level pmines, level 1 (pm-hub.rl2), authored player start in
segment 115. Explicit target: trigger 3, wall 10, segment 76, side 2; destination
value 11 * 65536 selects PMC. Other original hub exits select level 2 and the
campaign ending. No original HOG, descriptor or trigger was changed.

The disposable native stage is extracted from the published November 1 runtime
tar, preserving mission/mod directory structure, Vertigo HAM, camera HOG and
effect assets. The metadata selector is a local one-level identity record with
no precomputed route; the engine computes its live route. It is not published as
canonical mission metadata or a completed-mine regression.

## Findings and changes

- Desktop startup did not initialize a new pilot. An empty profile selected
  original GuideBot routing and stopped at frame zero despite a computed plan.
  Desktop now initializes the same default pilot configuration as headless
- A desktop working directory did not isolate PhysicsFS writes. The explicit
  route user-directory option now selects the disposable profile before config
  loading, and skips interactive INI inheritance
- Windows startup redirected stdout/stderr back to CON, losing process logs.
  The rendered route invocation now preserves the caller's redirected streams
- The generic runner can select one explicit exit in Desktop mode and preserve
  its raw scenario result, executable hash and terminal screenshot separately
  from full-level regressions. Regression writes and multiple-level exit requests
  are rejected
- The existing exit fallback could report success after XL rejected a companion
  exit activation. It now reports unsupported activation instead of certifying
  progression. Stock trigger callbacks retain their normal success path

## Pilot result and limits

The corrected pilot physically travels for 548 frames (9.133331 simulated seconds)
to segment 77, position [-13809964, 0, 23924101], through the authored PMC exit
side. Terminal status: unsupported, "XL exit reached but companion activation was
not accepted". No exit objective is credited and no campaign transition is made.
Earlier diagnostic results reporting confirmed are superseded by this correction.

The companion uses the player collision radius, an immobilized/invulnerable
player sandbox and 160 percent test speed. The trace includes an assisted flare
at wall 8 and incidental trigger calls for triggers 2 and 7. General harness
assists also include direct switch activation, key collection and damage to
reactors/key carriers/bosses. No such assisted action proves ordinary player
combat or activation. XL rejects most companion-only trigger actions; teleports,
trigger chains, multiple exits and cross-level state require separate probes.

Retained initial evidence: temp/xl-traversal-20261102/pilot-pmc-qualified,
including exit-scenario.json, native result JSON, redirected logs and
desktop_sandbox/screenshots/scrn0000.png. The earlier staging/configuration
failures and false-positive receipt remain retained for diagnosis.

Next: pair this physical route with normal-player crossing of the hub doors and
PMC exit, including saved state. Expand GuideBot coverage only where its actor
and objective model match the authored interaction. Do not relax XL actor gates
or dispatch an exit to turn this result into a pass.

## Validation

Windows rendered build and scoped code quality pass. Runner discovery/selection
tests reject exit scenarios in Headless/Headed modes, regression publication and
multiple-level selections. Schema tests and both automation catalogs pass.
The positive Counterstrike level-1 control completes key, reactor and exit in
2374 frames; only that selected level was rerun. Its separate exit-only control
times out at the still-locked exit, preserving the reactor prerequisite. Neither
result changes canonical regression files. Android provisioning passes 95/95
launch/lifecycle steps on the unchanged published APK; this is not Android
validation of the new native harness code.

## November 2 phone-candidate replay

Replayed the same authored route after the depth/POG changes using a clean
committed native build from `4abb2b47dab5915d898959089fed8b826d669372`.
Executable SHA-256:
`eeca8a242b42453b3318101f5c4b476ac14d0ca083334182be44f50f67c7e7e3`.
Fresh disposable staging was extracted from runtime archive
`f54be3a7f2b6a6755a85b22d52bcb947379f5d54ecde8df8d329230217db4bbd`;
all 272 file hashes and the unchanged original mission HOG hash are retained.

The result remains unsupported after 548 frames, with the same segment77 and
fixed position[-13809964,0,23924101]. No objective was credited. The terminal
render was inspected. Native executable/DLLs, source-commit receipt, trace,
last position and screenshot are under temp/xl-phone-fog-20261102/guide-native-4abb2b4
and guide-pmc-4abb2b4; stage hashes are in guide-stage-hashes.json.

The paired exact-APK normal-player probe passes 36/36 hub door/PMC exit checks,
including save restoration and repeated laser/thrust activation. This separates
the companion actor limitation from normal player progression. Expand the
planner only if its actor model can express the required XL interaction; do not
bypass that actor gate. Further ordinary-player save/route probes have priority.
