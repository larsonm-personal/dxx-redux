# Protected playtest emulator recovery boundary

The Pumo runtime owner now passes `-NoEmulatorRecovery` to both warm and cold
`run_test.ps1` phases. A failed health check or launcher startup fails the run
before emulator restart or fallback-profile selection. Ordinary disposable CI
runners retain their existing recovery behavior when this option is omitted.
Physical-device app-only recovery is unchanged.

The owner records the selected emulator's AVD name before backup/staging and
checks it again before force-stop or restoration. A missing or different name
refuses those writes. Its backup remains on the original profile if restoration
cannot proceed. This does not establish ownership or authorize a user profile:
callers must still select an explicitly owned device and bind its AVD directory
and recovery name. An AVD name check is not a cross-machine identity guarantee.

Reason: a task-owned host-GPU emulator became unavailable during an earlier
normal-play probe. Generic recovery fell back to a preexisting profile on the
same serial, and its game prelaunch reset configuration/preferences before the
probe failed at pilot selection. The task-started replacement process was stopped;
there was no complete pre-run snapshot for that profile. The original user device
on the other serial was not touched. Disabling recovery in the protected owner
prevents this runner from replacing its selected profile, and the final identity
check also refuses restoration if an external replacement appears.

Validation: mocked tests cover healthy/no-recovery use, unavailable-target and
launcher refusal before recovery, unchanged default recovery entry, same-profile
restoration and rejection of changed/missing AVD names. Strict-mode helper loading
retains an explicit caller policy and safely defaults an absent option. Protected
healthy-device replay passed 36/36 steps in 133.599 seconds on the unchanged
published APK, with identical before/after resource, pilot and settings hashes.
Receipts are in `temp/xl-original-hub-activation`; prior coverage is retained in
`temp/xl-traversal-20261102/protected-hub-B`. Failure-path tests do not stop or
replace any real emulator.
