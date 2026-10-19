# Retroid Pocket 4 Pro full single-device validation

- [x] Read repository instructions and identify attached RP4Pro JYPR42510121028
- [x] Run full single-device catalog with extended graphics, preserving initial failures
- [x] Diagnose failures using durable automation evidence and targeted reruns
- [x] Repair bit-rotted scripts and verify relevant catalog/quality checks
- [x] Summarize passes, confirmed defects, exclusions, and residual uncertainty

Use isolated com.dxxredux.app.nsdtest. Preserve existing working-tree changes and do not edit outstanding_bugs.md. Evidence: temp/retroid_full_20261018. Physical profile excludes host, extraction, server, dual-device and emulator-console-only tests; record these exclusions explicitly

## Initial findings

- Sandbox build failed before tests (Gradle cache/network access); normal-host retry built successfully
- Suite command: pwsh -NoProfile -File android/run_all_tests.ps1 -Serial JYPR42510121028 -SingleDevice -FullSuite -ExtendedGraphics -ReportDir temp/retroid_full_20261018
- Frozen suite APK SHA256: BC374874324D29DE6ABAF3A0F0A3B0A03475BAE1903DD63FBE7E35AC55DA25AA
- Original mod-bitmaps failure: ADB host daemon unavailable during D2DESCENT.PIG push, before assertions; requires rerun
- D2 mod renderer reproduces UUD-vs-KTX2 priority image hash mismatch; preserved in renderer_failure/
- D2 mod sounds reproduces HAM patch op 580 robot33 AnimState0_0Offset expected685 actual0; preserved in sound_failure/
- Graphics recovery helper rejected all physical devices. Adapted shared selection, isolated-package guard and package-scoped broadcasts; assertions unchanged
- Target selection and automation catalog pass. Suite catalog timed out sandboxed, passes with normal host access. Scoped quality pass before final broadcast-scoping addition; rerun after edits complete
- D1 pause checks passed; D2 prerequisite and standalone save/load both failed at route certificate readiness (step57/87), cold worker timeout and route_readiness=calculating. Added automap-priority readiness before save; requires serial reruns of both owners
- Audio mix wrapper failed before launch because temp/music-spectral-venv was absent. Added pinned environment provisioning and shared target/isolation checks; dependencies installed successfully, device measurement pending
- Retroid live rebind PASSED (12 launcher steps +13 native steps, same game PID), unlike earlier emulator validation
- Controller touch hotplug rejected physical serial immediately; requires uinput and no preexisting controller. Added physical-profile exclusion rather than pretending built-in controls can be detached
- Co-op session instrumentation failed migrated-host adoption check. Original APK built before concurrent CoopSessionChecks.kt update adding required built-in sizeBytes/SHA256. Existing user update already corrects missing fixture identity fields; do not overwrite it. Preserve installed instrumentation APK and rerun current instrumentation after main suite
- Counterstrike level21 pre-reactor route: metadata status failed before route assertions. Preserve level21_metadata_failure.json; needs focused cold retry with logcat
- DXA asset usage completed all12 gameplay scenarios but verifier lacked SFX records. Device global log.tag=I suppresses DXX-DLOG debug. Added per-tag D override with finally restoration in DXA owner; original12 captures in dxa_failure/, needs rerun
- FOV classic demo roundtrip: D1 rejects its own fresh .dem as corrupt, error menu instead of gameplay at step66/82. Saved raw .dem, input demo and RNG companion in fov_failure/. Helper now exports companions/logcat before propagating failure
- Full first-run graphics matrix PASS in17m27s, both initiating engines and cross-engine handled-state checks, including accept/cancel/back/timeout/previous/unchanged/background/stall/unsupported/crash/rearm
- Latest scoped quality, automation catalog and suite catalog checks PASS
- Headed/headless parity failed at hardcoded run-as com.dxxredux.app introspection/staging, while headed game used nsdtest. Replaced three package literals with selected diagnostic package; requires parity rerun. Original summary/log in parity_failure/
- LAN orphan restart waits on suppressed DXX-DLOG; added scoped log session, needs rerun
- LAN QR reports active lobby lost but initial hostLobby had no precondition assertion and frozen app default built-in requirement omitted required identity. Current user source already updates LobbyService default identity. Added explicit hosted-lobby precondition to instrumentation; rerun after current build with co-op fixture
- Graphics preview: no previewable levels follows6064ms metadata timeout and request.json deleted before worker read; robot preview passed. Original evidence preview_failure/
- Graphics recovery first stall reached rollback but debug evidence suppressed by global INFO. Shared checked log-session helper added and verified on actual Retroid (D then blank restored). Host checks cover blank/I/D and selected-device restoration
- Generated media-controls fixture failed because ffmpeg/ffprobe were not on PATH. Added explicit args/environment tool resolution to generate_preview_test_audio.py; existing local FFmpeg8.1.2 tools generated and validated the exact synthetic30s MP3 successfully. Device retry must set FFMPEG/FFPROBE
- Uneasy4 result-cache reuse failed at runner600s before first analysis finished. Saved uneasy_cache_progress_logcat.txt and uneasy_cache_failure_logcat.txt: active request CPU-bound for8m+, background next requests time out at120s while queued. Cache-hit assertion never reached
- Lobby latency fixture never entered injected slow-save validation, same initial host identity prerequisite suspected. Added initial-host precondition; current app/instrumentation rerun required
- Preserved FOV .dem passes current host validator (74frames, version13) using buildd1 exe SHA2562525DF7B4F2CC8C13CBC1F0D008005FF13859A00E02C8AD00422DBF1F55D1202; current Android must still be checked
- Extraction audit: child test_extract.ps1 hardcodes com.dxxredux.app and emulator startup checks, so retain physical-profile exclusion. Host physical-route corpus is host-only and excluded despite FullSuite flag
- Mission asset isolation completed95 automation steps but final activation-owner trace was empty because global INFO suppressed DXX-DLOG. Added scoped debug-log session, assertions retained. Original mission_isolation_failure_logcat.txt preserved
- Mod fuzz smoke completed14 cases:11 pass,3 assertion failures (all-on imported D1 highres pack not rendered; all-on D2 and Vertigo UUD metadata not applied). All7 baseline modes passed. Preserved exact3 failing cases, inventory, journal and summary in mod_fuzz_failure/
- Obsidian6 route prerequisite failed status=failed before switch/grate assertions. Added explicit automap-priority readiness to it and Counterstrike21; semantic assertions unchanged, original failures retained
- Obsidian7 completes metadata and initial marker/door assertions, then after warp-to-me trigger changes7->8 while endpoint remains321. Saved live snapshot and guidebot JSON; retain assertion and investigate focused retry
- Package documents fails immediate post-Compose-state absence check. Replaced immediate check with existing bounded wait for same absence condition plus visible fixture dialog; instrumented retry required
- Host timing/suspension gap during random-level preview: clock.sleep requested55s returned elapsed1180s, suite random-preview owner reports50m29s and launcher-readiness timeout. Treat result as infrastructure-affected, rerun with stable transport; no gameplay assertion reached. Quick-record classic sidecar passed
- Slider instrumentation fails production GraphicsChooserLayoutChecks:640x360 atfont2.0 leaves <48px scrolling area. Preserve assertion and rendered chooser PNGs in chooser_layout_failure/; current-build retry required
- Sound trace gameplay passes23/23 but INFO global log threshold yields empty filtered output and no Set-Content file. Added checked scoped logging, explicit empty capture file, shared selected-target initialization. Original gameplay/trace failure stays in main log
- UUD author acceptance passed all7 levels and rendered-reference checks
- UUD combinations captured15 states, then stopped verifier at D1 uud-first missing mixer trace (global INFO), before later D2 priority assertions. Added shared checked log session; original15 state captures preserved in uud_combinations_failure/. Requires full owner rerun to validate repaired trace gate and expose subsequent conditions
- UUD2SP original stops at sound-only mixer-consumption trace gate (suppressed debug) before metadata phases. Added checked scoped log session, preserve current sound-only captures in uud2sp_failure/. Do not conflate this original failure with the independently reproduced native HAM rejection until repaired rerun reaches it
- Main run complete173 executed:141PASS,32FAIL,191SKIP,0NOT_RUN; elapsed5h23m includes host timing gap. Original report_20261018_222300.md retained
- Final Vertigo merge creation completed51/51 automation steps but lacked debug capture event=begin; added shared log session. Original vertigo_merge_failure_logcat.txt preserved
- Focused retry selection includes all32 original failures; updated physical profile will explicitly skip3 emulator-only admissions. Current app + instrumentation build will distinguish concurrent source updates from harness repairs and retain original report

