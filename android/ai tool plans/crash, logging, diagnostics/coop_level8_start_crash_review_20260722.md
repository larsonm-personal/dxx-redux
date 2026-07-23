# Coop level 8 start crash review

## Scope

Study only. Analyze the supplied build 18650 crash stub and breadcrumbs against
the source at commit `b609c954`, without runtime source changes.

## Work Plan

- [x] Read repository instructions and preserve the dirty worktree.
- [x] Classify the xCrash status-102 report.
- [x] Map the last breadcrumbs to the level-sync, graphics, and TSF paths.
- [x] Inspect code immediately after successful D2 co-op level sync.
- [x] Rank supported crash hypotheses and identify missing evidence.
- [x] Recommend focused instrumentation for the next reproduction.

## Notes

- The supplied report has no signal, fault address, crashing thread, register
  state, or native backtrace.
- Existing unrelated worktree changes are out of scope.

## Findings

- xCrash 3.1.0 returns `100 + errno` when `execl()` of its dumper fails.
  Status 102 therefore means `ENOENT`, not that the game exited with code 102.
- The APK contains `libxcrash_dumper.so`, but its merged manifest has
  `android:extractNativeLibs="false"`. xCrash expects an executable filesystem
  path under `ApplicationInfo.nativeLibraryDir`; this packaging mode is
  incompatible with that assumption and explains the missing tombstone.
- The graphics swap returned before the later network breadcrumbs. All sampled
  TSF synth calls completed and all sampled callbacks obtained their requested
  frames. Neither subsystem shows a failure in the supplied window.
- D2 co-op level sync returned `result=0` while `Network_status=0`.
  `NETSTAT_MENU` is 0 and `NETSTAT_PLAYING` is 1, so this is not a valid
  successful sync state.
- The host path calls `net_udp_read_sync_packet()` locally, but
  `net_udp_send_sync()` returns 0 without checking whether that function
  rejected the sync. Rejection can leave `Network_status=NETSTAT_MENU` because
  of a checksum mismatch, duplicate self match, or failure to find the local
  callsign.
- In the local-player-not-found case, `Player_num` is left at -1. The outer
  level loader treats the sync as successful and later calls `StartLevel()`.
  Its co-op path uses `Player_num` as the `Player_init` index and asserts that
  it is nonnegative. This is a direct, source-supported crash path inside the
  breadcrumb gap.

## Next Evidence

- Export the same timestamp's `crash_error_emergency_*.txt` or
  `crash_error_degraded_*.txt`, if present. It may contain xCrash's emergency
  signal and fallback stack.
- Export the Network and Game Logs files. Look for `read_sync_packet` lines,
  the local player number, a level checksum mismatch, and the full
  `level_sync result` line.
- Add reason-specific breadcrumbs before every rejected sync return and include
  `Player_num` in the final sync breadcrumb.

## Fix Direction

- Make `net_udp_read_sync_packet()` report success or failure and propagate the
  local read failure from `net_udp_send_sync()`.
- At minimum, reject sync unless `Network_status == NETSTAT_PLAYING` and the
  local player identity is valid before continuing into `StartLevel()`.
- Package native libraries in legacy/extracted mode for xCrash's executable
  dumper, or replace/update the crash capture mechanism for current Android.

## Implementation Tranche

- [x] Enable extracted native-library packaging so xCrash can execute its
  dumper on current Android.
- [x] Add reason-specific D1/D2 sync rejection breadcrumbs with safe identity
  and checksum details.
- [x] Add D1/D2 post-sync level-initialization stage breadcrumbs around the
  previously unobservable gap.
- [x] Make host sync propagate local sync-processing failure instead of
  continuing with `NETSTAT_MENU` or an invalid player.
- [x] Add focused native-crash coverage and compile the D1/D2 network changes
  on Android and Windows.
- [x] Run scoped code quality, focused tests, and proportional builds.

## Implementation Validation

- Debug APK and JVM unit tests passed.
- Internal APK build passed for all configured ABIs.
- D1 and D2 Windows build passed.
- Both debug and internal APK manifests contain
  `android:extractNativeLibs="true"`.
- The focused emulator test installed the debug APK, verified the extracted
  xCrash dumper, signal-crashed the app, and confirmed a native report with
  `SIGSEGV` and `backtrace:` and without status 102.
- Scoped code quality and `git diff --check` passed. Existing compiler warnings
  were unchanged and outside this work.
