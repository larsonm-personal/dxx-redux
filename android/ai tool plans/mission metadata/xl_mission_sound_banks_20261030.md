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
