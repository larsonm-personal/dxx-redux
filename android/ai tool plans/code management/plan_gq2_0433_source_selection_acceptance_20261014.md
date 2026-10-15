# Chunk0433 source selection and prior-evidence reconciliation, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. No implementation or runtime acceptance.

## Actual readiness and source transition

MusicOverlaySourceOption now checks usable custom tracks, filters CD sources by local availability and content-URI descriptor probes, applies shared capacity admission to the available subset and retains an already active CD option. MusicControlPanel supplies real canAccessSafUri rather than the permissive default callback. Preserve these repairs. A filtered available subset is not proof that every enabled row was admitted, and a successful probe is not a later native-open or playback acknowledgement.

Actual CD selection checks writePlaylist false and catches ordinary exceptions, preserving displayed state/preferences on that preparation failure. Files mode similarly rejects a null M3U result. After preparation, however, setSource changes its displayed source and SharedPreferences before nativeSetMusicSource. Native converts the selected label and enqueues a bounded64 command under a mutex; success means enqueued, not native source opened or playback delivered. Queue failure has no UI rollback here. Engine consumption changes GameCfg, saves configuration and replays current music, then publishes a snapshot; the selected path returns no per-request terminal admission outcome. Retain the repaired queue and snapshot instead of repeating direct UI-thread music mutation as current behavior. Existing BR-0432 and launch/resource ownership retain the complete accepted transition requirement; archived0326 remains fixed for its actual capacity/parser controls.

Complete this through the existing source-transition owner: prepare one owned playlist/descriptor candidate, obtain an engine result tied to request/source generation, then commit displayed/preference state and retire old resources. Preserve the prior source and usable generation after queue, preparation, native admission or output failure. Avoid a second source-policy table or parser.

## Progress remains presentation

Advanced Settings page open still has three off-main scan steps and clamps progress. The music formatter renders discovering, empty, metadata-wait and finished-with-failure wording from RouteMetadataPrecomputeSnapshot. The monitor copies those fields under FILE_LOCK; this confirms producer-to-renderer field handoff, not complete worker lifecycle, cancellation or durable accepted metadata. Preserve current wording and partial producer repairs; do not turn a displayed fraction or total into a native completion assertion.

## Existing configuration and metadata owners

Current ConfigImportLoader retains strict UTF-8 and1MiB IO admission, worker dispatch, string/container/value/slot limits and ordinary typed errors. Its lexical nesting scanner understands only double-quoted strings, decrements depth without nonnegative/balanced admission and runs before lenient JSONObject. Postparse tree walk has no depth counter. Exact prior GQF-0225/GQR-0210 owns accepted-grammar and pre-materialization nesting admission. The historical166-byte reproducer was not rerun here; preserve current IO/rollback and repair grammar in the actual owner. Do not introduce compatibility readers.

GQF-0227/GQR-0212 remains the distinct attempt-wide live-memory/work/deadline policy for BinHex and metadata consumers, including AcoustID response materialization. Whole current canonical rows were read with all extensions retained, but this does not freshly qualify every unrelated extended consumer. Existing0193/0194 native handle/JSON roots remain separate. Ratings and statuses are not changed at this checkpoint.

## Historical reconciliation

Read whole imported0261/0262/0489/0649/0650/0654/0660 reports and original R1-0528 completion. They independently record the same native parser/handle, audio resource, grammar, metadata, gesture and ownership boundaries. Retain current atomic registry and manifest, plural URI schema, ordinary rollback, bounded CUE, native capacities, external artifact and shared-grant repairs. Do not resurrect singular URI readers, old exponent APIs, zero-CRC fixtures, filename-only external ownership or old direct-final registry publication.

Archived0505 confirms sparse names retain physical CUE track identity separately from audio ordinal and displayed track number. Current preview helpers, plural source persistence and native ordinal selection preserve that repair.

Older reports use earlier frozen heads and excluded live deltas. Their reported tests/builds are historical only. The current chunk separately binds its frozen assignment, removals, present source and later BinHex test delta. Earliest report ancestry and complete final HEAD/worktree supplement remain broader campaign work; selected reports are not a closure claim.

## Remaining acceptance

Finish whole-owner and prior-plan/quality synthesis, direct unpooled disposition and exact publication/replay/Drive. Future source tests must drive real queue failure, stale requests, unavailable later opens, native rejection, lifecycle retirement and playback proof. Use strict grammar and small injectable limits through real import routes. No new finding, root reopening, status/rating mutation, inherited saving or runtime success.
