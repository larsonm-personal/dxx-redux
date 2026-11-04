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

## Combined original Pumo UI qualification

The optional `test_xl_addon_import.ps1 -PumoUiImport` mode imports the unchanged
442181363-byte `pmines_v11.7z` and original raw XL camera/effects through the
production general-picker callback. It then launches the canonical 36-step hub
probe using the imported mission owner. The host protects the whole default
retail set, preferences, native profiles and mission mounts and verifies every
restored file hash. Instrumentation supplies document URIs; system-picker file
browsing remains outside this test.

Two setup failures are retained separately: the generic launcher selected the
default set instead of the first cloned test set, and a later default-set launch
ran out of space while copying the full model view. On this Android image,
SELinux denies hard links, so mission publication uses its existing private-copy
fallback. The launcher now checks the existing 50 MiB free-space reserve before
each fallback copy, retaining the resource name in the error and transactional
temporary-directory cleanup. It does not weaken ownership or use mutable import
cache paths as native mounts.

The protected runner streams its verified backup to the host before mutation
and removes only its hash-verified picker input after successful import. Original
host resources and the imported private archive remain intact. These changes
remove test overhead from the small disposable device volume; users still need
space for the original archive, extracted cache and private native resource view.
The earlier staging/import failures are not credited as gameplay passes.

The UI-imported mission on universal 29440 completes all 36 gameplay steps in
113933 ms, run `cb76cdf1ca0a48a082ea76bd72dca748`. Native preparation takes 59
seconds on the owned software emulator; its existing software-startup budget is
required to avoid a premature 30-second harness restart. Native state retains
`mission_asset_owner = mod/pmines_v11.7z`, generation 1. `mounted_mods` is empty
because it reports legacy mounts, not this catalog view. The original host
postcondition incorrectly required that legacy list, so the wrapper reports a
harness failure after the native PASS. Its corrected postcondition checks the
exact catalog owner/generation. All protected restoration hashes match. Evidence
is retained in `temp/xl-overnight-20261103/pumo-ui-native-pass-29440`; qualification
of the corrected complete wrapper on a committed candidate remains required.

## Full-model opening gates and catalog-save follow-up

Exact universal29450 passes original PMC1 opening unlock/open-wall shots, three
normal gate crossings and persistence:76 warm steps in89299ms,15 fresh-process
steps in24301ms. Both use full original Pumo models, no Vertigo, normal hub entry
and fixed local approaches; no full human traversal is claimed. Protected hashes
match. The old opening-gates output was preserved under
`temp/xl-overnight-20261103/prior-opening-gates-20261102` before the run. The new
owned621196066-byte transfer was removed after recording SHA and restoration;
all original resources remain. Current evidence is temp/xl-original-pmc-opening-gates.

A staged-engine cold load does not establish imported-catalog source ownership.
The existing addon UI owner therefore gains optional `-PumoUiImport -PumoUiCold`:
it imports the unchanged original resources, runs the same76-step opening probe,
then starts a fresh game process and loads the15-step checkpoint through the
normal menu. It requires the exact imported catalog owner before and after the
restart, retains both PIDs/results/renders, and restores the whole protected
profile/set. The default public and36-step UI modes remain available.

The first UI-catalog run on29450 passes76 warm steps in110203ms and15 cold
steps in29527ms, PIDs30948 ->31873. Both retain catalog owner
`mod/pmines_v11.7z`, with exact protected before/after hashes. The cold menu load
restores the opened/unlocked gate state and crosses the gate normally. Evidence:
`temp/xl-overnight-20261103/ui-cold-29450-pass`. The next committed candidate must
repeat this full owner to verify its own APK/source IDs. This is local approach,
gate and model-bearing save qualification, not full campaign or phone validation.

## Exact29460 and mission-toggle29470 checkpoints

