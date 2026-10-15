# Chunk0420 SoundFont integration default oracle plan, 2026-10-14

Diagnosis-only conditional repair. Proposed GQF-0312/GQR-0298,PRIMARY47 MEDIUM23/0/4/10/10,unpooled;IDs are not admitted until canonical publication. Later GQ2-0531 assigned test-file review must reuse the admitted owner.

Current witness

SetupActivity music_preferences_reset -> MidiPreviewBridge -> GameSettingsPreset -> SoundfontStore.resetPreferences selects the bundled path and DEFAULT_RENDERER=sf2,wet effects and cleared EQ overrides. Current JVM defaultAndBothPresetsUseBundledSoundfontWithoutDeletingImportedFonts explicitly checks sf2 for both presets and keeps imported assets.

test_soundfont_profiles.py113-120 instead requiresymfm after ORIGINAL/DEFAULTS resets. test_soundfont_release_download.py generates enter_launcher and expectsymfm after confirmation cancellation and after download without first selecting FM. The actual launcher harness classifies its first action,stops app and clears dxx_prefs before launch;the current defaultsf2 conflicts with its expectation before release transfer. Both are stale test assumptions about one retired default,not two production defects. Source-only false-failure witnesses;no app/network/test execution.

Conditional implementation

- Preserve production sf2 and current bundled identity/effects/EQ reset policy
- Update the profile reset oracle to assert the intended sf2 result for both presets,including wet effects,cleared EQ overrides and retained imported assets;keep wrong-state assertions strict
- Make release-download renderer retention intentional:explicitly selectymfm before its cancel/download checks,retainymfm expectations through those operations,and keep its later explicitsf2 selection and actual downloaded-bank playback check. This preserves independent FM-retention coverage without assuming the old default
- Preserve release hash/URL/source metadata assertions,Info flow,cancellation/native validity,actual downloaded PCM progress,optional paired D1/D2 selected-font checks and saved manifest/preferences cleanup
- Keep imported/local assets and source provenance isolated. Do not copy actual output into expectations,drop renderer assertions,skip resets,add a compatibility shim,or restore the older production default

Future acceptance

Use the maintained actual profile runner with both preset paths and optional paired games plus the actual release-download runner on a prepared test device. Preserve explicit missing-data/transfer/activation/cleanup failures. Reuse current JVM reset/persistence/rollback/EQ coverage where relevant. Disposable negative controls must reject a wrong reset renderer,bundled selection,effects/EQ state,and a download/cancel operation that changes the explicitly selected renderer. Recheck native selected-asset identity and saved metadata;no exact timbre or general device performance claim from these tests.

Dedup boundaries

BR0016 preview generations/lifetime,BR0543 aggregate/app teardown,BR0553 unmatched filter,BR0594 duplicate shell all-mode and BR0607 private corpus remain independent. GQF0299/GQR0285 automatic-profile eligibility has a current pre-ring source repair with actual-entry acceptance still pending. GQF0311/GQR0297 numeric-versus-ordered cheat oracle is a different representation cause. Selected root/title and current source review found no existing same SoundFont default-oracle owner. No existing root status or rating is changed here.

Related guide corrections belong to plan_gq2_0420_test_soundfont_guide_simplification_20261014.md. Broader source/media/device/license evidence limits remain explicit. No product/test/helper edit,build/test/download/device/audio/network operation or inherited saving by diagnosis.

Canonical admission2026-10-14:GQF0312OPEN/GQR0298TODO after publicationPASS3200;one two-helper cause,PRIMARY47 unpooled from guideREFERENCE0. No implementation/runtime acceptance;future GQ2-0531 reuses owner.
