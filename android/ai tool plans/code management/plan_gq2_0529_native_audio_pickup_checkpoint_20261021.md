# Chunk 0529 native audio and pickup checkpoint, 2026-10-21

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Previous turn completed source gate and consumer ingress. Revalidated all seven assigned sources,16 ingress contexts, five canonical files and HEAD before continuing. This continuation adds34 fresh contexts3611 range-lines, including eight whole files, plus one whole existing owner. With ingress,50 source contexts4731 range-lines/seventeen whole files are qualified. Consumer gate remains open

Fresh whole files: test_autoselect.cpp, imported D1 weapon bridge, HMP event exporter, offline TSF renderer, production autoselect runtime adapter and three DOS reference fixture metadata files. Selected ranges cover actual PCM producers/callbacks/start-stop-seek handoffs, both games' weapon/powerup hooks and selection behavior, both CMake registrations and native test dispatch. Recoveries completed clipped bridge and D2 CMake segments before coverage was recorded. No selected range qualifies its entire containing file

## Existing MIDI preview defect remains

Whole current BR-0276 remains OPEN. MIDI preview render_midi_frames stores playing0 and output_enabled0 at producer EOF. The render thread then publishes any final samples to the ring; the callback reads only while output_enabled remains nonzero. Final and older buffered PCM can therefore remain unread. Position publication still uses the generated timeline rather than consumed audio. These are existing BR-0276 semantics, not a new finding or owner definition extension

Current CD preview differs materially from the historical owner description. It publishes source_finished after the final ring write, continues callback consumption, and requires both an empty ring and no queued OpenSL buffer containing audio before audibly_finished. Preserve that repair. It does not prove every existing position/quiescence validation or fix the separate MIDI path. No tail-loss duration or hardware shutdown behavior was measured

## Behaviors to preserve

TSF, MIDI preview, CD preview and Redbook producers check ring capacity in samples before bounded stereo writes. Selected callbacks zero-fill underruns. TSF and Redbook publish EOF after their final samples; Redbook additionally qualifies completion and stale chunk publication by request generation. Do not collapse source EOF, ring drain, hardware buffer drain and terminal cleanup into one flag

TSF play calls free/join/unhook before starting a replacement song and hooking output. Preview seek/reset paths coordinate producer state and callback consumption; stop joins the producer and shuts down output before final reset. Redbook preserves asynchronous request handoff and cursor discard. Producer apply_pending_request also updates the reader cursor; full request/callback ownership must be reconciled before any additional race admission or broad safety claim

Native D1 and imported pickup code retain first-pickup suppression, better queued-weapon rank and logical quad identity16. Selection maps logical quad back to the laser inventory slot. The imported bridge validates exact native order permutations and retains native cutoff-aware cycling, availability and classic fallback; ordinary D2 still uses its separate engine behavior. Handmade comments and engine ownership remain

Actual autoselect integration code exercises PHYSFS and rewind-memory records, validation without applying state, truncated and opposite-endian input, queued quad identity across restore and later pickups, and release-time switching. Ordinary D2 rejects native quad identity without changing its current state. Both game CMake registrations link test_autoselect.cpp into test_upstream_compat. Source inspection establishes these assertions exist, not that they ran here

The event exporter uses production hmp2mid_playback_mem and TML loading, distinguishes legacy conversion, emits both MIDI bytes and loaded-event serialization, limits HMP input and checks write/seek/close outcomes. Offline rendering uses shared MIDI scheduling and HMP retained-state helpers with explicit little-endian48kHz stereo16-bit output, fixed synth settings and bounded source/duration. These diagnostics do not establish Android device playback or original-synth/listening parity

Reference metadata distinguishes game08 cold-start50.010-second opening without173.9-second repeat coverage, game02 retained-prefix279.6-second window and game07 device-selection250-second window. Optional second capture and20-second listening render remain separate scopes. No reference media payload was decoded, rendered, listened to or hashed

## Evidence and continuation

Native evidence: temp/general_cleanup_20261006/gq2-native-audio-pickup-progress-0529-20261021.json, SHA2562373c5974d5f1513aba00ae4d91338c21f56262d7cecd26f23f9b76ee17aed52

Whole owner and MIDI EOF supplement: temp/general_cleanup_20261006/gq2-preview-owner-progress-0529-20261021.json, SHA256ccbea7c0961e3f77c5eba8e04c30148c98cf42acdbbbac23eeae612bc72860c2. This flattens all50 contexts and binds the physical4223-byte BR-0276 block without editing it

Independent read-only PASS177 audit covers both artifacts, parent chains, current source/range/owner identities, canonical baseline and static EOF/drain anchors: temp/general_cleanup_20261006/gq2-native-audio-pickup-audit-0529-20261021.json, SHA256740a27dc22d46b1dc7410e69e7c070ac0399995434770d0480f7069dfcfb0bd0

Remaining: ending staging/native text/movie consumers; remaining production HMP/TML retained-state/scheduler ownership; sanitizer actual callee/platform-build/master budget/status/diagnostic publication inventory; replay actual runner/assets/build/master with exact qualified prior reuse; complete PCM/preview/Redbook lifecycle ownership; whole existing priors/raw quality rows/direct unpooled disposition; exact terminal publication and mounted Drive proof

All assigned sources, qualified contexts, five canonical files and HEAD remain unchanged. Source gate only; chunk0529 TODO/canonical528 DONE99 TODO627/327 findings313 remediations unchanged. No new finding/status/rating/runtime/saving/canonical append/Drive generation/product-test-helper edit or execution/build/test/import/generator/formatter/device/network/media/security/resource/allocation/fault probe/staging/commit. Broader queue/sweeps/preflights/investigations/current-head supplement/closure remain; goal active
