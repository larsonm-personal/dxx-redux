# Co-op briefings and synchronized mine launch

Status: implementation handed off for player testing (2026-09-14)

The user requested wrapping up after the final cross-world save/load test,
which passed in `temp/coop-cross-restore-live.log`. Further edge-case expansion
and the historical remaining acceptance cases below are deferred until player
feedback or a new request. Keep each player's N/M informational and the end/Skip
signal independent; content agreement remains intentionally out of scope.
Player testing should cover reading/skipping pages and videos, the visible
120-second maximum and 20-second host-ready limit, and the separate Launch now
button pulling everyone into the mine together. Ordinary observers, larger
rosters, the remaining reconnect/closure combinations, and overlay recreation/
multi-touch coverage are follow-up validation, not blockers for this handoff

Presentation networking/cancellation audit (2026-09-14): both `titles.c` event
loops call `coop_briefing_pump` before processing events and close their window
when cancellation is requested. D2's movie loop does the same even with its
pause window open, and closes that pause window on both network cancellation
and local Skip. Later page/movie entry points consult the same cancellation
flag, so closing one presentation cannot start the next part of the sequence.
The pump services protocol traffic, deadlines, host launch requests and peer
loss. `coop_briefing_run` records explicit completion after the sequence unwinds
and keeps the mine hidden and time stopped until the final barrier releases.
The inspected paired force-launch scripts assert that the client was still
presenting before launch and that both peers finish settled and unpaused.
Their current D1 run passed in `temp/coop-briefing-simple-d1-live.log`; the D2
run and cold resume passed in `temp/coop-briefing-simple-fresh-resume-d2-live.log`.
The retained `temp/coop-paused-video-force-d2-live.log` and
`temp/coop-paused-host-range-live.log` also passed. Their native scripts check
paused movie closure, absence of the modal pause window and synchronized play.
This closes the implementation audit, not the separate lifecycle cases below

User simplification (2026-09-14): page counts are informational and local to
each player. Keep the explicit end/Skip signal separate from N/M. Different
text or extra pages must not reject progress, skip content, or prevent launch.
Content hashes, shared denominators, mismatch warnings and their dedicated
fixtures have been removed. Historical content-agreement results below describe
superseded behavior and are not acceptance requirements. The 120/20-second
deadline, host Launch now control and final mine-ready barrier still apply.
The packet is now 128 bytes; Android protocol versions are D1 30064 / D2 30067.
Scoped formatting, Android assembly (2m 2s), and both Windows builds passed in
`temp/coop-briefing-simple-{format,build,windows}.log`. Both freshly rebuilt
transition-policy tests passed, including different per-player totals, reaching
N=M while still reading, and an explicit end signal with N<M. D1 paired
Skip/Launch now passed at 09:11:22 in `temp/coop-briefing-simple-d1-live.log`
with both native captures. D2 paired Skip/Launch now also passed and saving
completed at 09:13:59 in `temp/coop-briefing-simple-d2-live.log`. The additional
cold-resume check stopped at launcher startup at 09:15:21 when the host emulator
stopped answering ADB. The D2 run therefore exited 1; it does not verify resume.
Client native logs were captured, but the host logcat request also stopped
responding and was terminated, leaving that capture empty. A later host capture
was recovered in `temp/coop-briefing-simple-d2-host-recovered-logcat.log`.
The next run (`temp/coop-briefing-simple-resume-d2-live.log`) reached restore but
failed when the client proxy exited on ENETUNREACH at 09:27:13; the host's
bounded loader wait then expired. Both native captures are retained. After
restarting both test emulators, the unchanged APK passed the full D2 briefing,
save and cold-resume scenario at 09:32:55 in
`temp/coop-briefing-simple-fresh-resume-d2-live.log`, with both native captures.
The runner verified restored inventory, matching campaign metadata, zero
briefing presentations on resume and two peers in the settled mine. This
closes the interrupted simplification/resume check without changing timeouts

First-time join coverage passed with `-BriefingCase first_join`. It
starts a solo host briefing before launching the client, then checks deferral
and admission after the original host presentation finishes. The D1 baseline
failed at 08:49:28 in `temp/coop-briefing-first-join-baseline-d1-live.log`:
the host requested F6 approval at 08:48:52 while reading and rejected the client
at 08:49:00. Both native captures are retained. The ordinary approval path
preceded the existing transition guard in `net_udp_welcome_player`. D1 and D2
now share a local deferral check between admission and approval, so reading or
loading cannot spend the approval interval. Scoped formatting, Android assembly
(1m 3s), and both Windows builds passed in
`temp/coop-briefing-first-join-fix-{format,build,windows}.log`.
The first patched D1 run ended at 08:54:27 because client startup stopped
before auto-join; `temp/coop-briefing-first-join-ready-d1-live.log` and both
native captures retain that unclassified startup stall. A fresh D1 run passed
at 08:56:22 in `temp/coop-briefing-first-join-retry-d1-live.log`; D2 passed at
08:58:10 in `temp/coop-briefing-first-join-ready-d2-live.log`, both with native
captures. The logs show unauthenticated/new player deferral (`player=-1`) while
the host is reading, then the first F6 prompt only after the host's settled,
unpaused assertion passes. Approval admits the new client with two connected
players, zero client presentations, and the host's original generation.
These results cover first-time joins during reading; joins during closure or
loading still need separate runtime coverage

Briefing-time reconnect coverage is being added through `-BriefingCase rejoin`.
The runner stops the reading client, waits for its removal, restarts it while
the host is still reading and requires a new native join-deferral log without
changing the original generation, roster or countdown. The host then finishes;
the returning client must enter the settled mine with zero presentations and
the host must retain its original generation. The source audit found that join
uses `StartNewLevel`, which arms briefings before level synchronization resets
`Network_rejoined`. The initial run failed before reaching this check because
the fixture watched DXX:I rather than DXX-DLOG:D and did not enable the co-op
diagnostic category. With the fixture corrected, D1 reproduced the actual
failure at 08:37:00 in `temp/coop-briefing-rejoin-trace-d1-live.log`: the host
deferred the authenticated returning player without changing its briefing,
then admitted it after launch. The client completed level synchronization at
08:35:55 but waited for a new briefing PREPARE and left after the peer timeout.
Both native captures are retained beside that log. D1 and D2 now disarm the
pending briefing before clearing `Network_rejoined`; fresh level starts keep
their briefing barrier. Android assembly passed in 2m 8s, Windows builds passed
for both games, and scoped formatting passed in
`temp/coop-briefing-rejoin-fix-{build,windows,format}.log`. Paired reconnect
validation passed in D1 at 08:43:33 and D2 at 08:45:42 in
`temp/coop-briefing-rejoin-ready-{d1,d2}-live.log`, with host/client native
captures. Both runs first verified normal fresh-session presentation, then
authenticated rejoin deferral with the host's generation/roster unchanged and
its timer decreasing, followed by admission to the unpaused two-player mine
with zero returning-client presentations. This covers a disconnected reader
returning after participant removal; reconnection before removal and ordinary
observers remain separate acceptance cases. First-time reading-phase joins
passed separately above

Observing-host coverage passed through `-BriefingCase observer_host`
in D1 and D2. The runner uses the existing native host-observer option without
running the unrelated Guide-Bot fixture. Both peers must report the observing
host, retain its deadline/launch authority, shorten the deadline after host
Skip, show the waiting notice to the still-reading player and launch only on
the explicit host action. Generic Enter must leave the host in the wait state.
Ordinary non-host observers are a separate case and are not covered by this
fixture. D1 passed at 08:17:33 and D2 at 08:18:42 in
`temp/coop-briefing-observer-host-{d1,d2}-live.log`, with both native captures.
The host retained actual observer mode after launch (game mode 1052 in D1,
4124 in D2), while the playing client remained in mode 28. Both native scripts
passed the role, readiness, deadline, generic-Enter and explicit launch checks.
Scoped formatting passed in `temp/coop-briefing-observer-host-format.log`;
no native rebuild was needed

