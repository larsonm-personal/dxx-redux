# Phone resource ownership and readable errors

The Samsung SM-S931U / Adreno 830 rendered the original hub on 5455715,
version 29180. Its next transition failed with a clipped missing-camera error.
Authorized read-only app resource inspection found neither cambot.hxm nor
d2x-xl.hog. The selected mission was the original imported pmines_v11.7z.

Candidate 604ecde157de87e0717b1b651cc0d78ac792d08b, version 29210, wraps Android
menu titles and subtitles using their actual font measurements, including long
unbroken paths. The displayed strings and menu bounds are exposed through
introspection. D1 and D2 share the helper; desktop rendering is unchanged.

The bundle installer now supplies the original camera HOG and Vertigo HAM at
the selected retail file-set root. `-ImportedMission` preserves the existing
imported mission without staging another loose mission. Launcher publication
schema 2 explicitly lists installation mounts from the selected file-set
projection. Native validation accepts only an exact listed, currently mounted
source. Arbitrary global mods and unlisted child mounts remain rejected.

## Exact candidate evidence

Bundle: `temp/pumo-phone-readable-playtest-20261102`, with README, manifest,
installer and evidence. APK SHA-256:
`cf37935cfffb017d1f51c6b64579c597e860a0d838db43629369e883d09531da`.
All six ARM64/ARMv7/x86_64 D1/D2 library IDs match 604ecde / 29210.
ZIP SHA-256:
`d726e8eb01335945d7bc8f0ade91d2f0156cff79e2ed4fe01fe79eeaade8d77d`.

- Complete original pmines_v11.7z imported through the launcher: 38/38 PASS,
  run 349296f9f6d54141b591841e710002e6, 110.484 seconds of native steps.
  Normal laser/thrust door and exit activation reached level 11; hub save/load
  and repeat transition also passed. Fixed local approaches, no controlled exit
  dispatch. This was an owned Android 14 x86_64 SwiftShader emulator.
- Original archive SHA-256:
  `82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf`.
  Original HOG and descriptor identities were verified separately. No mission
  content rewrite or reduced package was used for this passing import run.
- Missing-camera integration: 22/22 PASS, run
  e46227bda8664bf6986a1f768ce0dc36, 70.775 seconds. Full error text and OK are
  visible in the captured render; measured menu bounds fit the screen. The
  blocked transition keeps the player in the hub and grants no progression.
- The imported-only installer preserves 15 pilot/save/settings files. The
  broader import/error-test baseline restores all 297 files with matching hashes.
- D1 and D2 scoped production resource tests pass against the exact candidate
  libraries, including explicit installation ownership and unlisted/detached
  source rejection. Universal APK assembly succeeds. JVM suites pass 61/62:
  localEnemyWithinAndMaximumKeepRealReplacementBankOwned returns null during
  archive import before the changed publication path; diagnosis remains open.

Earlier reduced import experiments omitted original smoke textures and were
not representative of the complete archive. A data-subdirectory layout also
resolved the HAM through an unlisted child mount; the final root layout fixes
that without broadening trust. Preserve those failures as diagnostic evidence,
not phone gameplay defects or passing route coverage.

## Boundaries and next check

Samsung hub acceptance belongs to version 29180. New version 29210 destination
transition, combat and quick save/load remain pending physical validation. Use
the bundle installer with `-ImportedMission`; installing only the APK cannot
supply the missing original addons. Prior fog, save-isolation and GuideBot tests
retain their original build identities. No full campaign is qualified.
