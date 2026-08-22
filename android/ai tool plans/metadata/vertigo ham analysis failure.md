# Vertigo HAM analysis failure

## Goal

Make Vertigo metadata analysis use the installed expansion data correctly when `d2x.hog` is present and `d2x.ham` is stored inside it rather than listed as a standalone launcher file.

## Plan

- [ ] Trace Vertigo discovery, staging, PHYSFS mount order, and HAM loading for gameplay and metadata workers.
- [ ] Reproduce the failure with the local Vertigo data or a focused fixture and confirm whether `d2x.ham` is embedded in `d2x.hog`.
- [ ] Fix the narrow shared staging or analyzer path without duplicating HAM parsing in Kotlin.
- [ ] Add regression coverage for Vertigo analysis from the supported installed-file layout.
- [ ] Run scoped quality checks, focused tests, and the Android build.

