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
timeout, the test step has a three-minute limit, and zero discovered tests is
an error. CTest logs/JUnit and configuration diagnostics are retained for three
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

Install the packages listed in `engine-smoke.yml`, then repeat its configure,
build and CTest commands with `d1` or `d2`. Use a different build directory for
each game. The workflow deliberately invokes the existing CMake/CTest entry
points directly, so there is no second test list or CI-only runner to maintain