Universal29460 at `b939f1494fc1de31e7397e082c299a9e0b254e0d` repeats the combined
UI/catalog PMC opening gate:76warm/15cold PASS, PID2001 ->2806, owner
`mod/pmines_v11.7z`, with protected restoration. Its independent D1 lifecycle
passes62steps. Retained [29460 bundle](../../../temp/pumo-owned-model-playtest-20261103-29460/README.md)
and full receipts remain separate from later candidates.

Android29460 exposed a launcher control defect: original Pumo imports enabled,
but disabling then re-enabling its7z archive fails with a ZIP-opening exception.
`ModManager.setEnabled` used the ZIP-only DXA scanner for mission archives.
Mission-kind enablement now follows import's bounded, format-aware inspection
and verified extraction-cache reuse. Resource/DXA admission and native guards
remain unchanged. Targeted JVM controls pass15tests with1optional resource
witness skipped. Public Android checks also reject a damaged synthetic7z after
reload; that fixture establishes admission only, not gameplay.

Exact universal29470, source `7d2899bf816d16f5d59e724ab3d3f144a46211a1`, passes
original Pumo import and production disable/re-enable. Its imported-catalog
stock/Pumo save isolation passes68warm steps in83025ms, run
`0c50bda16b104f1a8d8dbd79d0bb0823`, and39cold steps in59959ms, run
`75bcf0c35be44ee3a52bc1a7631768f6`. PID9734 ->10424; explicit Pumo/base owners
are checked at each runtime boundary and the final owner isbase. The protection
manifests both hash to `fef76174b8040ff8e408a7ecdd39896c79a18a5f6d5f673cf29709c73177faf2`.
Exact D1 lifecycle62steps PASS, run `ac88c99e8e4942788809ebe8cbc60ddc`, with47
protected files restored. Renders inspected. New owner-result attempt IDs and
native run-ID correlation distinguish failed attempts from older PASS artifacts.

The [29470 bundle](../../../temp/pumo-mission-toggle-playtest-20261103-29470/README.md)
records all six native IDs, APK SHA, exact stack heads and CI snapshot. No phone,
system Files browsing, full campaign or ordinary full-model robot combat claim
follows. Disabled-owner save rejection/recovery is a separate next gate; positive
isolation and successful re-enabling do not establish it.

## Reusable disabled-owner recovery gate

The existing protected UI owner now supports `-PumoUiImport -PumoUiDisabledOwner`.
It derives a29-step seed from the first ordinary stock manual-save boundary of
the isolation scenario. Two registered support scripts exercise normal launcher
Load Last Save while Pumo is disabled, then after production re-enable. Both
require fresh complete run IDs and compare the stock bank before full rollback.
No live-stock-world preservation claim follows from this cold rejection case.

On unchanged exact APK29470, the full owner passes with attempt
`1d616ecd90be48cb95d6ce43a27d3847`: seed `b000dc28adfb410999b664526d5aff65`,
negative26 `5626ee67cf554d1a84e247e780624bf4`, recovery9
`8c3880b31d6e42308885c003c9a468f9`. Error: Unable to load mission 'pmines',
in_game=false, owner=base, fitting menu. Normal recovery returns alive to Temporal
Hub, owner=mod/pmines_v11.7z,99.5energy, no cheats. Renders inspected. The reported
272/712ms describe only final native phases, not the complete launcher workflow.
Stock-bank manifest SHA256 is unchanged across all three snapshots:
`9d39b1f8b855704722b5d6859cf845dacdcb63c87373daedeb0cdc7ab9122567`.
Protected whole-profile manifests both hash to
`fef76174b8040ff8e408a7ecdd39896c79a18a5f6d5f673cf29709c73177faf2`.
Receipts: temp/xl-pumo-ui-disabled-android.

The first attempt stopped before native startup at the intact50MiB storage guard
while publishing original resources. Protected restoration succeeded; retained
failure evidence is temp/xl-disabled-owner-20261103/attempt-1-storage. After moving
one hash-verified old owned-emulator transfer archive to the host and removing
only its device duplicate, the retry passed without changing any guard/assets.
This remains emulator evidence, not phone, system7z browsing or campaign proof.
The next committed current-head APK must verify its own UI change and exact IDs.
