# Fix Malformed Unicode Properties Load

## Goal
Fix the Android build failure caused by Java/Groovy properties loading of android/get_deps/tool_versions.conf, then rerun the failing build entry to verify it starts cleanly.

## Status
- [x] Confirm the exact properties loader and offending config entry
- [x] Patch the smallest safe fix for Java properties escaping
- [x] Re-run the failing build entry and confirm the malformed unicode error is gone

## Notes
- settings.gradle and app/build.gradle both use Java Properties.load() on get_deps/tool_versions.conf
- UNAR_INSTALLED_VERSION_CMD introduced a literal \u sequence via $dependency_base\unar\unar.exe, which Properties.load() treated as a malformed unicode escape
- TARGET_SDK and MIN_SDK also needed inline comments moved onto separate lines because Properties.load() keeps trailing # text as part of the value
- After both fixes, bash ./build.sh completed successfully
