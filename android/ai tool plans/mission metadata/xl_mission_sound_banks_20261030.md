# Owned mission sound banks

Continue the single-player audio foundation on `xl_more_74_sound_banks`, based on
`xl_more_73_static_explosions` at `be4de7ed0`, in the disclosed checkout
`C:/local/dxx-redux/temp/xl-batch2-20261027`. The main checkout remains on `cmake`.

1. Execute the pinned XL loader with full, reordered, duplicate-name, mixed-case
   and partial DSND banks. Verify the suspected stale-offset behavior for omitted
   samples before choosing a defined replacement policy. Keep source defects
   distinct from intended sample selection and document any deliberate fix.
2. Decode owned S11/S22 banks with bounded reads, native-name matching and the
   source's bank-over-WAV selection. Reuse the already verified classic PCM
   converter and shared playback backend. Keep stock GameSounds untouched;
   omitted samples must retain the base bank. Reject malformed selected banks
   atomically with the selected resource path and a useful reason.
3. Extend existing source and native integration owners. Check complete and
   partial banks, both rates, signed ordinals, missing-name and duplicate rules,
   WAV suppression, ownership, byte mutation, actual output and restore through
   stock. Add Android coverage through the existing publication/audio runner.
   Use real base-bank bytes as well as derived samples; do not claim a campaign
   playable from this component work.
4. Verify Windows, Linux and Android serially where workloads contend, refresh
   evidence, format the changed scope, publish a stacked draft and check CI.
   Keep bounds tied to measured workload, not retries that hide a failed check.

Side investigation: the public-save workflow repeats the same ordered-replay
and conversion helper tests in `build-public` and its dependent `save-integrity`
job. Inspect their dependencies before removing the duplicate invocation. Keep
headed, headless and ASan runtime coverage, which exercise different paths.

Previous tranche final result: static explosions pass 106 Linux controls and
the final Android 75-step run, including 24 pixel cases, seven output checks and
unchanged restoration of 234 protected files. PR #223 records those receipts.

Publication checkpoint: the user requested a push to start CI before final
qualification. Publish this branch as a stacked draft on #223. The source owner
passes 405 selection controls, but the new native HOG-bank output check fails
at its first sample (15 instead of 25). Keep that test enabled, diagnose the
installed playback path, then finish Linux and expanded Android qualification.
The earlier successful Android bank checkpoint predates the HOG fallback work.

The failure is now diagnosed: the native harness played `arnold_1` at physical
slot zero while the archive fixture replaces `laser01`. Resolving that name and
asserting the actual voice binding passes 311,296 bank and 16,384 archive-WAV
output samples without changing production playback. Finish the pending
platform checks before upgrading the draft's qualification status.

Final Windows audio owner and Android bank regression now pass. Android's full
four-script owner passes 65/35/75/119 steps; after the duplicate-header correction
the focused bank run passes 119 steps, 15 output checks and 361,472 samples at
48 kHz, restoring all 234 protected files unchanged. Final Linux acceptance
passes 106 required controls, 16 original-format witnesses and three modern
source levels. The expanded audio owner takes 138.47 seconds within its
unchanged 150-second bound; total CTest wall time is 408.09 seconds.

The CI side change removes only the second invocations of the pure ordered-codec
and replay-conversion helpers from `save-integrity`. Its `needs: build-public`
dependency already requires those exact commands to pass on the same commit.
All 27 and five helper tests pass locally (1.117 seconds combined wall time).
This is a small duplicate cleanup, not a substantial CI speedup; no headed,
headless, ASan, graphics or engine replay coverage is removed.
