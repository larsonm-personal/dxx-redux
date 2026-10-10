# GQ2 chunk0306: multiplayer and music, 2026-10-10

Diagnosis and plans only; preserve concurrent product edits

- [x] Read assigned U0 diffs including removals across ten paths, whole97line UdpReconnectStore and whole47line MusicEq
- [x] Read all complete current deltas and bind frozen/current/original/manifest identities, including deleted TvButtons
- [x] Trace current production/native consumers and reconcile existing owners and minimization
- [x] Final report, canonical publication, independent audit and handoff

## Assigned source observations and boundaries

MultiplayerCallsigns/ResumeOffer/Screen adopt shared app buttons and NavigationAlertDialog; TvButtons is deleted115line duplication, all removed base lines read. Keep consolidation subject to actual shared-wrapper callers and focus context. Current deleted path remains absent; queue new1-1 is a deletion placeholder, not a surviving source line

ForegroundService assigned20hunks add separate game/LAN lease booleans and nonreference wake lock released onDestroy; currentdelta delegates shared notifications and stops MultiplayerGameService on forcedshutdown/destruction. Whole328current read. Background deadline expiry schedules bare delayed forceBackgroundShutdown; owner/generation reconciliation remains pending, no new defect admission. Separate game service54 and notification44 read: protect distinct processes, do not deduplicate services based on shared notification

ResumePrefs assigned10hunks persist coopBriefings/allowSecretWarps, use hostedLevel and quick-resume latest timestamp independent prior lobby selection. Whole398current read, currentlineequalfrozen. Secret-area fullsave uses ordinary hostedLevel1 before restore; screen981-1011/1203-1234 read. Native restore/body validation and actual save consumers stillpending

RuntimeGameStateBridge assigned3hunks bind applicationContext and update host registration/scheduling. Current227-290 read; generation/death/rebind/stateproducer reconciliation pending

UdpReconnectIdentity assigned7hunks replace process-only key generation with installation store and durable request counter. Whole103current/whole97store read; source lock plus filechannel lock serialize state, bounded key sizes and failed parse failclosed, pending file sync precedes replace. MainActivity1120-1143 initializes noBackupFilesDir before startup; native95186 obtains public key/counter and returnszero on JNI failure. Native request/host migration/protocol and prior fixed owners pending; no new runtime/crypto/storage acceptance

MusicControlPanel assigned22hunks remove local refresh retry and move refresh to caller, add visible-track polling predicate, queued-return handling, active-set readability admission, cached volumeRect and immediate optimistic volume. Currentdelta repairs integer native paused/oneTrackPerLevel JSON parsing, adds pause injection, spatial focus/close control and dropdown tap ownership. Complete delta read; current whole-body/nativepolling/command application consumers pending. Preserve current repairs rather than describe frozen bool parsing as current

MusicEq whole47frozen/current lineequal: presets flat/detail/balanced/broad map to indices0-3, bundled defaultbalanced and otherprofileflat, onlybundled/exactsoundfont supports measured presets. Native/header/asset/profile persistence consumer binding pending; no audio response or preset regeneration

## Checkpoint

Twelve assigned scopes/all complete currentdeltas and12context ranges saved; source completeness is partial pending consumers/owners. ScopeSHA 8b1dda944845769be64e3a57619cad5acb446038d0a83e3a034a1ad8c367ff84; productdiff unchanged and liveHEAD reverified. Canonical305DONE322TODO/627;0306TODO. No new IDs/status/rating/runtimecredit/inheritedsaving, code/test/helper edits or execution


## Chunk0306 music/native and delayed-service owner supplement

Saved20freshcontext ranges, scopeSHA 2c91c0a0bb9a785e995c8d41f52f9c4f568a817cdad24e4ba47b42d3ef6012eb. Native snapshot integer paused/oneTrackPerLevel matches current optInt repair; setters report queue admission. MainActivity owns120ms pending refresh and track polling rather than deleted localpanel timer. Native synthheader0flat/1detail/2balanced/3broad agrees orderedMusicEq presets, bounded generatedprofiles/fonts read without regenerated/measuredaudio acceptance. GQF0224/GQR0209 fullcurrentrecord remainsOPEN/TODO for unbound delayed service shutdown; no duplicate root. Assigned current/context/product identities rechecked unchanged. Remainingwholemusiccontrol/nativeapply/store/protocol/resume/UI consumers and ownerreconciliation, finalreport/publication/audit pending;0306TODO305DONE322TODO/627. No code/test/helperchanges or execution;goalactive


