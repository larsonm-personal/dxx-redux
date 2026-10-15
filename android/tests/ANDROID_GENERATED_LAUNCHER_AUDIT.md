# Generated launcher owner source audit

Source commit: cf0fe171413fdb17542aa447689ca0a20f54461d. Source-only review; no PowerShell, emulator execution, or downloads.

Admit exactly:
- test_demo_group_file_set_content: preserve original d2 run and all four inline demo/sidecar payloads.
- test_unified_file_set_content: preserve original d2 run and inline panic.mn2/panic.hog ownership, toggle, switch, deletion checks.
- test_soundfont_download_catalog: preserve BOTH original d1 and d2 runs. The catalog and licenses are bundled. It checks link buttons, opens confirmation dialogs, and cancels; no website or SF2 download is executed.

For each, add the exact name to the jsonc:emulator media-independent allowlist plus one master/android-single policy entry with args containing only suite. Retain existing Name==BaseName/type/infrastructure checks and the exact computed -TimeoutSeconds allowance. Do not supply -Game, -DataEdition, or -Params. The helper exemption removes both master standard-retail preflight and runner recovery provisioning. No new selected-owner public fixture setup is needed.

Transitive proof: LauncherScriptExecutor implements every used action. SETUP_COMMAND uses ordered broadcasts with failure propagation; SetupActivity forwards set operations to FileSetManager/FileSetContentManager. Writes are local inline payloads, catalog scan groups descriptor/demo sidecars, and state mutations/projections are local. Assertions trigger fresh SetupAutomationApi introspection, including the exact content IDs, paths, enabled state, and soundfont state. JSON proof includes all original actions and exact source SHA-256s.

These still require a full compatible debug APK and one disposable emulator. Launcher introspection loads dxx-redux-d2 through MIDI/CD bridges, even without entering gameplay. Soundfont DEFAULTS calls the real native activation path and fails if bundled gm.sf2 cannot load; do not stub it. gm.sf2 SHA-256 is 2f68d824f456e3367fe24105d590ee77b7e28faa9f622723b478fa90647d8a1a; bundled license assets are also required. The existing Gradle preBuild already downloads/verifies it if absent. Thus “no-download” means no additional/runtime test fixture download: ordinary APK host build access still needs its existing authorization or verified cache.

No new guest network, SAF picker, runtime permission grant, or persistent access is needed. Run only against disposable app storage: fixture setup/deletion is real. Add positive dictionary/object admission tests and negative type/infrastructure/extra-argument/alias tests before hosted validation. Runtime pass evidence remains outstanding after the KVM repair.

Keep test_pc_d2_demo_music deferred. Its unchanged test explicitly performs a live guest GitHub download. Current setupDownloadFile/installDemoArchive does not compare the fetched archive against DemoInstallerPackages’ declared SHA-256, despite checking required output filenames and hashing extracted files. A verified in-app archive identity gate/response fixture and allowed guest egress are concrete prerequisites; preserve all original music assertions.
