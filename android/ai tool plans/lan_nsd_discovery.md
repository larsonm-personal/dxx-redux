# Android NSD discovery

## Objective

Add Android DNS-SD/mDNS as an independent source of LAN lobby endpoints, retaining broadcast and remembered/direct-IP discovery. Finish implementation and local verification, with an ARM64 build and an ADB-driven phone test ready for reconnecting both phones.

## Design

- Advertise `_dxxredux._udp.` on launcher UDP port 42400, with protocol version and lobby UUID
- Resolve IPv4 endpoints, then use existing unicast QUERY/ANNOUNCE validation and lobby deduplication
- Keep discovery sources separate from transport diagnostics; retain valid UDP lobbies when NSD loses a service
- Handle registration, discovery, network changes, cancellation, late callbacks, and older Android resolution APIs
- Refresh resolved candidates using bounded unicast retries without persisting them as manually entered addresses
- Keep all production changes in Android Kotlin and the Android manifest

## Verification

- Build and scoped formatting
- Emulator integration with broadcast and remembered-host queries disabled, requiring NSD resolution, unicast confirmation, and lobby join
- Verify normal discovery, deduplication, stop/restart, and both hosting roles
- Prepare a non-destructive ADB runner for explicit phone serials; do not clear phone app data
- Prepare ARM64 APK and document any signing requirement for updating installed internal builds

## Progress

- Implemented platform NSD registration/browsing and IPv4 endpoint resolution, with service-info callbacks on API 34+ and serialized, periodically refreshed legacy resolution
- Added lifecycle/network recovery, stale callback rejection, unicast confirmation and discovery-source diagnostics
- Existing confirmed lobbies continue direct probes after NSD loss; mDNS does not own lobby liveness
- Added debug-only NSD-only, legacy-resolution, and advertisement-suspension controls
- Emulator integration passed both hosting roles, forced legacy resolution, deduplication with all discovery paths enabled, Wi-Fi reconnect, advertisement withdrawal/re-registration, and lobby join/readiness
- Focused lobby/discovery/file-provider unit tests: 31 passed
- Full unit suite: 1088 tests, 2 failures in unchanged SoundfontDownloadTest cases (expected ymfm preference, got sf2), 1 skipped
- Scoped formatter/linter passed
- Phone's Play signing certificate differs from the local upload key. A separate diagnostic application ID avoids uninstalling the real app or losing saves
- Diagnostic package installs alongside the normal app; fresh, automatic base-game provisioning passed on both emulators
- The same four-round integration suite passed again with the separate diagnostic package, including Wi-Fi reconnect and advertisement withdrawal/recovery
- ARM64 APK signature, application ID, debug flag, and both native game libraries verified; x86_64 diagnostic counterpart also built and exercised
- Implementation and local verification complete; ready for the physical phone test after both phones reconnect

## Phone handoff

Build the ARM64 diagnostic APK from `android/` with JDK 21:

```powershell
.\gradlew.bat :app:assembleDebug '-Pandroid.injected.build.abi=arm64-v8a' '-PnsdDiagnosticApp=true' --offline --console=plain
```

The staged phone APK is `android/temp/lan-nsd/dxx-redux-nsd-diagnostic.apk`, package `com.dxxredux.app.nsdtest`, launcher label `DXX-Redux NSD Test`. It is a separate debug installation; the Play-installed app and its data stay intact. The test temporarily stops the normal app so it releases UDP 42400. Do not uninstall either app to resolve a signing error.

After both phones reconnect and are unlocked, install the staged APK using `adb -s SERIAL install -r -t -g APK_PATH` on each phone, then run from the repository root:

```powershell
.\android\tests\test_lan_nsd.ps1 -HostSerial RFCY703C48J -ClientSerial R3CR40Q4XPK -AppPackage com.dxxredux.app.nsdtest -ProvisionDiagnosticData -ReconnectCoverage
```

Provisioning loads the repository's pinned base-game fixtures into only the separate diagnostic app. It does not copy or reset the normal app's preferences or saves. The runner restores screen timeout behavior, ends test lobbies, and resets its runtime discovery overrides. `-ReconnectCoverage` briefly toggles the client phone's Wi-Fi. Logs and structured round results are written to `android/temp/nsd-test/`.

