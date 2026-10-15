# Chunk0433 native audio and identification acceptance continuation, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. No code changes, execution or runtime acceptance.

## Preserve actual native admission

Native playlist input is limited to64KiB; CUE materialization uses the shared1MiB contract. Producer/native limits agree at eight sources and100 aggregate tracks. Current parser rejects failed source admission, excessive source/BIN/track counts, missing cue/bins, invalid suffix and a whole playlist with no playable audio. RBAInit distinguishes missing playlist from hard rejection and does not fall back to GOG after a negative parser result. One playable track is admitted. Retain these archived BR-0326 repairs; do not repeat prefix acceptance or unconditional legacy fallback as current behavior.

This does not make the handwritten parser a complete safe JSON implementation. Existing GQF-0194/GQR-0181 already owns unsafe unknown-value scanning past the NUL sentinel, unchecked numeric accumulation/atoi, discarded escape introducers and silent identity truncation. Current pj_string/pj_long/pj_skip_value retain those paths. Keep the complete existing remediation, including bounded grammar, typed fields, exact strings and rejection before source I/O. Do not add another finding or claim malformed-input qualification from source inspection.

Existing GQF-0193/GQR-0180 already owns the exact BIN cleanup edge: bh_open succeeds, but num_bins is advanced only after bh_length succeeds. The size<=0 branch closes only earlier counted handles; whole-playlist cleanup also uses that count and then clears storage. Establish ownership before a failing size query and prove repeated-failure descriptor stability. Later-open failure and preview's immediately counted successful opens are separate controls. No runtime leak probe ran and archived BR-0326 was not reopened.

RBA handles /proc/self/fd using dup/fdopen and closes a duplicated fd if fdopen fails. Native capacity and ordinary fail cleanup do not preserve a previous live playlist generation automatically; registry/PFD publication remains the preceding0433 plan's separate ownership scope.

## Identification progress and current fingerprint contract

All four actual fingerprintAndMatchDisc overloads filter audio tracks and emit (ordinal+1,audioCount) before each fingerprint call. Names remain keyed by physical trackNum; fileIndex selects the multi-BIN descriptor. Multi-local and multi-SAF overloads hold opened descriptors through the scan and close them in finally. Selected empty or missing-descriptor behavior returns/skips names, not a completed-match acknowledgement.

The actual Setup registration route posts clamped track-position text to the main handler, catches identification errors and still registers an otherwise valid audio source with optional names. It persists selected BIN grants, makes an owned local CUE copy, records plural BIN URIs and physical audio track numbers, saves the source and then enables Redbook before showing registered. These are separate resource/config/UI steps. Registration and queued progress updates have no freshly qualified complete generation or cancellation acknowledgement; preserve prior registry/grant/launch ownership findings.

Current nativeFingerprintDiscTrack validates the complete span against fstat, checks offset representability, reads exactly the full track in64-sector chunks and rejects incomplete reads or failed feed/finalize. The stream uses checked size_t frame totals and bounded int sample chunks. Its current behavior feeds all frames to Chromaprint and derives duration from all frames; it is not the archived BR-0048 resolution's120-second-prefix path. Preserve the current full-track contract and removed full-track raw/PCM copies. No whole-library memory bound, input identity oracle, interoperability or cancellation proof is claimed; do not restore a prefix as cleanup based on historical wording.

## Preview route and retained startup repairs

Current MusicPicker chooses one/multiple local paths or SAF descriptors, opens the complete SAF list, calls the matching native start and closes caller descriptors in finally. Native fd start duplicates handles and counts each immediately. The CUE parser selects the Nth audio track and its fileIndex; native startup validates count/rate/track bounds and closes admitted BINs on selected failures. Kotlin marks playing only when start returns true.

CdPreviewBridge serializes controls and generation-reserved start against stop. The selected native startup checks each OpenSL create/realize/interface/callback/initial-enqueue/play-state result, destroys partial local objects in reverse order and publishes globals only after success. Failed render-thread start invokes complete stop. Preserve archived BR-0275. Selected startup and stop are not complete callback-drain, end-of-playback, provider cancellation, MediaSession or hardware fault acceptance; those broader lifetime owners remain to reconcile.

Current JNI path conversion uses the checked UTF-8 helper. Multi-fd acquisition checks array exceptions and releases buffers; multi-path count/allocation are selected source reads only, not a complete JNI acquisition or budget family qualification. Do not add compatibility APIs or duplicate CUE grammar into Kotlin.

## Future acceptance and simplification

Repair existing0193/0194 in their production owner: prove descriptor balance for invalid-size/open failures at first/later positions, complete malformed every-byte JSON corpus with sanitizers and Unicode/numeric/identity controls, and no source opens or state publication on rejection. Preserve ordinary valid one/many-source controls, capacities and hard-rejection semantics.

Keep one immutable track identity and owned input generation through parse, fingerprint, preview and publication where the existing ownership repair requires it. Retain distinctions among physical track, audio ordinal and display number. Progress means work position; terminal typed results separately carry accepted source, matched/unmatched/error counts and publication outcome. Reuse the actual shared span/CUE authority and narrow paired engine hooks rather than a second parser or generic framework.

Actual device/fault/concurrency/provider/media execution is deferred. Remaining0433 owner/prior-plan synthesis must reconcile source-selection readiness, prior quality scopes and lifetime owners before direct disposition/publication. No new root, status/rating mutation, inherited saving or runtime success.