## Current-build rerun progress

- Current diagnostic APK SHA2562FDA1FA78F445EE5CD8A3C4C249FCCA0BE8394D65273AA3C51EA1D70525D7CC6; original installed APK matched frozen main hash at end
- Bitmap native test PASS; prior failure was upload infrastructure
- D2 renderer and sound-mod failures reproduce on current native libraries
- D1/D2 pause and standalone save/load PASS with readiness preparation; original route assertions preserved
- Audio mix full live capture PASS in8m48s after dependency setup repair
- Co-op session PASS with current preexisting user identity fixes/fixture update
- Counterstrike21 route PASS after explicit automap preparation
- DXA all12 phases + exact PCM/mixer/off-on-off verification PASS with scoped logging; positive captures in retry_current/dxa_pass/
- Graphics preview current-build rerun PASS for both games; original cold metadata failure remains preserved
- Current D1 FOV recording again rejected on Android, while current host validator accepts exactly those bytes (73 frames, version13, 57764 bytes); validation and actual playback are different paths
- Graphics recovery rerun passes first5 D1 fault cases, then fails accept_publication_blocked because surfaced launcher recovery message is absent (displayed_launch_error=null); failed OK publication result=-1 is logged and accepted tuple retained. Remaining recovery matrix needs individual continuation after retry suite
- Headed/headless parity PASS with isolated-package repair; LAN QR and lobby latency PASS on current app/instrumentation; generated media controls PASS with explicit FFmpeg tools
- Uneasy4 current rerun again fails at600s child deadline before first analysis/cache assertion; worker request logs show active duty10% and background queue timeouts120s. Preserve as analysis-throughput/prerequisite failure, no cache correctness conclusion
- Mission isolation current rerun completes95/95, but small Retroid log ring drops first base activation and original marker. Later6 transitions correct; stream required tags to host from before launch, preserving original seven-transition and sample-hash assertions. Serial retry queued in continuation
- LAN orphan current rerun still misses initial native auto_join marker. Added failure log/launcher evidence capture; focused continuation queued before classifying startup cause
- Obsidian6 PASS after explicit metadata preparation
- Obsidian7 reproduces late guidance change after warp-to-me: current trigger8 and endpoint379 (expected7/321), original trigger8 with endpoint321. Metadata/initial switch/door assertions pass first; current snapshot in retry_current/obsidian7_failure_snapshot.json
- Package documents now reaches recommended-mod phase; first View docs is below landscape viewport. Added accessibility scrolling while preserving requested-mod identity and all document assertions. Instrumentation-only rebuild successful; install/retry after current suite ends
- Current random preview seed1999534517 selects -MOON-.zip;180s expires while launcher analyzes its multiple mission definitions. Added exact-seed600s diagnostic continuation before deciding whether the physical caller budget needs repair. Current failure evidence in retry_current/random_preview_failure/
- Sound trace PASS23/23 plus effects-gain/mixer verifier with scoped logging
- Slider owner again fails production chooser layout at640x360,font2.0 before controller navigation; preserve as layout failure, do not claim later slider assertions ran
- Continuation results: documentation PASS; D2 FOV PASS82/82; streamed mission isolation PASS95/95 plus exact7 owner/sample checks; warm exact Moon seed PASS109s
- All12 remaining graphics recovery cases PASS, including all9 D2 faults. D1 blocked-acceptance message fails a second focused attempt; other17 distinct cases PASS. Preserve recovery_d1_accept_retry/ and first failed case separately
- LAN port collision precisely identified: ss UID10155 owns127.0.0.1:42430; process is background com.dxxredux.app.github, no :game process. Added actionable occupied-port precondition and socket capture. Plan adjusted to temporarily stop this verified background launcher for final LAN variants and reopen it afterward, without clearing data
- Random preview now separates metadata preparation (physical600s/emulator180s) from unchanged180s live-render wait; master900s accommodates both. New default-path cold Moon attempt runs only after pause/clear route and result caches are confirmed in cold_preview_setup.log
- Final23-path scoped mixed-language checks, automation catalog and suite catalog PASS; diff whitespace check PASS

## Final outcome

Completed170 portable tests: latest160PASS,10FAIL,194 catalog exclusions. Original173-attempt report141PASS/32FAIL remains intact;3 original failed admissions are emulator-only.19 initial failures now pass across documented reruns/builds

All18 recovery cases attempted:17 pass; D1 blocked-acceptance message fails twice. D2 FOV passes82/82. Both LAN orphan variants pass after temporarily freeing the GitHub launcher's occupied UDP port and reopening it. Documentation and full streamed isolation pass

Final cold Moon fixture explicitly clears both caches, uses the restored power lease and new separate preparation deadline:238s preparation,2s live introspection, all rendering/interaction/cleanup assertions pass. Original failed no-lease cold attempt is preserved separately

Report: retroid_full_validation_20261018_results/REVIEW.txt. Durable copied reports, final dispositions, source hashes, continuation outcomes and final device state sit beside it. Full logs/APKs remain in temp/retroid_full_20261018. Final application APK matches frozen2FDA...; power0 and DXX-DLOG blank restored, GitHub launcher running, USB battery100%/25C. No outstanding_bugs.md edits or engine changes by this task
