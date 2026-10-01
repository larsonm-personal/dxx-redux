# LAN QR joining design

Use `descent://192.168.1.42` as a compact invitation to a host on the local network. Reserve a square in the Android hosting lobby and in the lower-left corner of the native host player-selection screen, but keep the QR code hidden by default and reveal it only after the host taps inside that square. Add **Read QR code** beside the LAN discovery actions, and register the URI scheme so compatible camera apps can open DXX Redux.

Status: implemented, with emulator integration coverage. Physical-camera and network-topology checks below remain manual. This document addresses the QR entry in `android/outstanding_bugs.md`.

## Existing integration points

Paths below are relative to `android/app/src/main/` unless otherwise stated.

| Area | Existing behavior and proposed use |
| --- | --- |
| `java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt` | Contains the scanning view, **Your Hosted Lobby**, manual-IP dialog, and its cancellable join job. Add QR controls here; `LobbyScreen.kt` is the separate matchmaking lobby |
| `java/com/dxxredux/app/lobby/LobbyService.kt` | `tryJoinLobbyByIp(..., probeEngine = true)` queries a launcher lobby and can probe a native game. Reuse this path for scans and links |
| `java/com/dxxredux/app/multiplayer/NetworkConstants.kt` | LAN lobby UDP port is 42400; engine default is 42424 for both D1 and D2. Lobby announcements carry the actual engine port and game variant |
| `java/com/dxxredux/app/MainActivity.kt` | Already polls `nativeIsHostSelectingPlayers()` to show the Start Game overlay. Reuse that exact state for QR visibility |
| Repository `d1/main/net_udp.c` and `d2/main/net_udp.c` | Both set and clear `g_host_selecting_players` around the host player-selection menu. No new native lobby hook is needed |
| `AndroidManifest.xml` | `SetupActivity` is the exported launcher; `MainActivity` runs in `:game`. There is currently no URI handler or camera permission |

## Invitation format

The initial format is exactly `descent://<IPv4>`, with no game variant, port, mission, build number, or lobby identifier. Use lowercase `descent` when generating it.

The address points to a machine, not a particular lobby session. A scan asks that machine what it is currently hosting. The normal LAN handshake supplies the game, mission requirements, build information, and engine endpoint. This also means an old screenshot can find a newer lobby at the same address; the QR is not an identity or access token.

Keep the discovery port stable at `NetworkConstants.LAN_LOBBY_PORT`. The engine default remains `NetworkConstants.ENGINE_PORT`; use the advertised engine port when available. A bare engine with a custom port and no launcher announcement is outside this first format. Do not add ambiguous `:port` support until its meaning is defined. Both games currently share the same defaults, so encoding D1 or D2 is unnecessary.

Implement one bounded parser shared by the scanner and external intent handler. Accept four decimal IPv4 octets and an optional trailing slash; reject user information, explicit ports, query strings, fragments, other paths, hostnames, IPv6, malformed octets, and encoded authority tricks. Normalize leading zeroes as decimal or reject them consistently. Reject unspecified, loopback, multicast, limited broadcast, and a known local subnet's broadcast address. Do not restrict valid hosts solely to `192.168.*`; Ethernet, hotspots, and other LAN address ranges must work. The parser must not resolve DNS or open arbitrary URLs.

