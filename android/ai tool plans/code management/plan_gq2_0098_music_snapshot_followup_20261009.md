# Music snapshot and acknowledged source transition follow-up, 2026-10-09

Diagnosis and future implementation plan only. Do not implement during the current tranche. Chunk0098 diagnosis is complete; all implementation and acceptance below remain future work

## Evidence and ownership

- [x] Recover current snapshot, song reload, JSON writer and source-panel publication paths
- [x] Complete canonical ownership reconciliation; admit distinct GQF-0273/GQR-0259 for same-count native inventory invalidation
- [x] Carry the accepted disposition into the immutable0098 report and canonical observations

GQF-0233/GQR-0218 owns staging and allocation-safe snapshot publication. Preserve the previous complete snapshot or an explicit safe failure projection; never dereference a failed initial track-list/fallback allocation. Completed codec repairs are separate from producer-side scalar truncation

BR-0432 already requires source preference, playlist, UI and native source to update as one acknowledged transition. Current MusicControlPanel.setSource rejects empty custom/CD preparation, but changes state and music_mode before nativeSetMusicSource returns. A rejected bounded enqueue therefore needs explicit rollback or publication only after acceptance; accepted enqueue still does not establish durable write or successful playback. Coordinate command acknowledgement with BR-0244 without claiming the music mutex queue has the old volatile-mailbox race

BR-0503 concerns consumer scroll/focus reconciliation when the list changes. It does not establish producer cache invalidation. Current music_publish_snapshot caches tracks under type*1000+total, with no prefer-mission or inventory identity in the key. The source command calls songs_uninit then songs_play_level_song or songs_play_song; paired song initialization reloads BIMSongs and mission names. songs_get_track_list obtains names from that current inventory, but is called only when the cache key changes or no cached list exists. The equal-type/equal-count case is distinct GQF-0273/GQR-0259 after canonical reconciliation; it has source evidence, not a runtime reproduction or a closed scroll repair

## Future implementation and acceptance

- [ ] Stage one internally consistent snapshot with checked allocation/capacity and explicit publication failure. Keep command completion and snapshot generation coherent on the engine thread; release locks and temporary resources on every path
- [ ] Choose the smallest correct invalidation contract: an engine-owned inventory generation advanced at actual replacement boundaries, or uncached regeneration if measured bounded cost supports it. Keep game-format knowledge in the engine; do not add a Kotlin inventory mirror or historical format migration
- [ ] Publish the source choice and preference only under the documented accepted/applied contract. On queue rejection preserve the previous UI/native/preference state and expose actionable failure. Define playlist preparation ownership and cleanup so rejection cannot discard the prior usable generation
- [ ] Validate equal-type/equal-count replacements with different names/order, base-to-mission preference switches, CD audio membership changes, custom reloads and failed refresh retaining prior state. Require current list/current track/source to refer to the same generation; separately exercise BR-0503 selection/scroll reconciliation
- [ ] Validate full queue rejection, accepted-but-pending commands, failed engine application/persistence, lifecycle recreation and retry in both games. Require truthful rejection, no optimistic persisted mismatch and no interpretation of pending=false as durable success
- [ ] Extend GQR-0218 production allocator acceptance across first/replacement/fallback/overlay growth failures. Extend producer encoding acceptance with admitted multibyte names at fixed-buffer limits before strict JNI decoding; structural JSON escaping alone does not repair split UTF-8 scalars

All acceptance above is future work. Existing authored source-availability and transport fixtures are useful inputs but do not prove same-count invalidation, queue saturation, durable command completion or allocation/scalar failure behavior. GQF-0273/GQR-0259 is admitted as diagnosis only; no implementation closure or runtime acceptance is admitted by this plan
