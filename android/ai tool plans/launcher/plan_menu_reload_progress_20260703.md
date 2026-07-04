# Menu reload progress plan

## Goal
Make launcher submenu reloads show the same spinner and progress indicators whether the user enters a deeper menu, backs up to a menu that reloads, or resumes the app to an existing page that reloads.

## Steps
- [x] Create this plan.
- [x] Trace page/submenu reload paths that perform deeper work.
- [x] Identify loading states that are hidden when cached data is still present.
- [x] Update reload logic to expose progress on entry, back navigation, and app resume.
- [x] Add or update focused tests for the shared loading-state behavior where practical.
- [x] Run scoped code quality and relevant Android unit tests.

## Notes
- Keep unrelated local work untouched.
- `produceState(initialValue = null, key)` retains the previous state value when the key changes, so refresh-triggered reloads need explicit null/loading states.
- Reused the existing `MetadataLoadProgress` formatter test coverage while moving the progress view into the shared metadata progress file.
- Validation: scoped `android\run-code-quality.ps1 -Fix -Paths ...` passed; targeted
  `:app:testDebugUnitTest --tests com.dxxredux.app.MetadataLoadProgressTest` passed.
