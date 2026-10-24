# XL boundary cleanup

1. Trace the generated mod-save registry, verify its payload against HEAD, and
   document its production presentation-pack role. Do not create per-level XL
   registries or weaken save-source validation
2. Move XL corona drawing/depth diagnostics out of the stock OpenGL source into
   the existing XL runtime directory. Retain small guarded call sites and shared
   GL state bookkeeping; use ordinary target_sources, no generated engine sources
3. Build the opt-in host targets and Android APK, run lifecycle and Android
   rendering checks, and confirm the default-off path remains isolated
4. Explain the D1 boundary: this pilot extends the D2 loader/runtime. Shared
   save infrastructure still serves both games; no matching D1 XL port is needed

## Implementation and initial validation

- Extracted the XL halo draw and depth diagnostic into runtime/corona_renderer.c;
  normal target_sources adds it only to the enabled D2 arch_ogl target
- Kept alpha/depth-policy integration hooks in the stock renderer; D1 has no XL
  engine changes. Android continues building both D1 and D2 shared libraries
- Registry existed before XL (last original commit cb32b6a96). Its 28,836-character
  JSON payload is identical to HEAD; only comments and MSVC literal splitting
  differ. Regeneration preserves bytes and modification time when unchanged
- Opt-in Windows engine and lifecycle targets build; lifecycle passes in 2.29s
- Default-off Windows D2 builds and its build.ninja excludes corona_renderer.c
- Android x86_64 builds in 1m52s with no new compiler warnings
- Initial Android test never entered the game: three launcher readiness/start
  failures, Binder transaction failures and system-server delays. The runner's
  attempted SDK recovery also hit an outside-workspace lock permission error
- Preserved temp/xl-boundary-coronas.log. Requested an in-place reboot of only
  emulator-5554 after the test was confirmed terminal. No app data erased
- Post-boot retries still failed in launcher preflight, with system-wide ANRs;
  retained retry/ready logs. The elevated runner could access the actual AVD and
  perform a full emulator restart without erasing app data
- Rendering retest now passes 28/28 in 13017 ms, run
  aef424e243014005bab3d88b4d5a28a4. Runner exit code is 0. Evidence:
  temp/xl-boundary-coronas-elevated.log, temp/xl-boundary-coronas-result.json,
  temp/xl-boundary-coronas-object-report.json
- Actual object-depth probe retains 25 visible-before-object samples and 10
  object-occluded witnesses, same framebuffer, zero GL errors. Wall-depth,
  authored color and classic-depth preference checks also pass
- Retested APK SHA-256:
  9a2ed2e6d48c6d43e97d532cfb5b130778e24b12cda27529aa1d18daf90b3f19
- Stopped only the tested app on emulator-5554 after evidence capture. No D1
  source changes; no changes to the shared registry payload or save policy