Ordinary-observer source audit: `local_observer()` excludes non-host observers
from readiness acknowledgements. The host builds the participant mask from
playing slots, and UDP forwarding sends briefing control to observers without
their optional gameplay delay. An observer can accept a later-phase initial
snapshot and therefore need not replay an already finished presentation. These
are implementation findings; ordinary-observer participation and late-join
behavior still need live acceptance coverage

Current-build option-combination coverage passed after content
agreement and final-release loss validation. The D2 secret-advance scenario
now explicitly checks both enabled switches, a completed matching content plan,
and the client's host-waiting notice in level 9 after leaving a secret whose
base was destroyed. The physical entry/return scenario now verifies that both
server settings survive each transfer and that no presentation opens when
briefings are disabled. Both scenarios use co-op QoL disabled. Combined travel
and briefing advancement passed at 08:10:07 in `temp/coop-options-advance-live.log`:
the secret exit won the competing-exit race without a post-level split, both
players entered -2, then advanced to level 9 and completed host Skip/Launch now
with matching content plans and the expected campaign/inventory state. Physical
entry and return with briefings off passed at 08:12:15 in
`temp/coop-options-secret_only-live.log`; both peers kept the requested settings
and zero presentations after each leg, with matching campaign/checkpoint/arrival
checksums. Both runs retain host/client native captures. Scoped formatting passed
in `temp/coop-options-combined-format.log`; no native rebuild was needed

Final-release participant-loss coverage is available through
`-BriefingFailure host|client -BriefingFailureRelease`. The local diagnostic
release drop can hold the client for 20 seconds, giving the runner time to
stop the chosen process after both presentations have closed. Before injecting
loss, both native scripts and fresh state checks must show frozen peers, the
host awaiting the client's release acknowledgement, and the client rejecting
the final release snapshot. The host must reject saving during this hold.
Host loss must explain the interruption and permit a same-process new game;
client loss must remove the departed participant and release the surviving
host into an unpaused mine. Production timeouts are unchanged. Android assembly
(56s), Windows D1/D2 builds and scoped formatting passed in
`temp/coop-briefing-release-loss-{build,windows,format}.log`. D1 host loss passed
at 08:00:19 and client loss at 08:01:31 in
`temp/coop-briefing-release-loss-d1-{host,client}-live.log`, with both native
captures per run. Host loss showed the interruption dialog and allowed a new
game in the surviving process. Client loss released the paused host with one
participant and the original host-launch reason. D2 host loss passed at
08:03:04 and client loss at 08:04:21 in
`temp/coop-briefing-release-loss-d2-{host,client}-live.log`, again with both
native captures. Both D2 outcomes also verified that movie/pause windows were
closed. All four cases retained the surviving process ID. The host's save
request was rejected while the client release acknowledgement was outstanding

Missing-movie coverage passed through D2 `-BriefingCase missing_movie`
at Counterstrike level 1 with no intro movie installed. The diagnostic
`read_briefing` value `available` permits UNAVAILABLE completion while still
rejecting Skip and requiring a stable total and monotonic viewed count. The
paired scenario requires 5/6 completed authored steps, an explicit media
unavailable status, the host-ready notice and shortened deadline, then an
all-ready launch after the client finishes the available pages. The live run
passed at 07:31:59 in `temp/coop-briefing-missing-movie-live.log`, with both
native captures. Both players finished 5/6 in UNAVAILABLE state; the host's
visible roster explicitly showed `5/6 Ready - media unavailable`. The client
received host readiness and the 20-second allowance, finished at 07:31:49,
and both verified unpaused two-player gameplay with the all-ready reason.
This exercises existing production behavior. Android assembly (1m 28s),
Windows D1/D2 builds and scoped formatting passed in
`temp/coop-briefing-missing-movie-{build,windows,format}.log`

Preparation content agreement is implemented after the counting audit
found that readiness acknowledged local planning without comparing sequences.
The engine now fingerprints the ordered selected message sections and movie
names during its side-effect-free planning pass. Optional movie bytes are not
part of this identity. The host sends its identity and authored total in every
snapshot; clients compare before acknowledging preparation, and the host
rejects acknowledgements/progress carrying another identity. A differing local
presentation is skipped with zero credited steps, the agreed denominator and
an explicit content-difference notice. It becomes ready/unavailable without
rejecting a valid mine or forcing other players to stop reading. Save-restore
suppression continues to bypass presentations. The 140-byte envelope bumps
Android D1/D2 protocol versions to 30063/30066. Live validation covers
matching content, missing optional movies, and a new
`-BriefingCase content_mismatch` which temporarily installs an empty client
briefing override and cleans it up afterward. The first D1 run reached the
expected mismatch/unavailable state but failed at 07:41:29 in
`temp/coop-briefing-content-d1-content_mismatch-live.log`: the fixture used an
unknown `debug` action, so the host never sent Skip. It now uses the existing
`set_debug` action. Both native captures were retained and the override removed;
the corrected D1 retry passed at 07:43:17 and D2 passed at 07:44:26 in
`temp/coop-briefing-content-ready-{d1,d2}-content_mismatch-live.log`, with
both native captures per run. The client opened no presentation, showed the
content-difference notice and 0/M unavailable status, then entered the mine
with the host after host Skip. The runner checked differing local identities
and a shared agreed identity, and removed both temporary overrides/staging files.
Matching-content regressions passed at 07:45:41 (D2 missing movie, both 5/6
UNAVAILABLE) and 07:47:03 (D1 normal reading, both 25/25 READY), with all-ready
launch on both peers. Logs are
`temp/coop-briefing-content-ready-d2-missing_movie-live.log` and
`temp/coop-briefing-content-ready-d1-reading-live.log`, with both native captures.
Cold save-resume regressions passed in D2 at 07:50:07 and D1 at 07:52:37 in
`temp/coop-briefing-content-restore-{d2,d1}-live.log`, with both native captures.
Each run saved the seeded inventories, restarted both processes, restored six
homing missiles per player and the saved campaign context, and verified zero
briefing presentations. Save-transfer guards, full-payload retry/acknowledgement
and stale gameplay/end-level packet rejection also passed. This confirms that
preparation still agrees on an empty suppressed presentation during restore
Android assembly (1m 6s), Windows D1/D2 builds and scoped
formatting passed in `temp/coop-briefing-content-{build,windows,format}.log`

Normal-reading coverage is being added through `-BriefingCase reading`.
The diagnostic `read_briefing` action uses Enter only during text pages and
lets videos reach their natural end. It checks that the authored total stays
fixed, completed steps never decrease or exceed that total, and a full read
ends in READY with N equal to M, rather than skipped/unavailable status. The
paired scenario holds the client after one completed step, lets the host read
everything, verifies the host's partial-client progress and the client's
host-ready notice, then finishes the client and requires all-ready launch
without waiting out the host deadline. The initial D1 run failed at 04:25:20
in `temp/coop-briefing-reading-d1-live.log`: the host read 25/25 and the
client received that READY status, but the fixture's 300-ms key interval took
the client to 24/25 before its own 15-second limit. Its range/count logs show
monotonic progress with the game deadline still pending. The full-reading
steps now use a 100-ms key interval. Corrected runs passed in D1 at 04:27:15
and D2 at 04:28:16, in `temp/coop-briefing-reading-paced-{d1,d2}-live.log`
with both native captures. Both D1 players reached 25/25 and both D2 players
reached 6/6, including the naturally completed video. Each client first held
at 1/M, observed the host's READY status/20-second allowance, then finished
the remaining pages. Both peers entered gameplay with the all-ready reason.
The `partial_skip` companion scenario reads one step before host Skip and
requires the retained 1/M count and SKIPPED status on both peers. D1 passed at
04:30:41 in `temp/coop-briefing-partial-skip-d1-live.log`. The initial D2 run
stopped at its Skip request immediately after the naturally completed video:
the new briefing page's input guard was not ready yet. The fixture now waits
for that page's real Skip activation state, preserving the production guard.
The D2 retry passed at 07:23:40 in
`temp/coop-briefing-partial-skip-ready-d2-live.log`, with both native captures:
the host retained 1/6 and SKIPPED status, the client finished 6/6, and both
entered the unpaused mine with the all-ready reason. Both temporary movie
copies and staging files were removed after validation.
Android assembly (1m 25s), Windows D1/D2 builds and scoped
formatting passed in `temp/coop-briefing-reading-{build,windows,format}.log`

