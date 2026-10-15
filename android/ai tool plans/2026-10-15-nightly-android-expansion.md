# Additive Android owner audit

Source baseline: cmake e12e0bcf4c2fe6e3edd0302f75dd5c4666e9bb70, integrated nightly stack 3eeed6684f31b55858e32083580f771fdb9242f3. Source admission is not runtime pass evidence.

## Additional media-independent single-device owners

- `tests/test_slider_navigation.ps1` dispatches `slider_navigation` and requires its existing PASS marker, rejecting instrumentation failure/crash. `SliderNavigationChecks.kt` runs production Compose controller/graphics settings, orientation/focus navigation and temporary config fixtures. Its launcher automation callback throws if a game launch is attempted. Transitive `GraphicsChooserLayoutChecks.kt` uses generated capability JSON/bitmaps; `NavigationRepeatChecks.kt` uses synthetic key/axis dispatch. No media input is needed. The explicit `CapabilitiesOnly` variant is not covered by this default-owner admission.
- `tests/test_lobby_latency.ps1` dispatches `lobby_latency`, preserving its existing marker/error checks. `LobbyLatencyChecks.kt` uses real UDP loopback endpoints 127.0.0.1/127.0.0.2:42400, synthetic mission status and injected `hostedSaveCheckForTest`, restored in finally. PREPARE/START tests manipulate service launch events without starting a game Activity. `LobbyService.refreshHostedSaveWarning` also calls `CoopSaveCompatibility.hostRevision`, which handles absent selections through runCatching; `MissionTransferService.startHost` immediately rejects non-wrapper/default requirements. Requires emulator UDP sockets, not retail files or another device.
- `tests/test_host_dialog_loading.ps1` dispatches `mission_loading`, preserving its existing marker/error checks. `MissionLoadingChecks.kt` injects blocking/failing catalog loads to check responsive cancellation/retry and avoids its host-launch callback. `MissionCatalog.load` delegates to `MissionScanner` in `multiplayer/MissionPicker.kt`; builtins have absent-file fallbacks, and missing mission directories use listFiles().orEmpty(). No mission content is opened by an engine.

All three use the existing exact name/type/default-arguments fixture policy. `SetupActivity.onCreate` loads the packaged JNI library and launcher services; engine launch remains an explicit callback, not a startup requirement. A working APK/native library remains required.

## Dual-emulator candidates and topology

- `test_lan_lobby_discovery.ps1` default path launches only SetupActivity, hosts a synthetic builtin lobby and observes discovery. Reset-DeviceGameState publishes default-path markers but does not require media bytes. Optional Join/Resume/ScreenOff/Reconnect variants are outside this proposed default-only admission. Reconnect explicitly changes emulator Wi-Fi state and is not implicitly admitted.
- `test_lan_broadcast.ps1` compiles a small native UDP probe with the installed NDK when emulator Python is absent. Existing ping/unicast/broadcast/subnet-broadcast assertions need no APK/game data. Its previously nonfatal failure messages now contribute to nonzero exit; ping's pattern no longer accepts 100% loss as 0% loss. Focused source-contract regression covers all five failure sites and loss-pattern negative controls.
- Official documentation specifies shared virtual Wi-Fi by default in Emulator 36.5+, while older versions isolate instances: https://developer.android.com/studio/run/emulator-networking-interconnect . Existing bootstrap accepts older installed versions, so the nightly dual lane checks emulator >=36.5 and two distinct wlan0 addresses before dispatch. Actual broadcast delivery is still asserted by the existing tests; version/address checks alone are not pass evidence. No bridge, forwarding or security settings are changed.

## Further candidates requiring concrete fixture work

- `test_demo_import.ps1`: public corpus import plus D1-in-D2 runtime checks, not a retail-only exclusion. Requires all 11 original archives listed and SHA-pinned in `tests/fixtures/demo_import_oracles.json`; current two-installer shareware bootstrap is insufficient. A complete verified public corpus provisioner is needed before admission, retaining every import/runtime case.
- `test_enemy_within_import.ps1`: requires full `game_data/mission_files/ewithin-versions.zip`; no substitute/sliced fixture is acceptable. Public download/provenance and package constraints need verification.
- `test_lan_qr.ps1`: synthetic QR rendering/router plus actual scanner Activity, with camera grant. Media-free source, but scanner/emulated-camera/permission prerequisites require a separate explicit audit.
- Manual dual setup, LAN discovery bot and manual co-op owners remain genuinely interactive. Other gameplay owners remain pending source/fixture audit, not presumed hardware- or retail-only.

## Validation

No local PowerShell or Android execution. Focused Python source-contract test passes; actionlint passes modified reusable workflow. Hosted source/runtime checks must provide actual results. Do not equate owner count with nested assertion/variant coverage.

## Four launcher-only automation owners

`game_scripts/test_compute_faster_dialog.jsonc`, `test_debug_log_refresh_button.jsonc`, `test_controller_raw_inputs.jsonc` and `test_controller_trigger_axis_priority.jsonc` are standalone D2 catalog entries with no declared media dependencies. Their complete action sets are launcher entry, UI taps/button assertions, synthetic controller reports, state assertions and waits; none enters a game or imports content. Existing launcher automation result/run-id checks remain authoritative.

The shared fixture policy admits these exact JSONC names/types only. `helpers/run_test.ps1` otherwise restored retail media during launcher-startup recovery even for these zero-dependency scripts; the narrow change skips only that recovery provisioning when the script is the canonical repository path, parameters are empty and DataEdition is default. Unknown/custom scripts, extra arguments and existing gameplay owners retain the data guard.
