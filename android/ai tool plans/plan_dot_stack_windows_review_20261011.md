# Dot stack adversarial review and Windows verification

- Review published PRs #5 through #14 at tip fd3fb408de946f799b36048ffca4191bd4ec0227 against stack merge base 062a4c0f467fedf956ba12f497a2e7842833846b
- Keep the user's dirty cmake checkout intact; work on review/dot-stack-windows in C:/local/dxx-redux-dot-review
- Inspect each PR's behavior, failure propagation, public fixture admission, automation coverage, and platform assumptions
- Build both games with Windows MSVC and assertions enabled, prepare pinned public installers, and execute added host checks plus real metadata, save, and replay integrations
- Exercise Windows-hosted Android changes where the available emulator allows, recording exact limits
- Reproduce findings before fixing, retain logs under temp/dot-review, and run scoped quality checks and relevant regressions after fixes
- Deliver a review with severity, PR and source locations, fixes, commands and results; do not merge or publish changes

## Completed

- Reviewed all ten PRs and retained GitHub metadata and local evidence
- Reproduced and fixed PR #14's missing POSIX producer lease
- Reproduced and fixed pre-existing MSVC metadata structure packing and Windows build configuration/library deployment defects
- Passed both Debug CTest suites, all public save integrity checks, the complete Windows ASan public profile, and compact native/oracle/paired gameplay gates
- Executed all 51 Android variants: 49 passed; both graphics-native-interruption variants failed identically on an independent rerun because the inherited pause policy blocks the key required by the older scenario
- Tooling/catalog/quality checks passed except SDK cleanup's unrelated duplicate key in an existing original-checkout AVD config
- Final report: `review_dot_stack_windows_20261011.md`; fixes remain local and uncommitted on the isolated review branch
