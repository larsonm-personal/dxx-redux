# Chunk 0475 demo music consumer checkpoint, 2026-10-15

Diagnosis and plans only. Source gate closed; four later gates remain open and0475TODO. Actual demo fixture consumer boundaries traced without downloading, importing media, running the app or executing tests. No new finding/remediation/status/rating or inherited reduction.

## Default offer and selected set

DEMO_DOWNLOADS orders PC D2 first, D1 second. visibleDemoInstallerOffers filters ready games and hides all when the preference is disabled. The UI iterates that order with identical Download and install labels. In the fixture's intended empty selected set, the first matching button is therefore the D2 PC installer. This is actual source ordering, not evidence that the current device selected that offer or received expected bytes.

create_set validates/registers inside AtomicFilePublication.transaction; Setup catches IllegalArgumentException as already exists. delete_set removes registry/active selection inside the existing content/publication locks, checks recursive deletion and revokes unretained permissions. switch_set validates registration, saves active selection, checks actual selected name and returns early on rejection, otherwise writes active path/reconciles. Preserve these current transaction/checks and existing completed-command failure ownerGQR0277. The script uses create/delete/create to register and clear a directory retained by the runner; it does not assert the active set name after switch, so d2.ready false alone is not exact set identity.

Setup introspection resolves activeSet/setDir and publishes d2.ready from launchDataReadyForGame. That function requires every required FileStatus.found and only adds the special D1-in-D2 edition check for that target. checkFiles considers matching primary/alternative local names or retained SAF entries, with manifest metadata separately attached. Thus readiness is a required-presence contract, not archive checksum/content validation or native load success. Subsequent actual menu/mission/game/audio predicates provide independent later evidence when executed.

## Download, extraction and publication boundaries

The ordinary button captures the displayed demo and selected setDir, downloads to shared tmp, aborts on false completion, calls installDemoArchive and handles extraction/install exceptions, cleans tmp, selects the game and refreshes. setupDownloadFile uses HTTP200,15second connect/30second read timeouts, bounded shared LauncherFileCopy with expected Content-Length/progress, reports exceptions and disconnects in finally. These finite transport timeouts are not a demonstrated complete download deadline.

Whole DemoInstallerPackages stores known archive names/checksums/expected output names. The selected download and install bodies do not compare the downloaded archive to the known package sha256 before extraction; avoid treating mere presence of the checksum table as enforced admission. Complete downstream extractor verification is not claimed by this checkpoint and existing source-identity/admission owners must govern final reconciliation.

installDemoArchive dispatches by ZIP versus SIT/HQX, rejects extraction errors/empty output/missing expected names, stages under a unique publication sibling, carries prior assets manifest, copies extracted files and their recorded hashes, then invokes publishStagedArchiveFiles. Preserve these implemented validations and cleanup. Whole selected publication helper backs up prior files, moves or copies staged files, records publication ownership, attempts rollback on exception and preserves backup if restoration fails. This is stronger than historical direct final writes, but not whole-generation atomic visibility or all failure/concurrency/resource acceptance. Do not remove current backup/rollback/checked-copy behavior or add a duplicate transaction framework.

The script has no explicit _deps archive pin and follows the ordinary download UI; the intended release URL and known-package metadata identify source intent. Actual served-byte/published-generation/device identity remain separate future acceptance. No network request or source/payload hash was performed here.

## MIDI catalog projection

Whole MidiEnumerationBridge uses nativeDataLock around native enumeration, strict Kotlin decode and an incomplete/error result on invalid response. JNI uses the repaired strict UTF8 helpers and scoped frees. Native enumeration searches case-insensitively for descent2.hog first, then d2demo.hog, and emits d2-builtin before later D1/Vertigo/mission sources. Builtin catalog source construction uses hog_midi_catalog_load, reports catalog-load errors and frees the catalog.

Track entries use catalog names, attempt per-track reads/duration/metadata and can retain duration-1 or missing metadata for a failed individual read. The script checks first sourceid and at least8entries, not midi_catalog_complete, every track duration/decodability or exact source HOG generation. Setup projection exposes complete/errors but the fixture does not assert them. Preserve shared native catalog/format authority and strict JNI codec; count/name-only catalog checks are not full playback proof.

## Actual synthesized sample signal

mix_play_file dispatches prior completion and frees previous music before choosing PCM versus MIDI/HMP. On the MIDI/HMP path it resets synth/timeline state, resets clip/sample/peak/max-voice diagnostics for the new song, clears pause/finished state, sets playing, starts the render thread and hooks the SDL_mixer callback. Preserve this song-scoped reset rather than calling peak a permanently accumulated process statistic.

HMP and ordinary MIDI render paths update peak from actual synthesized short PCM before output-volume scaling. The background producer writes rendered samples into its ring; callback can output silence while paused/background-paused, reads ring data, fills underruns, then applies volume and captures a separate output-volume diagnostic stem. It marks completion only after final queued samples drain. The script's tsf_playing and peak>0 therefore demonstrate active synth scheduling and nonzero generated MIDI samples since reset when observed, not delivery of a specific number of callback samples, nonzero post-volume output or audible speakers.

Initial level playback and intro-skipped menu relaunch are distinct predicates. Neither asserts selected track name/sample identity, generation ID, callback count growth or captured output stem. Ring/start/stop and lower audio thread ownership remain existing acceptance scope; selected source reads do not establish every race/cleanup/failure path. PCM's separate branch was read as context but this fixture is builtin MIDI; no unrelated PCM diagnostic defect is admitted.

## Prior plan and continuation

Whole plan_pilot_long_hold_delete_hardening_20260624.md records why game_running=false replaced a fixed500ms relaunch sleep. Preserve that semantic wait and launcher-observable process/activity boundary; earlier successful builds/device tests remain historical and do not expand present prompt-only assertions into durable deletion coverage.

The three consumer checkpoints now cover graphics, pilot/Guide-Bot and demo boundaries. Next consolidate exact contexts with source evidence, read whole existing ownership definitions and material prior plans, then direct unpooled disposition and final publication. Avoid further speculative subsystem expansion. No primary/canonical/ledger mutation, no new Drive backup/cloud-sync claim. Broad goal active.
