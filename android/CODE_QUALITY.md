# Formatting and lint

Run from the repository root with PowerShell 7 (Windows PowerShell 5.1 is also supported):

```powershell
.\android\run-code-quality.ps1 -List
.\android\run-code-quality.ps1 -Fix
.\android\run-code-quality.ps1
.\android\run-code-quality.ps1 -Fix -Changed
.\android\run-code-quality.ps1 -Fix -Paths android\helpers android\tests\example.py
.\android\run-code-quality.ps1 -Tools ruff,prettier
```

The default is a check. `-Fix` formats files, and lint-only tools still report issues. `-List` previews eligible files without loading tools. `-Changed` selects working-tree edits, staged edits and untracked additions. `-Tools` selects stages for a shorter rerun; omit it for all stages. Every helper also supports `-Paths`, and formatting helpers accept `-Check` for a dry run.

Only files added since the merge base of HEAD and the upstream baseline, including staged and untracked additions, are eligible. Inherited files stay excluded even with an explicit `-Paths` argument. The baseline is the first locally available ref among `upstream/main`, `main`, and `origin/main`. Override it with `-BaseRef <ref>` or `DXX_CODE_QUALITY_BASE_REF` for direct helpers. Missing or invalid baselines fail instead of widening the scope. Fetch enough history and the upstream ref when using a shallow clone.

All files under `d1/` and `d2/`, upstream SDL patch copies, build/dependency directories and generated `BuildInfo.kt` remain excluded. Generated mission metadata JSON under `game_data/` retains the producer's canonical formatting. JSONC regression scripts remain eligible.

| Files                                                                  | Tool                                                              |
| ---------------------------------------------------------------------- | ----------------------------------------------------------------- |
| C/C++ headers and sources, Java, C#, `.csrc`                           | clang-format                                                      |
| Kotlin sources and scripts, including JVM/device tests and CLI modules | ktlint                                                            |
| PowerShell scripts, modules and data files                             | PSScriptAnalyzer                                                  |
| Bash scripts throughout the eligible tree, including server scripts    | shfmt and shellcheck                                              |
| New CMakeLists.txt and `.cmake` files                                  | cmake-format and cmake-lint                                       |
| Python                                                                 | Ruff formatter                                                    |
| Rust                                                                   | Pinned rustfmt, without following child modules outside the scope |
| JSON/JSONC, YAML, Markdown, HTML, XML/SVG, JS/TS and CSS               | Prettier and its XML plugin                                       |

Markdown keeps existing prose wrapping and code-fence contents. XML/HTML formatting treats text whitespace as significant. These choices preserve handmade comments, snippets and resource text. Groovy Gradle files, Cargo TOML, batch wrappers, properties/config files and disc CUE descriptors currently receive BOM checks only; the runner does not claim syntax formatting for them.

Existing native/Kotlin/Bash/CMake tools use the installers in `android/get_deps/helpers/`; PSScriptAnalyzer installs its pinned module on demand. To install the additional tools, put Node.js 20+, npm, Python 3.11+ and rustup in PATH, then run:

```powershell
.\android\run-code-quality.ps1 -InstallTools
```

This uses `npm ci` with the checked-in lockfile, installs pinned Ruff under ignored `android/tools/code-quality/python`, and installs the exact Rust toolchain without changing rustup's default. It records the resolved Node path in `android/tools/code-quality/node-path.txt` so later runs can reuse that installation without changing PATH. Keep Node itself outside disposable scratch directories. Python/Rust pins live in `get_deps/tool_versions.conf`; Prettier pins live in `tools/code-quality/package.json` and its lockfile. Installed formatters and the Node path cache survive temporary cleanup; only per-run manifests and status files belong under `android/temp`.

Wait for formatting to finish before editing files or starting builds. The runner records its active stage and final summary under `android/temp/run-code-quality.*.json`. For interrupted runs, see [CLEANUP.md](CLEANUP.md) and preview matching processes with `helpers/stop-stale-formatters.ps1` before using `-Kill`.

Run `android/tests/test_code_quality_files.ps1` to verify selection and mixed-language dispatch in an isolated Git repository without installed formatters. Run `android/tests/test_formatter_process_cleanup.ps1` to verify interrupted-run cleanup.