Briefing membership-loss coverage (2026-09-14): `-BriefingFailure host|client`
stops the selected process while both peers are reading a fresh level-1
briefing. Host loss must close the client's mine, present an explanation until
acknowledged, and permit a new single-player game in the same process. Client
loss must remove that player from the host's roster without ending its reading
allowance; host Skip then launches the remaining player through the barrier.
The initial D1 host-loss run failed at 04:03:29 in
`temp/coop-briefing-host-loss-baseline-live.log`, with both native captures:
the client closed the mine but returned to its menu without explaining why.
The shared briefing owner now retains the host disconnect/timeout reason and
both Android main menus display it after window teardown, which longjmps out
of the presentation stack. The message is consumed once and a new level arm
clears stale reasons. The fixed D1 host-loss case passed at 04:08:37 in
`temp/coop-briefing-loss-d1-host-live.log`, with both native captures. The client
kept the explanation open until acknowledgement, returned unpaused to menus,
and started a single-player game in the same surviving process. Android
assembly (1m 23s), Windows D1/D2 builds and scoped formatting passed in
`temp/coop-briefing-loss-fix-{build,windows,format}.log`. D2 host loss passed
at 04:09:45 with the same checks. Client removal passed in D1 at 04:10:42 and
D2 at 04:11:39: the host remained reading with only its own roster bit, then
Skip launched the one remaining player with the all-ready reason. Logs are
`temp/coop-briefing-loss-{d1,d2}-{host,client}-live.log`, each with host/client
native captures. The runner verifies that the surviving process ID does not
change. These runs cover the initial authored-page briefing; the paused-video
variant is covered below. Loss during later ready/release phases, rejoin and
observers remain open

Paused-video membership loss passed at 04:15:26 (host) and 04:16:33 (client)
in `temp/coop-briefing-paused-loss-{host,client}-live.log`, with both native
captures for each run. Use D2 `-BriefingFailure host|client
-BriefingFailurePaused` with `other-h.mvl` installed. Both peers first pause
the actual `pla.mve` movie. On host loss, the client removed both movie/pause
windows, acknowledged the interruption message and started a new game in the
same process. On client loss, the remaining host stayed paused and unready,
then Skip closed the movie/pause windows and launched the sole participant.
Scoped formatting passed in `temp/coop-briefing-paused-loss-format.log`; no
native change or rebuild was required. Both temporary movie copies and staging
files were removed after the runs

Paused overall-deadline coverage (2026-09-14): `-BriefingCase paused_overall`
keeps both players in the authored `pla.mve` pause windows without host Skip
or Launch now. The first run stopped on a test error at 03:47:26 in
`temp/coop-paused-overall-live.log`, with host/client native captures. The
client entered its final 30-second wait with 117 seconds still remaining:
the automation parser intentionally reads only one operator per field, so
`{"gte": 1, "lte": 20}` did not impose its upper bound. Co-op countdown
assertions now use the supported `range` operator, including older force,
host/overall-deadline, touch and secret-advance scripts. The earlier runs'
combined-bound assertions did not verify both bounds. The corrected overall
run passed at 03:52:16 in `temp/coop-paused-overall-range-live.log`, with both
native captures. The host logged 111 seconds at 03:50:16, both peers reached
20 seconds at 03:51:46 while paused and unready, and deadline launch completed
at 03:52:07. Both verified removal of their movie/pause windows and two connected
players in the unpaused mine. The visible countdown also advanced while client
movie frame 59 stayed fixed. The corrected paused host-ready run passed at
03:54:01 in `temp/coop-paused-host-range-live.log`, with both native captures.
Host Skip changed 109 seconds to 20 at 03:53:31; the host checked 10 seconds
at 03:53:41 and deadline launch completed at 03:53:53. The client stayed paused
until expiry, then both peers passed movie/pause-window teardown. Both copies
of the temporarily staged movie library and their staging files were removed.
No native change or rebuild was needed for these corrected integration tests

The corrected D1 overall-deadline run passed at 03:57:32 in
`temp/coop-overall-range-d1-live.log`, with both native captures. The host
checked 116 seconds at 03:55:26 and 26 seconds after the 90-second wait at
03:56:56; both peers verified deadline launch, two connected players and
unpaused gameplay. This repeats the authored-page path with both timer bounds
enforced. The corrected D1 overlay-touch run passed at 03:58:48 in
`temp/coop-overlay-range-d1-live.log`, with both native captures. Repeated
physical Skip taps stayed in the wait state, dragging onto Launch now did not
launch, and a fresh tap on the separate button released both players. Scoped
formatting passed in `temp/coop-briefing-range-format.log`. These four runs
cover the newly corrected countdown assertions in the selected scenarios;
the other membership, media-counting and failure cases remain open

Paused-video force/host-deadline validation passed (2026-09-13): new D2 runner cases
`-BriefingCase paused_force` and `-BriefingCase paused_deadline` require the
authored Counterstrike level-1 video from `other-h.mvl`. The test pauses the
real movie windows on both peers, verifies a fixed client frame while the
visible countdown decreases, then checks host Skip cleanup and coordinated
closure by force launch or the host-ready deadline. Movie introspection now
reports actual frame, paused state and presence of the modal pause window.
The initial live run failed at host Skip: the movie window had closed but its
modal pause window remained (`temp/coop-paused-video-first-d2-live.log` and
host/client native captures, terminal failure at 20:31:46). The client held
movie frame 47 while its visible countdown advanced, proving the paused loop
was still pumping. Android movie teardown now closes any remaining pause
window after the event loop, covering Skip's draw-handler exit as well as
network cancellation. Fixed Android and Windows-both builds and scoped
formatting passed (`temp/coop-paused-video-fix-{build,windows,format}.log`).
The fixed `paused_force` run passed at 20:35:31 in
`temp/coop-paused-video-force-d2-live.log`, with host/client native captures and
no failed script result. Client movie frame 58 stayed fixed while the countdown
decreased; host Skip removed the pause window before exposing Launch now, and
the host launch request closed the client's paused video and released both
players into the mine. The first host-ready expiry run passed both native
launch scripts, including closure of the client's paused movie, but failed a
runner-only post-check at 20:37:36. That check read the host's earlier file
snapshot (movie frame 30, briefing remaining 118), despite its native launch
assertions having passed at 20:37:31. Final movie cleanup is now asserted on
both engine threads through a paired script, avoiding the stale file read.
Logs: `temp/coop-paused-video-deadline-d2-live.log` and native captures. The
complete expiry rerun passed at 20:41:00 in
`temp/coop-paused-video-deadline-native-d2-live.log`, with host/client native
captures and no failed script result. Client frame 48 stayed fixed as its
countdown decreased; both peers launched with deadline reason 3 and passed
the engine-thread movie/pause cleanup assertions. The yellow countdown is
visible over the actual pause screen in the inspected screenshot
`temp/coop-paused-video-reading-client.png`. The final scoped test formatter
passed in `temp/coop-paused-video-native-check-format.log`. Both emulator
copies of the temporarily staged movie library were removed after testing.
This covers host-ready expiry and force launch with paused movies; overall-limit
expiry is now covered above. Local resume/input combinations, host loss, observers
and larger rosters still need coverage

