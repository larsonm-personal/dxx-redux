# Pull request checks

`Engine smoke` runs on every PR and can also be dispatched manually. Two
independent Ubuntu 24.04 jobs build D1 and D2 in parallel. Each job has a
20-minute ceiling, including dependency installation. Superseded runs cancel
automatically. There is no second copy triggered by pushing the PR branch

The normal CMake `all` target compiles and links the desktop game, headless
tools, metadata workers and native tests. OpenGL, SDL_mixer and UDP stay enabled.
Debug builds keep assertions active and use reduced debug information to limit
memory and disk use. No warm cache is required

Every registered CTest runs with dummy SDL audio/video drivers. This includes
asset-free engine integration tests (AI, physics, weapons, triggers and save
state), input-demo recording/replay component tests and D2's synthetic classic
trigger demo writer/reader round trip. Tests run serially within each game
because some fixtures share filenames. Each test has a default 120-second
timeout, the combined configure/build/test step has a 17-minute limit, and zero
discovered tests is an error. CTest logs/JUnit and configuration diagnostics are retained for three
days, including on failures. Compiler output stays in the Actions job log

This is Linux native coverage. It does not compile Android Kotlin/JNI-only
paths, exercise a GPU/display, run an emulator, package an APK, or validate other
desktop compilers. Keep the manual packaging workflows for those build paths

## Tooling checks

Keep the existing Linux and Windows tooling suites, including the Windows
PowerShell 5.1 release tests. They exercise download verification, extraction,
dependency installation, cleanup/process lifetime, release operations, replay
comparison and test-catalog/evidence handling. Catalog validation is only part
of their coverage; it does not claim to execute the catalog's game tests

The workflow now runs on PRs that change scripts, automation fixtures,
dependency/extraction infrastructure, workflows, or the asset-loader contracts
those tests inspect. Ordinary engine and launcher source edits no longer
trigger it. Manual dispatch remains available; duplicate push runs are removed

For branch protection, require the two `Linux d1/d2 build and tests` checks.
Keep path-filtered tooling checks optional, since an unrelated PR will not
start that workflow. This change does not modify repository protection settings

## Full demo replays

The `.dximdemo` corpus and the required retail game files are not tracked in
Git. A clean hosted checkout therefore cannot run the existing full-game
regression runner. The smoke job executes the available asset-free demo checks
and does not silently skip a promised gameplay replay

To add a full-world PR replay later, commit a small validated recording and
provide the matching, hash-verified redistributable media fixture. Another
option is a separate trusted/manual job with privately provisioned retail
media. Keep asset access out of ordinary fork PRs and retain the existing
recorded-state comparison; repeatability alone is not recording fidelity

## Local reproduction on Ubuntu 24.04

Install the packages listed in `engine-smoke.yml`, then run from the repo root:

```sh
./run-linux-build.sh --test
# One CI matrix job, using the same four parallel build jobs
./run-linux-build.sh --test --target d1 --jobs 4 --generator Ninja
```

CI and local tests share this runner and the registered CTest list. Test mode
uses `buildd1-test` and `buildd2-test`, `BUILD_TESTING=ON`, Debug with `-O0 -g1`,
and all normal native targets and features. It rejects non-Debug configurations
to preserve assertions. CMake builds and CTest receive the same configuration,
including for multi-configuration generators. CTest is resolved beside the
selected CMake first, then from PATH

Test builds do not overwrite normal host replay freshness stamps. Add
`--sanitizer address` for independent `buildd1-test-asan` and `buildd2-test-asan`
trees, or `--clean` to recreate only the selected trees. Build-only invocation
without `--test` retains its existing defaults. Reports are written to
`ctest-results.xml` inside each test build tree, alongside `Testing/Temporary/`
and the CMake diagnostics retained by CI

The workflow first runs the isolated CLI regression suite:

```sh
python3 android/tests/test_linux_build_cli.py
```

It executes the real shell runner against recording tool shims in temporary
fixture repositories, then cleans them up. It checks argument validation,
tool discovery, routing, configuration/build/test failures, diagnostics options,
and separation from ordinary builds without compiling games or downloading assets

For interrupted coding tasks or GitHub access failures, see the
[task recovery runbook](TASK_RECOVERY.md).