## Physical phone results (2026-09-30)

- Installed the separate diagnostic APK on S21 R3CR40Q4XPK and S25 RFCY703C48J; normal app data preserved
- Four rounds passed: S25 hosting, S21 hosting, forced legacy resolution, and combined discovery
- Both NSD-only directions resolved and confirmed the host through unicast UDP with broadcast and saved-IP discovery disabled; each produced one lobby and completed join/readiness
- S21 Wi-Fi reconnect recovered fresh NSD confirmation
- Withdrawing the host's mDNS advertisement for more than 35 seconds preserved the reachable lobby through direct probes; re-advertisement resolved successfully
- Both phones were on U6-Lite BSSID f6:92:bf:91:f3:d6 at 5805 MHz before and after testing, with addresses 192.168.88.21 and 192.168.88.163
- Runner exited successfully, stopped test lobbies, reset discovery overrides, and restored stay-on settings
- Evidence: `android/temp/nsd_phone_integration.log`, `android/temp/nsd-phone/results/`, and before/after Wi-Fi snapshots in `android/temp/nsd-phone/`

This establishes working mDNS discovery on the physical phones on the same 5 GHz radio. The earlier 2.4 GHz / 5 GHz cross-band arrangement remains untested with mDNS. These launcher tests do not exercise gameplay or level transitions.

## Cross-band reproduction (2026-09-30)

- At the user's request, selected S21 BSSID f4:92:bf:a1:f3:d5 using `cmd wifi connect-network` with `-b`; association changed after a Wi-Fi reconnect
- Verified S21 at 2462 MHz and S25 at 5805 MHz, same SSID and original IP addresses, before, during, and after testing
- Raw UDP before NSD: S25 to S21 received 10/10 unicast, 0/10 subnet broadcast, 0/10 limited broadcast; reverse received 10/10, 4/10, 4/10 respectively
- Raw UDP after NSD: S25 to S21 again received 10/10, 0/10, 0/10; reverse received 10/10, 3/10, 5/10
- S25 hosting: NSD registration succeeded, but S21 discovery timed out after 60 seconds without a service-found event
- S21 hosting: S25 resolved and confirmed the lobby by unicast UDP, but the advertisement-withdrawal check timed out after 60 seconds; the runner stopped before join/readiness checks
- Thus neither cross-band integration run passed; mDNS did not overcome the reproduced discovery failure in the original S25-host/S21-client direction
- Evidence: `android/temp/nsd-cross-band/` and `android/temp/phone_udp/cross_band_{pinned,after_nsd}_summary.json`
- Initial measurements before the pin took effect are explicitly named `same_band_pin_not_applied`, not cross-band results
- Test lobbies and overrides cleaned up, raw probe removed; S21 left on the requested 2.4 GHz radio for continued investigation

The cross-band result supersedes the earlier pending status. It implicates the Wi-Fi delivery path but does not isolate AP forwarding from phone Wi-Fi multicast/broadcast handling. mDNS remains useful as another discovery layer, not a demonstrated solution for this failure.

## Swapped-band attempt (2026-09-30)

- Requested S21 on 5 GHz and S25 on 2.4 GHz; initial status confirmed those associations
- Subsequent raw UDP received 10/10 of all three types at S21; S25 received 10/10 unicast, 5/10 subnet broadcast, 4/10 limited broadcast
- All four NSD integration rounds passed, but the final radio check showed S25 had roamed to 5 GHz; these results cannot establish sustained cross-band success
- Two further attempts to select S25's 2.4 GHz BSSID failed the radio check before a shorter, continuously checked NSD test could start
- The BSSID request is not a demonstrated persistent radio lock on S25; initial raw measurements also lack continuous association verification
- Both phones ended on 5 GHz; test overrides/lobbies cleaned up and raw probes removed
- Evidence: `android/temp/nsd-swapped-bands/` and `android/temp/phone_udp/swapped_bands_summary.json`
- A controlled swapped-band test still requires keeping S25 on 2.4 GHz, for example a UniFi 2.4 GHz-only SSID on the same LAN

