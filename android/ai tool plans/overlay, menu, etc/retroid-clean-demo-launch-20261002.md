# Clean Retroid D2 demo startup reproduction

User explicitly requests uninstalling the apps, reinstalling, downloading the D2 demo, and launching on Retroid Pocket 4 Pro JYPR42510121028

1. Save retained logs, exit history, package details and the installed production APK before uninstalling
2. Uninstall both installed DXX packages without retaining private data
3. Reinstall the exact production APK and use the launcher's Mac D2 demo offer, preserving first-launch defaults
4. Capture full logcat and periodic process memory from before launch, inspect whether the game reaches its first menu and remains stable, then enter gameplay if startup succeeds
5. If the production failure repeats, use the debug build for introspection and allocation investigation; avoid speculative fixes
6. Record outcomes, restore temporary device logging/power settings, and leave a working installation

Evidence lives in temp/retroid-clean-launch. Production private data is inaccessible to run-as; uninstall intentionally removes it as requested

## Completed reproduction

- Saved the exact installed production APK, versionCode 23730, SHA256 018fca483019f0ef4f9d76767780b97d60af342a2c80bf05c9ac9a39f2b04a81, plus retained package details, logcat and exit history
- Uninstalled com.dxxredux.app and com.dxxredux.app.ci without retaining data; verified no DXX package remained before reinstalling the saved APK
- Used the launcher's actual Download and install offer for D2 Demo on each attempt; both downloads extracted the same five Mac preview assets and sizes/hashes as the original investigation
- Attempt 1: download completes 18:39:35.024, engine starts 18:39:51.904, first pilot screen appears with working IME, pilot is created, level 1 gameplay starts around 18:41:30. Monitored through 18:42:40 before the second uninstall. Peak game RSS 330152 KiB (322.4 MiB), peak PSS 200985 KiB (196.3 MiB); final RSS 313808 KiB
- Attempt 2: another full production uninstall/reinstall, download completes around 18:42:51.569 and engine starts 18:42:52.685. Used ADB gamepad-source input through intro/pilot/menu, including B to dismiss IME, Down to select OK, and A to accept. Reached the main menu, waited past the original failure interval, then started level 1 around 18:44:39. Monitored through 18:45:20. Peak game RSS 308816 KiB (301.6 MiB), peak PSS 178405 KiB (174.2 MiB); final RSS 306296 KiB
- Metadata workers stayed below 137 MiB sampled RSS. No sustained growth, app low-memory kill, native fatal signal or Java fatal exception occurred in either attempt. Neither produced the continuous keyboard-hide calls from the failed session

The black-screen/OOM failure did not reproduce. This does not establish a fix or rule out an intermittent first-launch defect. User cannot recall whether the original session reached the intro or pilot screen

Full streaming logcat is clean-logcat.txt, periodic dumpsys memory samples are memory.jsonl, normalized process peaks/times are memory-summary.json, and gameplay screenshots are gameplay.png and attempt2-gameplay.png. Memory sampling started before the first demo download and continued across both attempts; sample timestamps precede sequential process queries by up to several seconds. Production debug broadcasts are unavailable, and Android uiautomator returned null root nodes, so ADB input and inspected screenshots were used for the exact release build

Returned to the launcher, restored stay_on_while_plugged_in=0 and the unset log.tag.DXX-DLOG, stopped the host logcat capture, and removed the temporary device screenshot/UI dump. Only the fresh original production app remains installed, with the Mac D2 demo and newly created Player pilot. The CI app remains uninstalled. No application source change was made for this reproduction

## Third attempt after repeated user request

Repeated the complete production uninstall and verified no DXX packages remained, then installed the same saved original APK. The device was at its ordinary Retroid lock screen; an A-button event unlocked it before using the demo offer. The Mac preview download/extraction completed at 19:04:33.057, and game PID 15068 launched at 19:04:34.275. The intro, pilot screen and menu appeared normally; used B/Down/A to complete the pilot screen, then A to start a new game and choose difficulty. Level 1 began around 19:05:21.477 and remained visible through 19:06:54

Memory sampling ran for three minutes from before the download, including about 2 minutes 20 seconds after engine launch. Game peak RSS was 344428 KiB (336.4 MiB), peak PSS 215899 KiB (210.8 MiB), and final RSS 311396 KiB (304.1 MiB). The in-game metadata worker peaked at 139896 KiB (136.6 MiB) RSS. No black screen, low-memory kill, native fatal signal, Java fatal exception or sustained keyboard-hide loop occurred. The original intermittent defect remains unresolved

Evidence is under temp/retroid-clean-launch/attempt3. Restored both device settings to 0/unset, stopped the sampler/logcat capture, removed the temporary device screenshot and returned to the launcher. The fresh original production app with the demo remains installed; no app source changes were needed
