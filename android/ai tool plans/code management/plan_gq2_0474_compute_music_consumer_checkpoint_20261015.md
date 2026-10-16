# Chunk 0474 compute and music consumer checkpoint, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. This is a partial consumer checkpoint; the primary consumer, owner, disposition and publication gates remain open. No product change or runtime acceptance.

## Compute dialog and scheduler contract

The assigned fourteen-line script checks Foreground compute button presence, taps it and waits. It does not assert the Foreground Compute dialog title, a worker request, CPU duty, Close/dismiss behavior or cleanup. Preserve the fixture as a launch smoke check; do not use it to accept the complete scoped-compute feature.

AdvancedSettingsPage keeps showComputeFaster in remembered section state. The button sets it true. While the dialog branch is composed, DisposableEffect invokes onSetComputeFaster(true); disposal invokes false. Dismiss and Close set the flag false. The callback is passed into the precompute section and SetupActivity supplies routeMetadataCoordinator::setComputeFaster. These selected call sites establish the intended binding, not whole activity lifecycle acceptance.

Coordinator setComputeFaster is synchronized and idempotent. It updates the volatile flag and diagnostic event, starts when no runner exists, or cancels the previous runner and restarts from its completion callback only while runner identity still matches. Preserve this completion/identity condition. A previous runner is not immediately replaced at the cancellation call. This excerpt is not whole cancellation, worker cleanup or process-generation proof.

RouteMetadataCpuPolicy maps ordinary launcher work to 20 percent and compute-faster work to 100 percent. The coordinator passes that policy result to LevelMetadataAnalyzer.analyze; the request builder clamps cpu_duty_percent to 1..100. Priority and duty remain separate request fields. The mode also bypasses the 750ms inter-work wake wait. Existing game-running and severe thermal-pressure pause reasons remain in place. A request asking for 100 percent is not measured CPU utilization or proof that useful work exists; native worker enforcement and cancellation remain pending.

## Native music numbering and observations

songs_play_specific_track selects behavior by MusicType. Builtin uses the BIMSongs index and bounds it against SONG_FIRST_LEVEL_SONG and Num_bim_songs; successful playback updates Song_playing and overlay notification. Both game headers define SONG_FIRST_LEVEL_SONG as 5. Builtin list array element zero is therefore track id5, and element one is track id6. This explains why a request to play track6 can select the name asserted at music.tracks[1]; it is not an off-by-one error by itself.

Redbook delegates to its track API and lists audio tracks from1; custom jukebox uses zero-based indices. Do not apply the builtin offset to all music sources. The current-info path resolves decoded mission names with filename fallback; redbook and jukebox have separate providers. These read bodies do not establish which runtime MusicType or bytes the device loaded.

The compressed-music fixture checks launcher analysis metadata and in-game names, switches to track6, waits250ms and checks the current name. It does not assert duration, audible output or source identity. Preserve its separate analysis and native-label roles; metadata and playback provider ownership remain to be reconciled. No new issue is declared solely from this selected trace.

## Evidence and continuation

The companion progress JSON captures exact UTF-8 context bytes, inclusive ranges, whole-file hashes, HEAD, primary plan and canonical ledger identities. An independent audit rereads those bytes. Overlapping contexts count as physical instances rather than unique source coverage. Whole-file hashes identify revisions and do not claim whole-file reading.

Initial broad locators and clipped combined output earn no body credit. Guessed LevelMetadataAnalyzer.kt and d1/include/songs.h paths were absent; actual implementations are LevelMetadata.kt and d1/main/songs.h plus d2/main/songs.h. Bounded successful rereads supply the recorded contexts.

Mounted Drive directory exists and contains the evidence ledger and hash sidecar; both local and mounted file sizes were observed as828701376bytes. This checkpoint does not perform a new Drive backup, hash the mounted file, or claim cloud synchronization. Prior0473 receipt remains historical. No evidence-ledger append or canonical queue/status/rating change.

Next finish preview wrapper/native render and asset lifetime, boss HUD preparation versus rendering and collision helper, launcher same-track metadata predicate/native playback-provider binding, harness freshness/deadline/cleanup, then whole existing owners/prior plans and direct unpooled disposition. Canonical0474 stays TODO; broad cleanup and inherited-file minimization remain incomplete.
