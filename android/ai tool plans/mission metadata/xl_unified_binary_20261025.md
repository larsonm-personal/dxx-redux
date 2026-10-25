# Unified stock and XL binary

Include supported XL functionality in normal OpenGL D2 builds, with runtime
feature/resource admission and stock isolation. Remove the xlPilot Gradle switch
and DXX_XL_PILOT CMake option. DXX_XL_RUNTIME is an internal renderer-capability
definition, not a user-selectable build variant. Preserve software rendering,
which cannot currently render XL and must continue rejecting it explicitly.

Separate private diagnostics with DXX_XL_DEVELOPER_TESTS, default OFF. Keep normal
desktop and metadata executable names and entry points. Preserve existing D1
behavior, Android introspection diagnostics and bounded experimental profiles.

Verification: default Windows game and metadata builds, private lifecycle harness
with the diagnostic option, normal Android APK without xlPilot, original Konflict
startup/render/save/stock transition, focused build/workflow contracts, and scoped
formatting. Preserve the previous tranche's uncommitted work. No navigation or
combat playthrough attempts. Update current documentation without rewriting
historical evidence claims.

Progress: complete. Normal OpenGL D2 builds include the supported XL runtime;
the optional private diagnostic harness remains separate. Runtime feature and
resource admission still reject unsupported missions. Existing uncommitted
Konflict tranche changes were preserved. No commits were created.

Validation artifacts: `temp/xl-unified-20261025/`.

- Default Windows game and normal metadata targets built successfully, preserving
  `d2x-redux.exe` and `dxx-redux-d2-headless-metadata.exe`. Normal metadata extracted
  both stock Ahayweh Gate and XL Konflict successfully.
- The private lifecycle harness built with `DXX_XL_DEVELOPER_TESTS=ON`; Konflict
  feature and Observatory rejection tests passed (2/2).
- A fresh `OPENGL=OFF` Windows game build passed, without XL runtime sources.
  Software rendering remains stock-only and reports this limitation explicitly.
- Normal Android debug APK built without any XL property. On emulator-5554 the
  controlled Konflict integration passed 33/33 steps, including rendering,
  quicksave/restore, XL indicator, stock transition and authored lighting checks.
  No navigation or combat playthrough was attempted. Durable result:
  `temp/xl-konflict-android/result.json`, run `739a9b0d6a3740a4897112b5ab66dc43`.
  Tested APK SHA-256:
  `ab214222ba38c3c6bbc0153b0b5e672a142a9783372a6b3764dc33564959605d`.
  A subsequent compatibility-browser wording-only change was rebuilt successfully;
  the device integration result applies to the preceding APK.
- Standalone inventory contract and the updated nightly workflow selection test
  passed. The broader workflow suite encountered an unrelated Windows symlink
  privilege error (`WinError 1314`); that suite is not claimed as fully passing.
- Scoped mixed-language formatting/lint and `git diff --check` passed.

These checks establish unified build selection and bounded runtime behavior, not
universal D2X-XL compatibility or a completed mission playthrough.