End-level packet regression (2026-09-13): D1 overlay-touch briefing and cold
restore passed at 20:19:48 in
`temp/coop-endlevel-stamp-briefing-d1-live.log`, with host/client native captures.
Repeated Skip taps stayed on the host wait screen; dragging onto Launch now
did not launch, while a fresh tap on the separate button launched through the
shared barrier. Saving, process restart, saved inventory and suppression of
briefing replay passed. Both peers rejected stale/frozen end-level packets
without changing tracked state on initial and restored visits, while accepting
current packets. MDATA, PDATA and full-info probes also passed. This does not
close the outstanding paused-video, observer, full-roster, multi-touch or
host-loss coverage

Full-info preflight regression (2026-09-13): D1 host-deadline briefing and cold
restore passed at 19:30:08 with the new shared GAME_INFO/SYNC validation, which
rejects stale/invalid metadata before changing session state. Command:
`test_lan.ps1 -Game d1 -Briefings -BriefingCase host_deadline -BriefingRestore
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`; terminal zero in
`temp/coop-info-preflight-d1-live.log`, with host/client native captures.
Initial and restored visits each rejected ten invalid parser inputs while
preserving Netgame, tokens, master ownership, reconnect identities/generation
and visit state. Host-ready launch timing, cold startup lifecycle, saved
inventory, suppression of briefing replay, transfer guards and the existing
MDATA/PDATA probes passed. This does not add the outstanding paused-video,
observer, full-roster, multi-touch or host-loss coverage

Latest transport validation (2026-09-13): D1 briefing force-launch, save and
cold resume passed at 17:23:15 in `temp/coop-mdata-loaded-life-d1-live.log`.
MDATA now retains a visit/level/frozen trailer through retries and relay; both
peers rejected old/frozen score mutations while accepting mixed typing control
after initial launch and after cold restore. The full reliable packet is now
476 bytes, and both dropped-initial-send retry probes passed. Successful briefing
release now publishes player life bookkeeping from the loaded world, because
frozen REAPPEAR traffic cannot establish that state. The same helper runs after
successful transfers. This does not cover the outstanding paused-media/input/
observer/migration cases below, or PDATA and join metadata fencing

D1 briefing/cold-resume passed at 16:07:10 on 2026-09-13 with the reliable
sequence rollover and maximum-payload buffer fixes. Command: `test_lan.ps1
-Game d1 -Briefings -BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild`;
terminal zero in `temp/coop-mdata-capacity-briefing-d1-retry.log`. The runner
preserves both peers' native evidence that a 464-byte reliable packet was
acknowledged after dropping its initial send. After both app processes restarted,
restoration completed without briefing replay, inventory checks passed, and the
host recorded acknowledgment 150778. Neither final native capture contains a
failed script result

The first run passed the packet check but failed reopening the client's launcher;
its Android system server logged a pre-watchdog event. A cold boot of emulator-5556
allowed the same APK to pass. Logs: `temp/coop-mdata-capacity-briefing-d1-live.log`
and its `-host-logcat.log`/`-client-logcat.log` captures. This does not add the
remaining paused-video, observer, rotation, multi-touch, full-roster or host-loss
coverage, and the shared gameplay-visit stamp still needs transport integration

D1 briefing/cold-resume regression passed at 15:24:38 on 2026-09-13 after moving
both engines' active-transition join guard ahead of destroyed-source rejection.
Command: `test_lan.ps1 -Game d1 -Briefings -BriefingRestore -NoCoopQol
-TimeoutSeconds 180 -SkipBuild`; terminal exit zero in
`temp/coop-early-sync-briefing-d1-live.log`. Host Skip/Launch, saving, restarting
both app processes, restoring without briefing replay, the active-transfer
autosave guard and full-width ACK checks passed. Neither native capture
`temp/coop-early-sync-briefing-d1-{host,client}-logcat.log` contains a failed
script result. This does not add the outstanding paused-video, observer,
rotation, multi-touch, full-roster or host-loss coverage

D1 briefing/cold-resume regression passed at 14:11:03 on 2026-09-13 after
the shared reactor-death departure changes. Command: `test_lan.ps1 -Game d1
-Briefings -BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild`;
terminal exit zero in `temp/coop-reactor-briefing-d1.log`. Host Skip/Launch
now, saving, restarting both app processes, restoration without briefing
replay, the active-transfer autosave guard and full-width acknowledgment
checks passed. Neither native capture
`temp/coop-reactor-briefing-d1-*-logcat.log` contains a failed script result

D1 briefing and cold-resume regression passed at 13:37:46 on 2026-09-13
with the updated shared travel protocol and death-settlement preparation.
Command: `test_lan.ps1 -Game d1 -Briefings -BriefingRestore -NoCoopQol
-TimeoutSeconds 180 -SkipBuild`; terminal exit zero in
`temp/coop-dying-briefing-restore-d1.log`. Host Skip/Launch now, saving,
restarting both app processes, restoring both inventories without briefing
replay, the active-transfer autosave guard and full-width acknowledgment
checks passed. Neither `temp/coop-dying-d1-*-logcat.log` capture contains
a failed script result. This does not extend the outstanding paused-video,
observer, rotation, multi-touch or host-loss coverage

D1 briefing and cold-resume regression passed at 12:42:55 on 2026-09-13
with the latest shared transfer code and campaign-return-context fix.
Command: `test_lan.ps1 -Game d1 -Briefings -BriefingRestore -NoCoopQol
-TimeoutSeconds 180 -SkipBuild`; log:
`temp/coop-return-context-briefing-d1.log`, terminal exit zero. Coordinated
force-launch, saving, process restart, restoration of six homing missiles on
both peers, suppression of briefing replay, the transfer autosave guard and
full-width reliable acknowledgment checks passed. Final native log captures
contain no failed script result

D2 natural advancement from a secret mine with a destroyed base passed on two
emulators at 12:34:27 on 2026-09-13. The `-SecretAdvance -AllowSecretWarps
-InitialLevel 8 -NoCoopQol` scenario advances from -2 to level 9, opens the
new mine's authored presentation, applies host Skip and then Launch now,
and releases both players with the expected campaign and inventory state.
Log: `temp/coop-advance-activation-live-d2.log`, terminal exit zero; neither
peer's native log contains a failed script result. The Skip fixture waits
for the briefing page's activation gate, including its tap-suppression interval.
Optional missing media was skipped in this run; this does not test force-closing
a playing or paused standalone movie

D1 briefing and cold-restore regression passed at 11:04:08 on 2026-09-13
after the destination ship-index and frozen-position restore fixes shared
with secret travel. Command: `test_lan.ps1 -Game d1 -Briefings -BriefingRestore
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-secret-cold-briefing-regression-d1.log`. Both players completed
the coordinated briefing launch, saved, restarted their app processes and
restored six homing missiles without replaying presentations. The transfer
autosave guard and full-width reliable acknowledgment assertions also passed

Physical Android overlay coverage passed on two emulators for both D2 and D1
on 2026-09-13 using `test_lan.ps1 -Game <game> -Briefings -BriefingCase
overlay_touch -NoCoopQol -TimeoutSeconds 180 -SkipBuild`. The runner uses
actual Android input at the display-sized Skip and Launch now hit targets.
Repeated Skip taps left the host waiting with the native countdown at most
20 seconds. A drag beginning outside Launch now and ending on it did not
launch. A fresh tap on Launch now closed the remaining presentation and both
peers passed the mine-release assertions with host-launch reason, rather than
deadline expiry. Terminal-zero logs: `temp/coop-briefing-touch-retry-d2.log`
and `temp/coop-briefing-touch-d1.log`. Both games' native transition policy
tests also passed, including the late-host five-second remainder case