## Chunk0306 music application, reconnect and resume ownership checkpoint

Saved39freshcontext ranges, scopeSHA 1abaf25034b2f5c62909fe18845b3e8e0f145cca0798e96250912933f5882313. Native queue applies on engine thread and publishes snapshot before clearing applying; MainActivity bounded refresh and destroy cancellation traced. Runtime host publisher/register/disconnect source preserved; archivedBR0004FIXED read with its historical onlineacceptance limit. Native reconnect obtains positive durable counter before signing and rejects stale identity counters; archivedBR0225FIXED read, process-lifetime wording is historical and current installation store repair retained. FullBR0450/0502/0506 remainOPEN: roster lifetime, online resume endpoint/list generation and complete launch-generation acceptance respectively

Bounded current MusicControlPanel spatial focus/paused-volume queue handling and source readability traced; optimistic source/prefs-before-admission saturation outcome remains candidate for existing owner reconciliation, not newly admitted failure. SoundfontStore profile/default and nativeSF/FM EQ projections traced without generated or measuredaudio acceptance. Remaining shared UI/callsign, actual pairednative/game/mission/restore and source/panel consumers and ownerreconciliation, finalreport/publication/audit pending.0306TODO305DONE322TODO/627; allprior assigned/context/HEAD/product identities unchanged. Diagnosis/docs only, no code/test/helper edits or execution;goalactive


## Chunk0306 source diagnosis complete; publication pending

All12assigned scopes/current deltas and 65bound contexts reconciled. Shared button consolidation, paired engine music/reconnect routes and native restore-choice consumers retained. GQR0218REFERENCE56/RETAIN/NO_INHERITED_EFFECT0; existing allocation/cache/service/source/scroll/resume owners retained and current readability/integer parsing/durable identity/IPC repairs qualified. No new roots/status/rating/runtime acceptance or inherited saving. ReportSHA 054e1f2dafd914b58aa5b15463d54ad7f6f9795449d0f5a2266209c44530f716; scope 37d360ad47588e0b91a605e2cdbd550e9dcef1916c58b9df264c31096a348912; snapshotSHA b112789ddfc9889e1dc7c8733cd0f99a71aecb5558ee7884cafc66515c4610da. Canonical publication and independent audit pending;0306TODO305DONE322TODO/627. Productdiff/liveHEAD/source/context identities revalidated unchanged; diagnosis/docs only, goal active


## Chunk0306 published and independently audited

GQ2-CHUNK-0306DONE/GQC1134ISSUES/GQD1014NO_INHERITED_EFFECT; RETAIN/GQR0218REFERENCE56. All12assigned scopes/current deltas and65contexts reconciled; existing music allocation/cache, delayed service/source/scroll/resume owners retained, current shared UI/readability/integer parsing/IPC/durable reconnect repairs preserved. No newroot/fix/investigation/rerating/status/closure/runtimeacceptance/inheritedsaving. Independent publication audit PASS379 SHA db03291c1ae13e0260870ed637eb4c8472f9e6446d3dbc0bac82c1694e51a71c; exactfivecanonicalbytes/prior terminal suffixes and independently sortedrank/fixandowners preserved; source/frozen/current/original/manifest/context/prioraudi/product identities checked. Canonical306DONE321TODO/627;1121terminal267fix11investigations. ReportSHA 054e1f2dafd914b58aa5b15463d54ad7f6f9795449d0f5a2266209c44530f716; scope 37d360ad47588e0b91a605e2cdbd550e9dcef1916c58b9df264c31096a348912; snapshotSHA b112789ddfc9889e1dc7c8733cd0f99a71aecb5558ee7884cafc66515c4610da. Next0307 SetupAutomationApi/SetupConfigFiles. Diagnosis/docs only, goalactive; remainingchunks/current-head reconciliation/supplements/investigations/sweeps/historical0116audit debt/finalclosure open
