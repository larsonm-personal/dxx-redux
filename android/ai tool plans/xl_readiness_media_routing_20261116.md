# Final CI media routing investigation

PR540 rebased candidate19f06a05766ca70f052d42c33694059116c1cce6 fails its first
MIDI pause in workflow43927543361 attempt1. The other four selected Android
owners pass. Preserve artifact14262376410 and its failed result. Setup and
provisioning both equal1. The platform adds the preview session at19:13:22.763,
updates playback at22.829 and selects it for media buttons at22.878. No routed
key event appears; post-failure removal at27.226 is normal test cleanup.

The current start assertion observes native PLAYING and duration only. Android
chooses its global media-button target asynchronously. Investigate that missing
test precondition using the exact platform session token on API33+. Keep the
existing global AudioManager key dispatch, every transport assertion, the
40-by-100ms start/assertion polling limits and the180-second owner deadline.
Do not add a fixed sleep, retry a key, dispatch directly to a session or modify
production playback behavior. Historical failure causation remains an inference
until direct local observation confirms the ordering gap.

Use the existing instrumentation runner for privileged read-only observation,
with only MEDIA_CONTENT_CONTROL adopted around each routing query and dropped
before sending a key. Earlier Android versions retain the original script path.
Record initial routing readiness and the transition for MIDI, CD and MP3. Retain
ordinary launcher entry and the original native/file transports. The production
session object remains unchanged; test instrumentation can inspect its token.

Validate the observed precondition and complete unchanged controls locally with
protected APK/profile restoration. Run scoped formatting and both automation
catalog checks, then publish a coherent immediate-parent PR above540. Inspect
its own CI and keep all old failures visible. Remain within the current tranche's
validation/repair reservation ending22:03UTC; do not start another tranche.

Platform references:

- https://developer.android.com/reference/android/media/session/MediaSessionManager#getMediaKeyEventSession()
- https://developer.android.com/reference/android/app/UiAutomation#adoptShellPermissionIdentity(java.lang.String...)
- https://android.googlesource.com/platform/frameworks/base/+/31b731c2de27/services/core/java/com/android/server/media/MediaSessionService.java

## Local qualification

The ordinary x86_64 app and instrumentation build passes with zero compiler
warnings. Frozen app SHA256 is
8c1ebcb2ab8e564dca95796fdb2b6cbe638bba12f7a34ea320ceab886936c800;
instrumentation SHA256 is
99f759ebf9250359b994ed7e3b9977df73a4dd1f2f5183483ad391ec1a3d74c1.
media-routing-build-01/receipt.json identifies the uncommitted source bytes
built above parent19f06a057. android-media-routing-01 passes rejected requests
and all unchanged MIDI/CD/MP3 control assertions. Each first observed token was
already selected locally, so this does not reproduce the hypothesized timing
gap. It proves the read-only precondition and global key path operate together;
historical causation remains an inference. No key retry or direct controller
dispatch was introduced. Profile hashes, original APKs and preferences restore
exactly. The maintained owner remains within its180-second limit.

Automation catalog validation passes99standalone JSON,549support scripts and
315PowerShell owners; master catalog integration passes428entries. API33+
requires the existing test APK, now declared by the maintained owner's catalog.
Earlier Android versions retain the original launcher-script path and are not
newly runtime-qualified by this API34 run. Final candidate hosted CI is required.

## Current-base replacement

Apply only the final media-routing commit 1465ad8ed from #544 onto current cmake
eba98b696. Its diagnostics and selection prerequisites are already present via
#536 and #540. This avoids republishing unrelated historical XL stack changes.
No branch merge or history rewrite is performed; #544 remains open for coordination.
The local qualification above belongs to the original candidate. Current-base
compile, catalog and hosted results must be recorded separately; no fresh device
qualification is implied by forward-porting the patch.

Current-base verification: app and Android-test Kotlin compile successfully with
the pinned JDK/NDK/Gradle toolchain. Scoped formatting, automation catalog and
20 stack-policy tests pass. The master catalog validator cannot run in this
container because reading /proc/self/task/<pid>/children is denied (exit125).
No fresh emulator/device run: KVM is unavailable and adb cannot create its default
user directory. Existing original-candidate device results are not transferred.
