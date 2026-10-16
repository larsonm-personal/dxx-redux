# Chunk 0471 music and save consumer trace, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Previous turn closed the assigned-source gate with scopePASS54/sourcePASS89. This partial consumer checkpoint follows actual automation/music command application, paired save metadata restore and native track/render oracles. No product/test edits or execution, payload reads or runtime acceptance.

## Command acceptance versus application

The parser defaults music expect_success to true. STEP_MUSIC_CONTROL next/previous/play call actual shared songs functions; source calls android_music_set_source. The step compares Boolean result with expectation and fails before advancing on mismatch. The source setter maps mission/MIDI/CD/files to type/preference, then only enqueues; return1 proves accepted queue admission, not applied playback or disk persistence.

The bounded64-entry mutex queue stores one coherent source command. android_music_control_apply_pending drains it on the game thread, applies MusicType/prefer, invokes config/player saves and restarts the current level/title song, then publishes a snapshot. Paired game frame hooks consume it before continuing that frame. The test's later source-type/preference waits therefore check actual applied engine state beyond enqueue success. Full idle/pause/menu consumption and queue reset/acknowledgement/lower audio lifetime still require their material existing owners; do not revive blanket historical no-queue claims.

music_save_config invokes void WriteConfigFile and player writer without checking completion here. android_music_replay_current uninitializes songs then calls the level/title play entry point without a result contract. Preserve runtime source/state assertions but do not infer successful disk write or actual audible source from the command result.

## Saved preference precedence

state_android_write_save_metadata captures type and prefer_mission, representing mission preference with ANDROID_SAVE_META_MUSIC_MISSION; it then writes the Android metadata block and propagates a failed write. Paired state writers reject metadata failure and failed close in the captured boundaries. Actual D2 state_save_all calls state_android_save_to_path and only shows Game saved on success.

The shared save-to-path wrapper drains periodic checkpoints, stages the engine save, validates it, publishes it, writes the last-save-set record, removes failed stage and restores save kind/thumbnail state. Its validator/publisher/checkpoint/index callees remain pending; the selected wrapper is not whole crash-durable save acceptance.

The metadata reader checks footer tag/version/size, exact reads, format validity including bounded music enum and cursor restoration on success. Its failure cleanup seeks back without checking the seek result. In-memory read uses the same validity predicate. Both restore consumers apply the metadata only when it exists. restore_music_source_from_meta validates music enum, decodes mission preference, detects an actual conflicting current pair, assigns both fields and calls replay_current. Thus the D2 test's deliberately conflicting mission source is meaningful for saved preference precedence; missing metadata normally leaves current state and should fail the final CD2/preferfalse assertion.

Actual requested save identity/menu selection/result generation and import/launch ownership, no-stale-save proof, playback success and corrupt/missing/older trailer behavior require existing owners before disposition. No save or mission payload was opened or executed here.

## Native track and render oracles

Whole songs_android_shared.c was read. Built-in next/previous wrap valid level-song indices and only publish Song_playing after songs_play_file succeeds; explicit play bounds the index and likewise propagates failure. Redbook/custom branches retain their own source APIs. songs_get_track_info writes type from GameCfg before checking track availability, so music.type can be meaningful without a playing Redbook track. Its built-in index/list output explains tracks5/6 and the ordered first track; the complete lower songs_play_file and asset-conversion/render path still need tracing.

Introspection reads actual GameCfg/prefer, songs track/list and TSF getter values; it does not use the UI music snapshot for these assertions. Track-list size allocation/parsing and exported-producer exception ownership remain conditional under existing owners.

The TSF normal new-song path resets clip/sample/peak/voice counters before starting rendering, resets playback/paused/finished state and rejects render-thread start failure. HMP and ordinary render blocks update peak/clipping counters from rendered int16 PCM before output volume. The getters use atomic loads for counters/gain. EQ reads the synth; full synth lifetime/renderer selection remains pending. Nonzero peak with no clipping is render-side nonempty/headroom evidence, not output-sink/audible delivery or source-byte identity.

The D2 balanced SF2 calibration branch assigns MUSIC_D2_SF2_BOOST_DB. Its actual current value is0.0f, and gameplay gain is-7dB; the test's source_gain_db0 assertion is consistent. Do not infer a contradiction just from the constant's BOOST name or old calibration history. Explicit SF2/EQ fixture remains distinct from FM/HMQ comments and renderer-family coverage.

## Read limits and next

The broad music_playback_levels header display clipped unrelated calibration comments; only complete lines27-35 defining the inspected gains are accepted. Save metadata read failure tail250-253 and save wrapper closing tail were explicitly recovered. Selected paired restore/write/frame-hook bodies are context, not whole state.c/game.c acceptance. The entire songs helper is current source context, not a whole lower audio subsystem.

Next actual MIDI preview/Setup ordered command and renderer/EQ selection producers, save-menu identity/completion and lower playback path; MSAA scene/menu probes/render composition and menu scale/interaction; harness params/edition/freshness/deadline/cleanup; whole existing BR/quality/original plans. Then unpooled synthesis and immutable publication/Drive proof. Consumer gate remains open; canonical0471TODO/sourcegateonlyclosed. No new root/remediation/status/rating, ledger append, runtime proof or inherited saving. Broader diagnosis and closure remain required.