The first D2 touch attempt timed out starting SetupActivity on emulator-5556,
before any briefing ran. Its Android activity commands stalled while basic
shell commands remained responsive; the retry passed after a cold emulator
restart. Diagnostic log: `temp/coop-briefing-touch-device-stall.log`

This adds live touch coverage at the tested 1920x1080 layout. Rotation,
multi-touch cancellation, eight-player roster layout and force-closing a
paused video still need coverage

D1 briefing/cold-resume regression passed at 04:27:33 on 2026-09-13 with the
fair reliable retry scan and full 32-bit receive/relay packet numbers. Options:
`-Game d1 -Briefings -BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild`.
Log: `temp/coop-sequence-width-resume-d1.log`, terminal exit zero. The restore
fixture advances the sender to 65534 and requires a live outgoing packet above
65535 to be acknowledged. Both players restored six homing missiles, the
briefing option survived, no presentation replayed, and an autosave attempted
during the real transfer was rejected. This does not add coverage for physical
overlay taps, paused videos, observers or host loss

Latest shared-protocol regression: D1 passed two-peer briefing force-launch,
saving, cold resume with six homing missiles and no briefing replay after the
frozen travel-player handoff changes. Command options: `-Game d1 -Briefings
-BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild`. Log:
`temp/coop-portable-final-verified-save-d1.log`, terminal exit zero at 00:14:40
on 2026-09-13. This does not add coverage for the outstanding physical overlay
touch, paused-video, observer or host-loss scenarios

Part of the same work as [co-op secret teleporters](coop_secret_teleporters.md).
Share operation ownership, participant readiness, status reporting, and recovery
mechanisms, while keeping each phase's gameplay rules explicit

Cold-resume lifecycle correction (2026-09-13, D1/D2 validation passed): the
PDATA follow-up stopped before the restored host lobby because startup rotation
recreated MainActivity. Native logs showed two library loads in the same process,
and Android reported configuration changes `0x580` (orientation, screen size and
screen layout); the manifest omitted screenLayout. MainActivity now handles that
change without recreation. RuntimeGameStateBridge also binds and unbinds through
the same application context, correcting the observed leaked service connection.
Debug lifecycle records identify activity instances and handled configurations.
The cold-resume test rejects recreation/service leaks and prints lifecycle proof;
gameplay probes retain their native evidence before launcher restart clears logs.
Scoped formatting and the Android build passed. The paired D1 host-deadline /
cold-restore run passed at 18:16:37 (`temp/coop-resume-lifecycle-d1-live.log`,
with host/client native captures). Both initial and resumed startup delivered
portrait -> landscape configuration to the same activity. Saved player state,
no briefing replay, restore guards and visits 1 -> 2 passed, with MDATA/PDATA
evidence retained for both peers on both visits. The D2 touch/restore follow-up
passed at 18:19:28 (`temp/coop-resume-lifecycle-d2-live.log`, with native captures).
Repeated Skip taps stayed at the host wait screen, a drag ending on Launch now
did not activate it, and a fresh tap launched both players. Cold startup retained
one activity instance through rotation; inventory, suppressed briefings, restore
guards and released-world packet checks passed on both peers

## Requested experience

- Optional briefing pages and videos in co-op
- Host sees each player's progress as completed steps N/M and current activity
- Host completion or Skip goes to a launch/wait screen, never straight into play
- Host can immediately pull everyone out of briefings with a second overlay
  button in a different location from Skip/Next, preventing accidental double-taps
- Clients still reading see that the host has finished and is waiting for them
- Maximum reading time is 120 seconds from the agreed briefing start
- Host completion or Skip shortens the remaining time to 20 seconds, or keeps
  the existing remaining time if it is already shorter; it never extends the
  two-minute limit
- Show the effective countdown on briefing pages, videos, and waiting screens
- One fresh tap on the separate host `Launch now` overlay immediately ends
  everyone's remaining briefing time, with no extra confirmation or countdown
- Everyone starts the mine together after loading and synchronization
- Launch is never assigned to tapping anywhere or to the generic advance action

The normal-exit correction remains binding: an exited player waits while
teammates continue escaping or dying under the live reactor countdown. The
briefing phase for the next mine begins only after ordinary end-level completion.
It never freezes players who are still trying to leave the previous mine

## Existing code and integration points

