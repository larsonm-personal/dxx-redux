# Headless Dump Tool Split

## Goal
Determine whether the secret-area dump entrypoint has become a general headless metadata tool, then split or rename it so unrelated dump modes do not accumulate in a secret-area-named file.

## Steps
- [x] Inspect build targets and script callers for the current secret-area dump executable.
- [x] Classify the existing modes in `headless_metadata_dump_main.cpp` and identify reusable harness code.
- [x] Rename the source, CMake targets, executable defaults, and docs to headless metadata naming.
- [x] Run focused build and quality checks.

## Notes
- The CMake targets and executable defaults are now `dxx-redux-d1-headless-metadata` and `dxx-redux-d2-headless-metadata`.
- The source file is now `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`, since it contains both `-secretarea-json-out` and `-coop-starts-json-out`.
- A larger follow-up can split this into shared headless runtime helpers plus separate secret-area and coop-start mains if more modes accumulate.
