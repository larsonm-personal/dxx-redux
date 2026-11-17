# Legacy smoke trigger dispatch

Checkpoint on xl_recovery_20261117_07, parent
126c84606eb5895c9e50deb4376905df4cf8d441 (#568), stack 471.

The pinned CTrigger::Operate has empty action bodies for types 20 through 24.
These are legacy life, speed, density, size and drift controls. Empty dispatch
does not bypass actor eligibility, scheduling, one-shot/alternate flags,
object-owner retirement or saved state. Type 27 brightness remains outside this
qualification. Modern particle objects, their resources and rendering retain
their independent admission requirements.

The existing camera source harness already extracts the actual full dispatch
and checks these five smoke actions in both source Debug and Release modes.
The native wall and object dispatchers now use the existing lifecycle around
the empty bodies. No smoke graphics, fake assets or authored mask edits are
introduced. Boiling Point 5 loses its wall action 22 and object action 21
blockers, but its equipment masks still reject; this is not another passing
original level.

## Qualification

Private receipt root: E:/dxx-redux-builds/xl-recovery-20261117.

- smoke-source-controls.log: actual source Debug/Release flags, actor gates,
  deterministic/random delays, boundary completion and permanent rearming PASS.
  Existing texture, countdown, disarm, exit and cue comparisons also PASS.
- smoke-inventory-direct.log: native Debug inventory contracts PASS, including
  positive wall/object 20-24 admission and negative type-27 admission.
- smoke-native-02 and smoke-debug-02: complete existing trigger-flags owner PASS
  in optimized and Debug builds, under unchanged 80-second process watchdogs.
  Six dispatch types each cover all 256 flags, unchanged live walls/objects/SIM
  RNG, actor/link rejection, master chains, deterministic/random pending delays,
  disk restore through stock, permanent rearming, autoplay and removed attached
  actor continuation. The five smoke cases additionally verify their authored
  robot binding, event activation, retired owner and restored earned flags.
  Existing 558 source camera placement/orientation comparisons also PASS.
- android-smoke-save-01: rebuilt actual Android libraries and passed seven D1
  and twenty D2 native save cases on emulator-5554. Isolated native fixture;
  no APK install or app-save modification. This is not Android smoke gameplay
  or a general Android XL input replay qualification.
- smoke-original-inventory.json: all seven original Boiling Point members decode.
  Relative to the earlier object-audit receipt, only the two smoke blockers and
  the already-qualified Subzero sentinel blockers disappear. Equipment blockers
  and source payloads remain.
- smoke-affected-01: ordinary developer-OFF probes still reject Boiling Point
  2, 3 and 5. This expected remaining-deficit result is preserved, not called a
  readiness pass. Full candidate CI is required after publication.
- Both automation catalogs PASS: 99 JSON, 315 PowerShell, 550 support scripts;
  428 top-level runner entries.

Optimized diagnostic binary SHA256:
99352015e5d5d5e0b28896de428a53893c5afc0f1d4e7b299d7567de63dabfeb.
Debug diagnostic binary SHA256:
e17b730e8ebf1582cb809c099e12fee6ce09bd00e69af0b94cf2673e7893e5fc.
Pinned reference main/trigger.cpp SHA256:
74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852.

The initial native/Debug attempt assumed disk record zero remained runtime
entry zero. The source owner/type sort disproved this; the fixture now discovers
and verifies the unique authored binding before checking its transitions. Both
failed receipts remain. Initial compile logs retain the missing direct include.
No assertion was dropped and no timeout was extended.

Each smoke type has its own derived fixture. The original Android camera HOG
and descriptor hashes remain exactly 60809f660ad1cda0427b7235d2beb1f229b72c7f0e2d37f2a4b1ee80eb771f09
and b545bcf6cbd53be91c5ea750f6d6187f08cad253da9274efd40b084d3cd1d2e7.

## Latest complete measurement before this checkpoint

#568 trusted run 44043915482 measured 61/64 in 246.65 seconds of a 354-second
remaining probe budget, with original 8/8 preserved. The frozen cohort, seed,
resources, 120 unassisted frames, 45-second case watchdog/start guard and
ten-minute CI job are unchanged. Expanded artifact 14310808274 has SHA256
4ea71260b0a610b60b6b748034e30cd00884a8311750fe882c2040e60c2d39d7;
original-eight artifact 14311430248 has SHA256
e9f67cf8aca97d5b76d88095d5cc6f041a2617863d0a0b7e0bda2c2af71d6f74.
The separate Windows frozen64-head06-01 run was incomplete: 57 passed, three
failed and four untested in 317.437 seconds of its 360-second budget. Its
remaining-budget guard is preserved, and its result is not combined with CI.