| Location | Finding |
| --- | --- |
| `d1/main/gameseq.c:StartNewLevel` | Briefings are skipped under `GM_MULTI`, before `StartNewLevelSub` |
| `d2/main/gameseq.c:ShowLevelIntro` | Multiplayer guard skips level intro movies and briefing pages; built-in, demo/OEM, and custom/D1-emulation selection differ |
| `d2/main/gameseq.c:StartNewLevel` | Calls `ShowLevelIntro` before loading/synchronizing the level |
| `d1/main/titles.c`, `d2/main/titles.c` | Native briefing parsing, page/screen lifecycle, and input handling; inspect both when adding hooks |
| `d2/main/titles.c:do_briefing_screens` | Waits in an `event_process()` loop until its window closes |
| `d2/main/movie.c:RunMovie` | Movie window/event loop needs phase networking, cancellation, and cleanup integration |
| `d2/main/net_udp.c:net_udp_level_sync` | Existing load/sync barrier is distinct from finishing presentation |
| `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | Existing `SkipButtonView`, `nativeRequestScreenAdvance(generation)`, and generic tap-to-Enter path; launch must bypass generic advance |
| `android/app/src/main/cpp/shared/android_screen_advance.h` | Existing screen-generation interface; use its protection when wiring Skip and the separate launch action |

Removing the multiplayer guards alone would leave independent presentation
loops ahead of the normal sync path, without progress/deadline messages or a
safe host-driven cancellation path. Source locations are investigation anchors,
not proof that these loops currently service all necessary network traffic

## Setting and presentation scope

Add an independent `Show co-op briefings and videos` Yes/No server setting
beside the co-op options. Proposed initial default: No, preserving current
behavior until enabled. Neither this option nor secret warps requires the other
or the general QoL switch. Carry the setting through host defaults, advertised
session configuration, native launch, save/resume, and host migration

Initial scope: pre-mine story/level briefings and intro videos selected by
existing D1 and D2 mission logic, including applicable D1-in-D2/custom missions.
Startup logos and endgame cinematics are not automatically added to this phase.
Enable content only where the engine's normal presentation rules select it;
do not invent a new briefing for a secret teleporter return/revisit

Use at a fresh game start and natural progression into a new mine. Save-load,
rewind, restart, and joining an already running game do not replay briefings.
A secret departure that advances naturally to a new normal mine can use that
mine's authored briefing after the secret transition has chosen its destination

Existing local Skip/skip-intro preferences may mark only that player's
presentation complete; they never authorize session launch. Distinguish
startup-video preferences from level-briefing preferences rather than silently
applying a startup-logo skip setting to all mission content

## Player flow

1. Host fixes the destination, participant roster, presentation ID, content
   sequence, and reading deadline; clients acknowledge entry into the phase
2. Each participant reviews its own pages/videos at its own pace and reports
   progress. No mine simulation, game clock, or player controls run yet
3. Finishing or pressing Skip closes that player's remaining presentation and
   marks them presentation-ready. A client sees a waiting screen. The host
   sees the roster and a dedicated `Launch now` overlay button
4. When the host becomes ready, clients still reviewing content see `Host is
   ready and waiting for you` plus remaining time, while retaining page controls
5. Host Launch now or deadline expiry ends remaining presentations for everyone.
   Clients see `Host is starting the mine` or `Briefing time is up` as appropriate
6. All clients close presentation safely, load/synchronize the same mine, and
   report mine-ready. The host releases gameplay only after that barrier

When everyone finishes voluntarily, use the same coordinated launch path
without an additional reading countdown. Host Skip still enters the launch/wait
state; it never directly invokes StartLevel or bypasses readiness

The host's screen shows, for example:

| Player | Progress | State |
| --- | --- | --- |
| Host | 5/5 | Ready |
| Alex | 2/5 | Watching video |
| Sam | 4/5 | Reading page |
| Lee | 1/5 | Ready - skipped remaining steps |

Waiting is the default while someone is reading. No extra confirmation dialog
is required for Launch now. A client cannot force launch. The host cannot
accidentally force everyone by repeatedly advancing its own pages

## Overlay and input contract

Use a real overlay button with its own hit target, label, and accessible action.
Show Launch now on the host's launch/wait screen after host completion/Skip.
Place this second overlay button in a different screen location from Skip/Next,
with non-overlapping hit targets across the screen change. Do not replace Skip
with Launch now in the same position. Preserve that separation across supported
orientations, scaling, and safe-area layouts. Keep launch separate from generic
menu or tap-anywhere behavior
Use the existing overlay style/layout where practical; keep text and progress
legible over videos and clear of subtitles and page controls

Bind activation to a native host-authorized launch request carrying the current
presentation/operation generation. Stale taps from a previous screen are ignored.
Require a fresh press/release begun on the launch control; a held Skip or a tap
that began on the previous page cannot land on the new button. Generic Enter,
advance, or tap-anywhere events cannot launch from the waiting screen. Controller
and keyboard support use explicit button focus/activation with no inherited
confirm press or default focus that consumes a held page-advance input

Once launch is requested, disable the button, show loading/waiting status, and
coalesce duplicates. Launch now immediately ends the remaining reading allowance
without an additional display delay; it does not add another countdown or
confirmation. It still requires safe presentation closure and the mine-ready
barrier before gameplay starts. Keep a way to leave the session. Completing the overlay
must restore video/audio/palette/input state and remove it on success, abort,
disconnect, or host change. Flush held inputs before gameplay release

## Progress and finite waiting

Define a step as one logical briefing page or one standalone video in the
selected pre-mine sequence. Embedded looping robot animations are part of a
page, not additional videos. N counts completed steps; the current page/video
does not count complete merely because it opened. Skip is a separate ready
state and must not claim the player viewed all remaining steps

The engine determines M through the same mission selection and parser semantics
used to display the sequence. Do not count raw page markers in Kotlin: engine
commands, D1 screen selection, and layout-driven page breaks need reconciliation.
Prefer logical authored pages for comparable totals; verify correspondence to
displayed pages. Counting must be side-effect-free and must not play audio,
mutate game state, or mark content as already seen

Display each player's own N/M without comparing presentation content or totals.
The end/Skip signal alone marks the player ready; reaching N=M does not imply
readiness, and an explicit end signal does not require N=M.
Represent no content as ready with 0/0. Missing/unsupported optional media is
reported as unavailable and advances that step; it never causes infinite wait
or a fabricated viewed count. Fatal mission/gameplay asset mismatches still
fail normal content validation. While counting, show `Preparing briefings`
with a bounded preparation timeout rather than guessing a denominator

Send bounded progress updates on step/state changes and periodic heartbeats;
do not stream every video frame. Host relays a consistent roster to waiting
players. Use presentation ID, stable participant identity, and monotonic message
revision so delayed progress cannot turn ready back into reading or affect the
next mine. A reading player may revisit pages without revoking terminal ready

User-specified timing, kept as named policy constants:

- Overall briefing limit: 2 minutes after the agreed presentation start
- Once host finishes/skips: at most 20 additional seconds for remaining readers,
  bounded by the existing overall deadline
- Display remaining time throughout; emphasize the final 10 seconds without
  flashing or obscuring content
- The host can launch earlier from its overlay. Overall expiry also ends the
  host's unfinished content and proceeds through the same launch barrier

The effective deadline is `min(presentation_start + 120s, host_ready_time + 20s)`
once the host is ready; before that, it is `presentation_start + 120s`.
Show the effective remaining time to the host and clients during pages, videos,
and the waiting screen. Host completion at 1:55 leaves 5 seconds, not a fresh
20 seconds beyond the two-minute maximum. Long videos may be cut short by this
limit; do not silently extend the user-specified budget for them
The deadline is host-authoritative and uses monotonic real time. Page changes,
video pause, duplicate packets, late joins, and screen rotation do not reset it.
Starting the 20-second host-ready interval can only shorten the existing budget

Timing acceptance examples (elapsed from presentation start):

| Host action | Effective deadline | Visible time immediately after action |
| --- | --- | --- |
| Finishes or skips at 0:30 | 0:50 | 0:20 |
| Finishes or skips at 1:55 | 2:00 | 0:05 |
| Still reviewing at 2:00 | 2:00 | 0:00; close everyone's presentation |
| Taps the separate Launch now button while waiting | Immediately | Replace timer with loading/synchronizing status |

The Launch now control requires one deliberate fresh tap, with no second
confirmation. The user's second overlay button means a distinct control from
Skip/Next, not a second tap on the same control

Briefing allowance is separate from load/sync and disconnection timeouts.
Reaching zero requests closure and loading; it cannot release a client whose
mine is not ready. Do not count intentional reading time as a stalled level load

## Shared phase and synchronization design

Extend the common co-op operation controller with `BRIEFING`,
`BRIEFING_WAITING`, `CLOSING_PRESENTATION`, and the existing load/ready/release
states. Per-player presentation-ready differs from mine-ready. The host's
launch/wait screen is just the ready host's UI within the briefing operation

Normal progression is: individual normal exits/deaths -> existing end-level
completion -> destination fixed -> optional briefing phase -> load/sync ->
gameplay release. The secret warp warning and briefing are separate reasons
for waiting; share infrastructure without applying either to normal escape

Every page, movie, and waiting loop must service phase network messages,
liveness, deadline updates, and close requests even while the game window is
hidden. Existing `event_process()` alone must not be assumed sufficient.
Prevent normal `NETSTAT_WAITING` auto-start logic from releasing gameplay merely
because players reached a legacy load/sync hook early

Force launch ends the entire remaining sequence, not just the current window.
Check the authoritative close state before opening each next page/video.
Packet handlers queue the change; perform window/media cleanup on the owning
engine thread at a safe event boundary, acknowledge closure, then load. Do not
call StartNewLevel recursively from a network/UI callback or abruptly replace
a world while a movie decoder is still using its resources

Use existing network level synchronization plus a verified final release gate.
No ready host/client runs physics, takes damage, consumes timed effects, fires,
or starts mine timers while another participating client is still loading.
Where loading order makes preloading difficult, load after presentation closes;
parallel loading is an optimization, not a correctness requirement

Start together means one coordinated ready/release decision with no early
play advantage, not identical CPU timestamps across a network. A loading peer
must become ready or be removed through the established timeout policy before
the remaining team is released. Force-launch overrides reading time only; it
does not override mission validation, loading failure, or network readiness

## Save/load, recovery, and membership

Block save/load/rewind/restart during briefing, waiting, closure, and loading
with a specific reason; defer autosaves until settled gameplay. Do not save a
half-read page as a playable checkpoint. Preserve the last good campaign save
and destination/operation metadata needed to recover to the lobby. Capture the
new level's restart/rewind baseline only after the coordinated start is ready

For initial simplicity, host loss during briefing/loading returns participants
to the lobby with recoverable campaign state, consistent with the unfinished
secret-transition policy. A failed client or screen-off participant follows
bounded timeout/removal; no client launches independently when the host vanishes.
Closing the app or leaving remains possible from every presentation/wait screen

Fix the participating roster at phase start. New joins wait for committed mine
startup and then use ordinary join/rejoin, rather than restarting everyone else's
deadline. A reconnecting participant receives current progress/deadline/launch
state and never reopens a stale briefing after force-launch. An observer may
view content and status but cannot force launch or hold up the active team

Progress reporting and intentional waiting must survive Android media callbacks,
activity recreation, and UI polling without resetting authority or the deadline.
After a timed-out player reconnects, use the committed session state rather
than completing a cached local skip/launch action

## Work stages and readiness checks

Implementation progress: `shared/coop/coop_transition_policy.{h,c}` implements
the 120-second limit, 20-second host-ready deadline reduction, generation-checked
progress and host launch, and separate presentation-closed, mine-applied, and
release barriers. Host CMake tests cover early/late host completion, forced
launch with a slow participant, stale progress/acks, and up to eight participants.
These are coordinator tests, not evidence that the live briefing UI is wired

Runtime integration now lives in `shared/coop/coop_briefing.{h,c}`. Natural
StartNewLevel calls arm the gate before StartNewLevelSub, then present after
the existing initial level sync has established player slots. Gameplay stays
blocked throughout. The final presentation-closed, mine-ready and release
barriers remain separate even though initial mine loading happens before reading

The native presentation selection is run first in counting mode, then for
display. Engine-selected message sections supply authored page totals; local
layout overflow does not change the denominator. Standalone videos count once.
Presentation loops pump networking and close only at safe loop boundaries,
including cleanup of a paused movie's child window

`MULTI_COOP_BRIEFING` carries authenticated host snapshots and participant
progress/acks with session, level, generation and revision checks. UI polling
and launch requests use a locked mailbox; `CoopBriefingOverlayView` puts the
dedicated fresh-tap launch action at bottom-left, away from upper-right Skip.
Save/load/rewind/restart entry points reject requests during the briefing gate.
Android save metadata now records the two independent session settings

`test_lan.ps1 -Briefings` and the paired `test_coop_briefing_{host,client}.jsonc`
scripts exercise the live path. Passing the build alone does not complete this
stage: retain the timeout, reconnect, media, progress and layout checks below

Validation completed on 2026-09-12:

- Android D1/D2 native builds and Kotlin compile passed; debug x86_64 APK built
- Windows D1/D2 builds passed, including the updated v11 metadata ABI fixture
- Both games' coordinator, player-session and save-format executables passed
- `test_lan.ps1 -Game d2 -Briefings -SkipBuild` passed on two emulators
- `test_lan.ps1 -Game d1 -Briefings -SkipBuild` passed on two emulators
- Live tests verified host Skip, at-most-20-seconds remaining, continued client
  reading while simulation is paused, rejected client/stale launch requests,
  generic Enter leaving the wait intact, explicit host launch and both peers
  entering gameplay with the same launch reason. D2 selected six steps; D1
  First Strike selected 25. Tests invoke the real native overlay action bridge;
  they do not yet prove the Android hit targets or actual page-count completion
- D1 and D2 `-BriefingCase host_deadline` passed on two emulators: the host skipped,
  the client kept reading, and both entered the mine on the 20-second deadline
  without an explicit Launch now request
- D1 and D2 `-BriefingCase overall_deadline` passed on two emulators: neither player
  finished or skipped, both remained in presentation with simulation paused,
  and the two-minute deadline closed both presentations and released the mine
  after synchronization
- Visual inspection on both emulators confirmed the countdown above the D2
  briefing text, the client host-waiting message, and Launch now at bottom-left
  on the host's wait screen, away from upper-right Skip. This does not replace
  a real touch-input or eight-player layout test

Resume suppression, fixed-roster join deferral, and session-generation reset
are implemented. Resume uses a host-advertised suppression flag and retains
the readiness barriers without presenting pages or videos. D2
`-Briefings -BriefingRestore -SkipBuild` and D1
`-Briefings -BriefingCase host_deadline -BriefingRestore -SkipBuild` passed on
both emulators after the
transfer-pacing fix: each restored six saved homing missiles, retained the
briefing option, returned to settled gameplay, and opened zero presentations.
The reusable runner now clears stale initial restore selections and tolerates
the temporary absence of player records during level loading

Save-transfer chunk pacing now uses elapsed time
at 480 chunks/second with a 64-chunk maximum burst, replacing eight chunks per
rendered frame. This preserves the previous 60-fps throughput while allowing
slow frames to catch up within the existing transfer timeout. Both engines'
host tests cover a complete 864-chunk transfer at two fps, rate limiting at
high frame rates, clock rollback, and bounded catch-up after a stall. Android
and Windows builds passed. The live D2 transfer synchronized in about 15
seconds; the old frame-limited sender had timed out at 60 seconds with only
792/864 chunks sent. The existing timeout and apply-readiness checks remain

Observer handling now accepts authoritative presentation state without adding
non-host spectators to the acknowledgment roster. Host snapshots use direct
packets, and the observer control path bypasses optional spectator gameplay
delay. Observing hosts retain their deadline and launch authority. Live observer
coverage remains required; the ordinary D2 two-player force-launch/resume path
passed with direct host snapshots

Build artifact detail: passing `-Pandroid.injected.build.abi=x86_64` produced
the test APK at `app/build/intermediates/apk/debug/app-debug.apk`, requiring
`adb install -r -t`. The conventional `outputs/apk/debug` file was stale and
was excluded from the successful live test evidence

Final release now has its own acknowledgment/retry tracking. The host retains
the operation gate until all participants acknowledge receiving SETTLED;
clients retry that acknowledgment during the release grace period. This keeps
a subsequent save-transfer BEGIN from overtaking a client's briefing release.
Introspection exposes the release acknowledgment mask. The paired
`test_coop_briefing_release_delay_{host,client}.jsonc` scripts, selected with
`-BriefingCase release_delay`, drop client release snapshots for six seconds
and verify host pause/save rejection followed by successful retry and release.
D1 and D2 passed this case followed by cold save/resume on both emulators

The v12 cold-resume test exposed an autosave/restore race: while sending the
selected slot to peers, periodic autosaves could replace that slot, and the
host then reopened a different save at apply time. Autosaves now reject both
pending restore status and active transfer/application. Host application retains
and restores the captured transfer buffer, exactly as the receiving peers do,
instead of reopening a mutable slot. The reusable briefing-resume scenario arms
`test_coop_restore_autosave_guard.jsonc` before startup and requires an attempted
autosave during the real transfer to be rejected

Windows D1/D2 and Android builds passed for this fix. The D2 two-emulator run
with `-Briefings -BriefingRestore -AllowSecretWarps -NoCoopQol -SpewPartialPickup`
passed: both peers restored six homing missiles without replaying presentations,
then completed death/respawn and partial recovery pickup. This validates the
normal-world save/resume path; actual travel and settled secret saves remain
unverified

D1 also passed `-Briefings -BriefingRestore -SpewPartialPickup`, including the
active-transfer autosave rejection, cold restore and subsequent recovery pickup.
Logs: `temp/coop-restore-race-live-d1.log` and
`temp/coop-restore-race-live-d2.log`

After adding the separate dormant-world restore mode, the D2 two-emulator
briefing/cold-resume regression passed again with secret warps enabled and QoL
disabled. The runner now budgets initial mine loading separately from save
application; its startup and restore waits do not change the native 120/20-second
reading deadlines. The slow host's texture loading had exceeded the former
combined test budget while the client correctly waited. Latest passing log:
`temp/coop-world-restore-verified-save-d2.log`

The same D2 briefing/cold-resume scenario passed after connecting frozen secret
travel to the shared transfer frame loop. Both peers restored six homing
missiles, preserved campaign metadata and the briefing setting, rejected an
autosave during the active transfer, and opened no presentations on resume.
Log: `temp/coop-travel-world-gate-save-d2.log`, exit code zero on 2026-09-12

Known remaining runtime work before declaring briefings complete:

- Verify reconnect before participant removal while the host is still reading,
  and joins during closure/loading. First-time reading-phase joins and reconnect
  after removal passed in D1/D2 above; two-player removal during pages and paused
  video is also covered
- Verify a new multiplayer session does not inherit a previous generation
- Verify ordinary-observer behavior. Observing-host controls passed in D1/D2
  above. Content identity/count disagreement is intentionally outside scope
  after the user's simplification. Fully viewed base-mission
  pages and all-ready launch without an extra countdown passed in D1 and D2;
  D2 missing-movie completion passed above
- Verify loss during closure/loading and overlay recreation/multi-touch;
  final-release host/client loss, paused-video force-close, reading-phase
  host loss and fresh-tap input have passed the device tests above

- [x] Record requested co-op briefing/host overlay flow and relation to secret travel
- [x] Locate D1/D2 suppression, presentation loops, and existing Android Skip wiring
- [x] Set the requested 120-second overall / 20-second host-ready limits and spatially separate immediate-launch button
- [x] Define native presentation plan/counting and local-completion/skip semantics
- [x] Audit D1/D2 presentation networking and safe whole-sequence cancellation
- [x] Add the independent host setting and consistent session propagation
- [x] Add progress/deadline messages and host/client status views
- [x] Add explicit Launch now overlay with generation and fresh-input checks
- [x] Integrate final mine-ready barrier, timeout policy, and operation gate
- [x] Validate both options independently and together using real multiple peers

Option-combination audit (2026-09-14): the current-build secret-only and combined
secret-to-new-briefing runs above complement the D1/D2 briefing-only reading,
missing-movie and cold-resume runs. Those briefing-only commands omit secret
warps: `SetupActivity.kt` reads the absent launch extra as false, and
`net_udp_android_autonet_shared.c` assigns the two settings independently.
The physical test directly checks both settings after entry and return; the
advance test directly checks both enabled on both peers during level 9's
briefing. These close the option-combination item without claiming the separate
observer, rejoin or full-roster acceptance cases are complete

Barrier implementation audit (2026-09-14): `coop_transition_policy.c` advances
through presentation closure, readiness and commit acknowledgements for the
current participant set. `coop_briefing_run` keeps the loaded mine hidden and
time stopped throughout; the host retains operation ownership until release
has reached each participant. The shared pump keeps networking active, removes
timed-out clients and exits with an explanation on host loss. The final-release
tests above verify both frozen sides of this boundary, save rejection, and
recovery after either peer departs in D1 and D2. `coop_save.c`,
`multi_save_transfer.c`, `android_rewind.c` and `coop_level_restart.c` also check
`coop_briefing_active` before beginning state-changing work, while
`coop_gameplay_runtime.h` marks briefing traffic as frozen. Earlier delayed-
release and cold-resume tests cover retransmission and return to settled play.
This closes the implementation item; the separately listed lifecycle and
membership acceptance cases remain open

Presentation implementation audit (2026-09-14): the native planning hooks select
the same message sections/movie names as the actual D1/D2 sequence. Shared
scanning defines each player's authored-page total. Normal completion credits
viewed steps and independently signals readiness. Skip preserves the partial
count and absent movies produce UNAVAILABLE completion. The base-mission
reading, partial-Skip and missing-movie runs above establish the original
presentation behavior; current simplification validation is recorded at the top

Status-view audit (2026-09-14): `coop_briefing.c::update_ui` produces the visible
remaining time, host roster N/M and activity, and client host-ready notice.
`MainActivity.kt` polls the JNI snapshot into `CoopBriefingOverlayView.kt`,
which draws every status line in yellow during the native presentation and
waiting phases. The inspected D1/D2 normal-reading runs above verify changing
counts, host/client status and all-ready release; the paused overall/host-ready
deadline logs verify a decreasing timer while actual movie frames stay fixed.
Together with the earlier overlay inspection this closes the implementation
item, while observer/full-roster and layout lifecycle checks remain separate

Checklist audit (2026-09-14): the Launch now implementation item is complete.
`CoopBriefingOverlayView.kt` uses a bottom-left hit target distinct from
upper-right Skip, requires a down/up pair inside that target, and cancels
presses on generation changes, resize, movement outside or a second pointer.
`coop_briefing_request_launch` rejects stale generations and duplicate queued
requests under the UI mutex. MainActivity/JNI route only that explicit button
action to the native request. The retained D1/D2 overlay-touch integration
logs (`temp/coop-briefing-touch-d1.log` and
`temp/coop-briefing-touch-retry-d2.log`) both passed; the runner checks repeated
Skip taps, a drag onto Launch now and a fresh deliberate launch tap. Broader
rotation, multi-touch, observer and full-roster validation remains listed below

Settings audit (2026-09-14): `CreateGameDialog.kt` exposes independent briefing
and D2 secret-warp switches beside co-op QoL. `MatchmakingState.kt` defaults
both off and persists each preference independently. `MultiplayerScreen.kt`,
`LobbyProtocol.kt`, `LobbyService.kt` and `MatchmakingService.kt` pass both
fields to launch; `net_udp_android_autonet_shared.c` assigns them independently
of QoL, and both native UDP readers receive them. `coop_save.c` records both
in save metadata, both `state.c` readers restore them, and
`MultiplayerResumePrefs.kt` retains them for session resume. The inspected D1
briefing/cold-resume and D2 secret save/revisit runs passed with QoL disabled
(`temp/coop-endlevel-stamp-briefing-d1-live.log` and
`temp/coop-secret-endgame-save-revisit-live.log`). This closes the setting and
propagation implementation item; it does not claim the separate rejoin,
observer or presentation-counting checks are complete

Test host/client finish order, host Skip on first/middle/last step, client Skip,
all-ready, no-content, missing movie, parser totals, custom missions, long videos,
slow video decoding, paused video, and overall/host-ready deadline expiry.
Assert host completion never releases gameplay while readers/loaders remain

Assert visible countdowns and launch at the effective deadline: host ready at
0:10 gives 20 seconds, host ready at 1:55 gives 5 seconds, and a host still
watching at 2:00 is pulled out too. Repeated ready messages do not reset the
timer. Launch now ends the allowance immediately, even with a video playing

Force launch during text animation, a page boundary, video decode, media cleanup,
and the next-video opening race. Spam Skip, tap-anywhere, Enter, and held controller
confirm; none may invoke Launch now accidentally. Only an explicit host overlay
action or a documented automatic deadline/all-ready path ends everyone's briefing

Double-tap the old Skip location across the waiting-screen change and verify
no launch. Verify the second button's hit target is elsewhere in each supported
layout, and a deliberate fresh tap there requests launch exactly once

Inject progress/launch duplicates, stale prior-level messages, dropped close/ready
acks, client timeout, screen-off, host death, reconnect, and observer delay.
Assert no early simulation, no permanent hidden window/audio, and recovery to a
known state. Test a slow loader after force-launch: others wait without playing

Verify normal exit waiting still lets remaining players escape or die before any
next-level briefing. Verify secret returns/revisits, save loads, and rewinds do
not replay an unwanted briefing or retain stale progress/deadlines

Expose presentation ID, step plan/count, each player's status, host-ready state,
deadline remaining, launch reason, closure/load acknowledgements, and action-block
reason through introspection. Use these assertions in a multi-peer test runner;
visually inspect overlay placement, subtitles, progress readability, and input
behavior on devices in addition to native state checks