Generate a black-on-white QR with medium error correction, a four-module white quiet zone, and integer pixel scaling without smoothing. Keep the scheme lowercase for Android intent matching. Let the encoder choose the smallest symbol; verify the actual matrix size in the implementation instead of forcing a version. Shorter data generally permits fewer modules, although capacity also depends on encoding and error correction. [QR capacity reference](https://www.qrcode.com/en/about/version.html)

## Host display

In both host views, reserve the full QR square even while the code is hidden, using a neutral **Tap to show QR code** placeholder with no scannable pattern. Only a tap inside that square reveals the code; focusing, hovering, tapping elsewhere, or receiving a network update must not reveal it. Explicit controller/accessibility activation of the same square is the equivalent action. This is a security measure against incidental scanning of a visible screen, not authentication for joining the LAN game. Keep the reveal state local to the current host view and reset it when leaving that view, pausing the Activity, changing the invitation address, or ending the hosting session. Entering the native lobby starts hidden even if the launcher code was revealed.

In **Your Hosted Lobby**, place the reserved square beside the player list in a compact row. Use a 132 dp square including the quiet zone. The first activation reveals the code in place without changing the layout; a subsequent activation hides it. Long-pressing the visible code opens the enlarged dialog, with no long-press hint shown. The enlarged dialog shows the address, **Scan to join**, and **Same Wi-Fi or local network**, with Close and Hide code actions. Multiple available addresses have a labeled selector below the square. Preserve the existing two-player layout goal: chat and Start remain usable without adding avoidable outer scrolling.

In the native host lobby, use a small Kotlin `LanJoinQrOverlay` above the SDL surface. Anchor it lower-left inside safe drawing bounds with an 8 dp margin. Cap the square at 112 dp, 10% of the surface width, and 28% of its height to clear the native player checkboxes. The existing Exit control is top-left and Back is bottom-right. Reserve the QR rectangle for tap-to-show/hide and long-press-to-enlarge, and leave touches elsewhere available to the current menu overlay. Hide the expanded view before starting play.

The native placeholder requires a LAN host launch and `nativeIsHostSelectingPlayers()`; displaying the code additionally requires a valid local address and explicit reveal. Keep the square reserved but disable revealing while no usable address exists. Hide the entire overlay on gameplay, menu departure, activity pause, polling failure, and destruction. Clear the reveal state on these events and on network loss. Do not infer lobby visibility merely from "not in game" or a generic waiting network status. Matchmaking hosts and clients must not show a local-host invitation.

The first implementation covers launcher-hosted LAN games in both the Compose lobby and their native player-selection lobby. Hosting entirely through the classic native menus needs explicit endpoint/transport knowledge before advertising a QR, especially with custom ports; add that as a follow-up rather than accidentally showing a relay or unreachable endpoint.

## Selecting the host address

Replace the invitation's dependence on `remember { getLocalIpLabel() }`, which currently caches an enumeration of all non-loopback IPv4 interfaces. A stale address or a VPN interface would produce a valid QR for the wrong route.

Add a small shared Android address provider that observes usable Wi-Fi/Ethernet networks and their IPv4 link addresses, with interface enumeration for hotspot hosting where needed. Prefer the active physical LAN and its primary IPv4 link address over secondary aliases, cellular, or VPN interfaces; do not require internet validation: an isolated router or hotspot is a valid LAN. If multiple plausible LAN addresses exist, let the host choose a labeled interface/address. Refresh on network changes, preserve a selection only while it remains valid, and hide the code while no usable address exists. Avoid an address carousel that changes while someone is scanning.

The launcher and game processes can each observe local interfaces using this same provider; local address lookup does not require reading the other process's `LobbyService` singleton. Pass the selected address/interface as a launch hint, validate it against current interfaces in the game process, and observe changes there too. This keeps the common case consistent without adding a new IPC protocol. Cache QR bitmaps by payload and size, never regenerate them on every overlay poll, and release network callbacks with the owning lifecycle.

## Scanning and joining

Add **Read QR code** near **Join by IP**, visible even when discovery finds no servers. It opens a QR-only scanner with Cancel and optional torch controls. Request camera permission only on this action. If unavailable or denied, keep manual-IP joining usable and explain why scanning is unavailable. Explicitly mark camera and autofocus hardware optional in the merged manifest so TV and camera-less devices remain supported.

Proposed dependency: `com.journeyapps:zxing-android-embedded:4.3.0`, pinned through the existing version configuration. It provides a scanner Activity, Activity Result integration, portrait/landscape support, and ZXing-based generation/decoding. Its documented default SDK requirement fits the repository's minimum SDK 24. Reuse its ZXing core for QR generation and verify the resolved dependency versions and merged manifest during implementation. [Library documentation](https://github.com/journeyapps/zxing-android-embedded)

This bundled approach is intended to work on an offline LAN without requiring Google Play services or a first-use model download. Google Code Scanner offers a permission-free app integration but delegates scanning to Play services and downloads its scanning module, making it a less suitable default here. [Google Code Scanner documentation](https://developers.google.com/ml-kit/vision/barcode-scanning/code-scanner)

Extract the existing manual-IP join action into one shared UI operation. A valid scan closes the camera, waits until the LAN screen is resumed and discovery/permissions are ready, and invokes that operation. This ordering matters because `tryJoinLobbyByIp` cancels progress while the app is backgrounded. Retain the current callsign, progress display, cancellation, recent addresses, and mission/game readiness checks. Do not duplicate the connection protocol or launch the engine directly from decoded text.

Invalid content produces **This is not a Descent LAN invitation** with Scan again and Cancel. A valid scan immediately starts **Looking for a lobby or running game at 192.168.1.42...**. A timeout offers Retry and Edit address using the existing same-network diagnostic. Scanning bypasses failed broadcast discovery; it cannot bypass client isolation or lack of a route to the host.

## Opening from another camera app

Android can register a custom URI scheme with an exported Activity and an intent filter containing `ACTION_VIEW`, `DEFAULT`, `BROWSABLE`, and `<data android:scheme="descent" />`. A wildcard host declaration is unnecessary; validate the address in application code. Custom schemes can have multiple handlers and may show a chooser. [Android deep-link documentation](https://developer.android.com/training/app-links/create-deeplinks)

Use a small exported, no-content `LanJoinLinkActivity` as the entry point. Give this short-lived router an empty task affinity and exclude it from Recents so a repeated cold-start URI cannot merely foreground a task whose base intent matches the old invitation. Validate the URI there, then explicitly deliver only the normalized address and a request ID to `SetupActivity`; finish the router immediately. Bring the existing launcher forward with `REORDER_TO_FRONT | SINGLE_TOP`, handling the request in both launcher `onCreate` and `onNewIntent`. Do not use `CLEAR_TOP`, which could destroy the game Activity above the launcher. This avoids changing the launch mode of every ordinary launcher entry. Test same-task and external-task delivery while `MainActivity` is running: neither bringing the launcher forward nor receiving a link may end the active game automatically. [Activity flag semantics](https://developer.android.com/reference/android/content/Intent#FLAG_ACTIVITY_REORDER_TO_FRONT)

Store one pending join request in saved launcher state until setup, callsign, permissions, and LAN navigation are ready. Consume each request once; rotation, recomposition, repeated camera callbacks, and returning from Settings must not start duplicate joins. Clear the consumed intent data/extras. A deliberate new scan may retry the same address. Start the join only while the launcher is resumed. If already hosting, joined, or running a game, show the destination and offer to leave the current session first; use the existing shutdown flow only after that choice.

External-camera support remains device-dependent: Android handling works when the camera emits a VIEW intent for the scheme, but some cameras treat unfamiliar schemes as text. Verify representative camera apps on physical devices. The in-app scanner is the dependable path. A future verified HTTPS App Link could provide a web/install fallback, but it requires an owned domain and a longer payload; it is unnecessary for the initial offline LAN feature.

## Implementation sequence

1. Add the shared URI parser, live address provider, and cached QR rendering. Show the host card with its reserved, hidden-by-default QR square; verify explicit reveal and that displayed payloads use the selected physical LAN address
2. Extract the manual-IP join operation and add the scanner Activity Result flow, permission handling, and cancellation
3. Add the external-link router and pending launcher request handling, including cold start, warm start, and an active game
4. Add the native lobby Kotlin overlay, reuse the existing native host-selection flag, and pass the preferred address with the LAN launch intent
5. Run scoped formatting/lint, relevant Gradle compilation/tests, and serial Android integration tests. Native hooks are not planned; if implementation exposes a need for one, check both D1/D2 CMake builds and preserve desktop builds

## Acceptance and validation

- In both host views, verify that the full square is reserved with no QR visible initially, only activation inside the square reveals it, and revealing causes no layout shift. Check that entering the native lobby, returning after pause, changing address, and starting a new host session require a fresh reveal; unrelated taps, focus, recomposition, and network updates must never reveal it automatically
- Parser and QR round-trip tests cover generated addresses, optional trailing slash, malformed authorities, non-invitation content, and the longest accepted payload
- A reusable Android integration runner exercises the real pending-request and join path against a host with discovery broadcasts unavailable. Test both D1 and D2, launcher lobby and native player-selection lobby, and reaching the game through the existing readiness flow
- Intent tests use `adb shell am start -W -a android.intent.action.VIEW -d "descent://<host>" <installed-package>` for a cold launcher, existing launcher, active game, rotation, duplicate delivery, and missing setup files
- On two physical devices, scan the actual displayed code in both host views, offline, in portrait/landscape and at small screen sizes. Check menu zoom, insets, controller focus, enlarged QR dismissal, and that gameplay has no QR overlay
- Exercise Wi-Fi address changes, hotspot hosting, Ethernet plus Wi-Fi, a VPN, network loss, and a host that stops responding. The code must refresh or disappear rather than advertise a stale endpoint
- Cover scanner cancellation, camera denial, camera-less TV, missing Play services, full/closed/incompatible lobbies, and repeated attempts. No scan should bypass existing mission transfer consent or readiness checks

## Implementation verification log

- Added shared invitation parsing, live physical LAN address selection, and a reusable hidden-by-default QR square
- Integrated the square into the launcher host card and native host-selection overlay, with lifecycle and address-change resets
- Added bundled scanning and an exported URI router, reusing the existing manual-IP join operation and checking active-session conflicts
- Kotlin compilation and the new invitation/reveal unit tests passed
- Full JVM suite ran 1091 tests: two existing soundfont preference assertions failed (`ymfm` expected, `sf2` actual); the QR tests passed
- `android/tests/test_lan_qr.ps1` passed on an emulator: actual widget pixels decode only after activation; focus does not reveal; address change and conceal remove the code; the exported router preserves an existing lobby; the real scanner Activity Result path rejects a non-invitation
- `android/tests/test_lan_qr_join.ps1` passed for D1 and D2, each with `-HostLobby launcher` and `-HostLobby native`: a cold URI joins and reaches two-player gameplay; a subsequent invitation during play offers Cancel and preserves the game process
- Debug app and instrumentation APKs built successfully, including both D1/D2 x86_64 native libraries; scoped formatting/lint and `git diff --check` passed
- Host screenshots verified the concealed launcher square and the native square's clearance from player controls. Full camera-to-screen scanning still needs physical phones
- The final D2 native run also passed `-CheckDisplayedQr -CheckLeaveAndJoin`: automation located the hidden square, tapped it, joined its revealed address, cancelled a subsequent invitation, then confirmed another invitation and observed the old game exit. The revealed screenshot independently decoded to the same URI
- Prefer the primary link address: the emulator's secondary Wi-Fi alias answered ping but failed the native join probe. The primary address passed the displayed-invitation test
- Remaining manual matrix: real camera apps emitting custom-scheme intents, camera denial/camera-less devices, Wi-Fi/hotspot/Ethernet/VPN combinations, small screens/menu zoom, and network changes during a scan

Run device tests serially with the debug and instrumentation APKs installed. The join runner uses `emulator-5554` and `emulator-5556`, requires matching game assets, and scopes its automation broadcasts to the app package. D1 uses the empty built-in mission key; D1-in-D2 uses `descent`.
