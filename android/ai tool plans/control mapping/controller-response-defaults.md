# Controller response defaults by mapping

Use Fine for pitch/yaw mappings (including single-direction trigger mappings) and Linear for slide, throttle and roll. Explicit stored curves remain authoritative; defaults fill missing settings and apply when an editor changes the analog action

- [x] Add shared mapping defaults and preserve explicit values on load/save
- [x] Set explicit response values in the shipped controller preset
- [x] Apply defaults at mapping selection; keep stick response edits pending until Save
- [x] Verify mapping transitions, stored custom responses and the shipped preset; build and format scoped changes

No configuration version bump or regeneration of existing user settings

Fine means center=0.25, expo=0.5; Linear means center=1, expo=0. Preset buttons and mapping defaults share these values. Missing response records use the mapped action, including half-axis trigger actions; explicit records remain authoritative even when they differ from that action's default

Defaults are applied when selecting a different analog action, including switching from digital to analog control. Reopening, saving, reselecting the same action, changing inversion, or loading an existing slot does not reset the curve. Stick dialog responses are committed alongside their mappings so Cancel also discards an automatic curve change

Validation: Android x86_64 debug build and 21 selected Kotlin tests pass, including mapping transitions, custom curve preservation and the actual bundled JSON preset. Scoped formatting/lint and git diff --check pass

Emulator UI actions followed by assertions against the saved controller_config.json pass for: applying the preset and customizing a stick curve, reopening/saving unchanged mappings, cancelling a remap, assigning a look trigger (Fine), and customizing/saving that trigger. Temporary scripts and saved configuration evidence are in temp/controller-defaults-ui

Additional UI checks for reselecting a trigger action and remapping to throttle/roll were not completed: compact-dialog automation skipped an off-screen action, then the launcher timed out during startup after an emulator density change. The original density was restored. These mapping transitions and same-action preservation are covered by the passing Kotlin tests
