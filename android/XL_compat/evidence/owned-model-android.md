# Original Pumo owned-model Android qualification

November 3, 2026: committed source
`fbb6257188b4326e33b1b4d9b49cdf51650b6479`, universal version 29430,
application `com.dxxredux.app`. All six D1/D2 arm64/armv7/x86_64 native IDs match.
APK SHA256 `2669624d47a2c425917fbd480008fdc6fe6969d0eea7504c9b8ee08df8365683`.
No mission, addon or proprietary model resources are bundled.

## Actual engine behavior

The protected original hub probe uses full Pumo HAM/HXM/VHAM/ASE/OOF resources,
original camera HOG and XL effects, without Vertigo. Logs identify Pumo's own
definitions with 78 robot and 178 model slots. Normal laser/thrust activation
reaches PMC level 11, restores the hub save at segment 65 with unfired triggers,
then repeats the transition: **36/36 PASS**, 73906 ms. Approaches use fixed poses;
this does not establish full human traversal or campaign completion.

Separate stock/Pumo save-menu isolation passes **68 warm / 39 fresh-process**
steps, in 76460 / 39475 ms. The game process changes from 7874 to 8653. Both
missions restore their own runtime and player state. Protected before/after
manifest SHA256 matches:
`160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`.
The installed APK hash matches and all 13 protected installation files survived.

This closes the earlier first-fire GLES texture-state failure on the owned
Android 14 x86_64 emulator. The pre-rebase diagnostic version 29420 also passed
36 steps, but remains separate historical causal evidence. No Samsung or other
physical-phone validation is claimed.

## Actual general-import UI

The `xl_addon_import` owner now drives the production general import button and
its document-picker callback with a disposable file set. The test supplies the
picker result through instrumentation; it does not automate the system picker's
file browsing. Both the public synthetic fixture and original user-owned XL7z
pass. The latter is 1607764603 bytes, SHA256
`2cfc020fbf25a048f099511b71599c7be1b77836be8fc1112e5eb310c65beb9d`.

The normal callback selectively imports and enables XL resources, verifies the
camera HOG SHA256
`e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6`,
smoking-fire PNG and lightning waveform, and confirms no Vertigo HAM is supplied.
The visible "Imported mod: XL resources" screenshot was inspected. Preferences
and file-set configuration restore byte-for-byte; the disposable set is removed.
The existing JVM witness independently extracts supported resources from the
same archive in 21.07 seconds. This is not phone timing.

Public controls retain raw, ZIP, seekable/streaming 7z, tracker and progress-bar
coverage. Full original Pumo import plus launch through the UI remains a separate
gate; staged engine success does not substitute for that combined workflow.

## Harness changes and evidence

Full original model staging produced a 1479604736-byte tar and exhausted the
host output reserve. The protected runner now compresses that transfer to
621196066 bytes. The full save-isolation run validates extraction and restoration.
The addon UI owner backs up preferences/configuration and retains logcat even on
failure. UI qualification also required fresh activity tasks, the matching picker
MIME filter, recursive Compose traversal and bidirectional scrolling. No engine
admission guard or mission content was changed for a pass.

Evidence: `temp/xl-overnight-20261103/current-hub-29430`,
`temp/xl-original-save-isolation`, `candidate-identity.json`,
`current-install-proof.json`, and `picker-original-pass` under the overnight
directory. Failed harness attempts remain retained and are not engine defects.
See [rendered GuideBot](rendered-guidebot.md) for the full-model route's correctly
unsupported companion activation and separate ordinary-player evidence.
