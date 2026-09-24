# SoundFont downloads

The curated list is `SoundfontCatalog.entries` in
`app/src/main/java/com/dxxredux/app/SoundfontCatalog.kt`. This is a Kotlin list,
with unavailable candidates kept as commented-out descriptors. It currently
enables GeneralUser GS 2.0.3 beta and records 19 further banks for future hosting
or compatibility/permission work. Opening the selector performs no network requests.

The active GeneralUser asset is hosted in Codetta's `soundfont-bundle` GitHub
release. Its original-source link points to S. Christian Collins' repository;
the full GeneralUser license and its sample-provenance caveat remain available
in both confirmation and saved Info. The SF2's internal name includes BETA,
so the app labels it accordingly. The downloaded file is 32,319,396 bytes and
its checked SHA-256 is
`9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe`.
This is a verification record, not a runtime checksum enforcement mechanism.

FluidR3 GM and OPL-3 FM 128M have release assets, but remain commented because
they exceed the 64 MiB limit. Other comments distinguish future GitHub hosting
from additional license, decompression, SF3 conversion or size prerequisites.
Commented URLs are research leads, not enabled downloads; replace them with
tested direct GitHub release SF2 URLs before enabling. Do not mistake a mirror's
repository license or a free-download label for the bank's redistribution rights.

Populate the list with descriptors of this shape (illustrative only):

```kotlin
SoundfontDownload(
    name = "Bank name and version",
    url = "https://github.com/OWNER/REPOSITORY/releases/download/TAG/bank.sf2",
    description = "Short description of this bank's sound",
    websiteUrl = "https://github.com/ORIGINAL-AUTHOR/ORIGINAL-PROJECT",
    license = "Full license and required attribution text",
)
```

The selection dialog leads to a Download? confirmation showing the description,
download URL, clickable original website/readme link, and selectable license
text. No asset request starts until confirmation. Keep the original-source URL
separate from the release-asset hosting URL. Include required attribution in
the license field; this dialog does not establish redistribution permission.

Use HTTPS direct SF2 assets. Same-scheme HTTPS redirects (including GitHub
Releases) are followed, but HTTPS-to-HTTP downgrades are not. ZIP, SF3, HTML
release pages, authentication-required assets and banks over 64 MiB are not
supported by this flow. The existing native SF2 validator still applies.

Downloads stream on OkHttp workers into SoundfontStore's private temporary
files. The app checks available storage, declared size, actual byte count and
native validity before publication. Unknown content lengths still have the
64 MiB streaming limit. Errors/cancellation close the response and remove
partial files. Completed imports are deduplicated by content hash, then
activated through MidiPreviewBridge and saved to game preferences. The
renderer choice remains unchanged. If activation fails, the previous choice
remains active and the imported asset remains available for retry.

Completed downloads also save their descriptor in the soundfont manifest.
The bundled TimGM6mb bank is named in the selector and has a permanent Info
entry in both **Manage soundfonts** and **Download soundfonts**. Its details
include the author, version, size, source release URL, repository link and full
offline GPL-2 license from `assets/licenses/TimGM6mb.txt`. It has no download or
delete action and is not stored in the removable-font manifest. Closing Info
returns to the list that opened it. This labels the existing bank; it does not
replace it or change its license.

**Manage soundfonts** offers **Info** and **Delete** for each downloaded or
locally imported bank. Info shows the saved description, asset URL, clickable
original website/readme and license, even after restarting the app or removing
the entry from the download catalog. Reimporting the same file locally preserves
its metadata; downloading a previously imported file attaches its metadata.
Local files without metadata are identified as local imports.

Delete requires confirmation and removes the bank from device storage. Deleting
the selected bank first switches to the bundled soundfont and stops its MIDI
preview, preserving the FM/Soundfont renderer preference. Inactive banks can be
removed without restarting playback. The bundled asset cannot be deleted.

Cancellation follows the selector's coroutine lifetime; leaving the page also
cancels the operation. This is not a background/resumable download service.
An import that finished just before cancellation can remain in the asset list,
without being activated. Slow stream reads do not hold the preference lock.

Validation:

```powershell
$env:JAVA_HOME='C:/local/jdk-21'
./android/gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.SoundfontDownloadTest --tests com.dxxredux.app.SoundfontStoreTest :app:assembleDebug
./android/helpers/run_test.ps1 -ScriptName test_soundfont_download_catalog.jsonc -Game d1
python android/tests/test_soundfont_profiles.py --library --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/soundfont-library/device
python android/tests/test_soundfont_release_download.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/soundfont-catalog/device
```

JVM integration uses synthetic HTTP responses through OkHttp and the real
SoundfontStore: successful import/selection/persistence, duplicate bytes, HTTP
errors, invalid content, incomplete responses, advertised/streaming size limits,
and cancellation during a blocked read. Store coverage also verifies retained
metadata, both settings resets, active/inactive deletion, renderer retention,
and refusal to delete when fallback activation fails. The catalog emulator
script checks the populated catalog, source link and confirmation cancellation
without network transfer. The release-download runner performs the actual HTTPS
download through the UI, checks its content hash and saved metadata, reopens
Info, and plays MIDI with the downloaded bank. It restores the original
soundfont manifest and MIDI preferences; like the shared launcher runner, it
resets other emulator game fixtures. Its command-line options select future
catalog releases by name, hash, download URL and original website.
The profiles runner's `--library` option uses a native-valid
fixture and saved test metadata to exercise Info, cancel, confirmed deletion,
file removal and persistence across restart; it restores prior assets/settings.
Dialog automation uses public WindowInspector on API 29+ to inspect and click
the focused dialog window rather than the covered activity.
Additional release entries should be checked against their actual hosted assets
and confirmation text before shipping.
