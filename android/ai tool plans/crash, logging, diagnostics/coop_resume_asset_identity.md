# Co-op resume object-link crash

## Evidence

- User reports host resume of D1-in-D2 level 8 after enabling presentation mods
- Build cc891569a, native SIGABRT in d2/main/object.c:1341
- Saved return addresses resolve with local Debug arm64 symbols to obj_link, state_relink_objects_by_index, and state_restore_all_sub_impl at state.c:3455
- Restore accepts versions 40, 42, and 44, but reads asset identity only for 40 and the platform's current version (44 on Android, 42 on host)
- Skipping this length-prefixed block makes its length become the object count and its hash bytes become object data
- Original save version is absent from supplied logs, so this is a confirmed reader defect consistent with the crash, not yet proof of the original save's contents

## Work

1. Enforce the user's current-format-only policy instead of adding compatibility readers
2. Share Android writer versions with launcher validation and reject obsolete formats before native world replacement
3. Test rejected saves, visible warnings, preserved state, and current-format loading
4. Record results and remaining limits

## Results

- Android now accepts only D1 version 20, D2 version 43, and D1-in-D2 version 44
- D1-in-D2 accepts only its current platform format; its obsolete version-40 reader condition was removed
- Native preflight rejects obsolete/future versions before mission or world replacement and displays an incompatibility warning
- Co-op launcher validation uses the same Android version constants, warns host and clients, and blocks starting with rejected saves
- Regression introduced by cb32b6a96 when Android moved from version 42 to 44
- Host checkpoint integration checks rejected earlier/later D1-in-D2 versions leave dynamic state unchanged and current saves still restore; passed
- Shared Android version-policy unit test passed for versions 0 through 45 in all three game modes
- Android Debug builds completed for arm64-v8a and x86_64; existing compiler warnings remain, with no new warnings from this change
- Emulator co-op integration passed: old/future versions rejected, warning advertised to clients, host launch blocked, current version accepted, Start fresh clears restriction, rejected save retained
- Emulator direct-resume integration passed for D1 and D2: visible incompatibility warning at level 0, one usable recovery menu, touch Quit exits without a crash
- Fixed the co-op test's activity component to support the isolated diagnostic package
- Scoped formatter/linter and both automation catalog checks passed
- An initial broader RuntimeState run stopped before the new version checks on differing effect records (55 versus 57). The D1 host executable was subsequently rebuilt; that broader variant was not rerun. The basic checkpoint integration and focused rejection checks passed
- Original co-op save and on-device retry are still needed to establish that this specific crash used version 42. Device validation uses an isolated diagnostic app