## Broadcast-query fallback test (2026-09-30)

- Verified original radio BSSIDs during discovery: S21 on 2.4 GHz, S25 on 5 GHz; current frequencies were 2437/5785 MHz and S25 address was 192.168.88.165
- User reported no deliberate network changes; an initial attempt was aborted because S25 was on another BSSID, then original radio association restored
- Diagnostic preferences had no remembered LAN addresses or resume record; stopped client NSD before creating the host and suspended host NSD too
- Normal bidirectional broadcast run discovered a lobby and joined; host broadcast arrived just before the correlated unicast query reply, so this alone did not prove fallback-only discovery
- Stricter run suppressed host broadcasts using its debug NSD-only flag, with NSD suspended on both phones; client retained broadcast queries
- Third client broadcast QUERY (ID 1debf424-1fac-4b53-88d7-0765cd2f474f) reached the host; unicast ANNOUNCE ID 3608cd8b-dc35-47a5-8459-9c0f7d432313 carried the matching reply ID and subnet-broadcast strategy
- Client logged `source=query-reply`, then JOIN_ACK and two players; discovery took about 15 seconds from host creation, 12.5 seconds from first query
- No host broadcast sends or client NSD resolutions appeared in the strict run; this demonstrates the existing broadcast-query/unicast-reply fallback working across bands
- Raw control between runs showed broadcast reception had become partial in both directions (S21 8/10 subnet, 7/10 limited; S25 4/10, 3/10), with unicast 10/10 both ways; the earlier permanent-looking zero reception was not reproduced in this session
- Evidence: `android/temp/broadcast-query-test/` including correlated packet traces and radio samples, plus `android/temp/phone_udp/broadcast_query_control_summary.json`
- Restored diagnostic preferences, stopped diagnostic processes (clearing runtime overrides), and removed temporary device probes; normal app data untouched

## Faster discovery follow-up

- Target useful discovery within five seconds by querying immediately and every 500 ms during the first five seconds
- Continue unanswered searches every two seconds, then return to six-second refreshes after finding a lobby
- Retain two-second save/pruning maintenance cadence and avoid rapid background/hosting polling
- Verify formatting, ARM64 build, and repeated physical broadcast-query/unicast-reply discovery with host broadcasts and NSD disabled
- Implemented and passed scoped formatting and ARM64 diagnostic build
- Three physical fallback-only rounds discovered the lobby in 1.999, 0.898, and 1.375 seconds from client discovery start; each logged source=query-reply and completed lobby join
- Radio samples retained S21 on original 2.4 GHz BSSID and S25 on original 5 GHz BSSID; no remembered IP targets or NSD resolution used
- Evidence: `android/temp/broadcast-query-fast/results.json`, packet traces, radio samples, and three runner logs
- Updated separate diagnostic APK installed on both phones; diagnostic preferences restored and test processes stopped afterward
- Five seconds is a discovery target, not a guaranteed deadline on a network that may drop every broadcast; this small sample establishes improvement in the tested conditions

## Adversarial review and cleanup

- Review discovery lifecycle, repeated/stale callbacks, polling costs, diagnostic accuracy, and test cleanup
- Remove redundant probes from identical NSD updates and rapid polling after the initial search window
- Keep NSD suspension stable across lifecycle updates, reset it when discovery stops, and discard expired confirmation IDs
- Ignore obsolete legacy resolution failures and clear closed session references during restart
- Report announcement transport accurately instead of labeling every non-query reply as broadcast
- Keep the separate diagnostic package and bounded test hooks: they enable repeatable discovery regression coverage without replacing Play-signed installations
- Verify scoped formatting, focused unit tests, build, and the existing NSD integration suite on emulators
- Verified: scoped formatting and x86_64 diagnostic build passed; 31 focused tests passed; all four emulator integration rounds passed with Wi-Fi reconnect and the new suspension-persistence check
- Review verification evidence: `android/temp/discovery_review_build.log`, `android/temp/discovery_review_integration.log`, and `android/temp/nsd-test/results.json`
- Phones were disconnected during review verification; the previously measured physical fast-discovery results predate these cleanup changes
