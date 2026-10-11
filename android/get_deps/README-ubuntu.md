# Fresh Ubuntu Android Dependencies

From the repo root:

```bash
./android/get_deps/get_all.sh
source android/helpers/set_vars.sh
```

`get_all.sh` installs Linux host prerequisites, installs PowerShell if `pwsh` is missing, creates `dependency_base.txt` with the platform default if needed, then downloads the pinned JDK, Android SDK, NDK, emulator image, Gradle wrapper, and formatter tools. The Linux prerequisite step includes the system C/C++ build toolchain, CMake, Ninja, pkg-config, Cargo, Rust, and the native desktop libraries used by the host regression tests.

If your shell cannot prompt for sudo, install the host prerequisites first from a terminal:

```bash
./android/get_deps/helpers/get_linux_build_prereqs.sh
```

To use a different dependency directory:

```bash
printf '%s\n' "$HOME/local" > dependency_base.txt
./android/get_deps/get_all.sh
```

The helper scripts also have Debian, Fedora, Arch, macOS, and Windows path handling where practical. Ubuntu is just the shortest fresh-VM path.

## Bounded extraction runtime

Bootstrap also installs the pinned Python 3.12.8 runtime used to supervise
bounded extraction. To install or verify it separately:

```bash
bash android/get_deps/helpers/get_bounded_python.sh
pwsh -NoProfile -File android/tests/test_bounded_python_runtime.ps1
```

Linux x86-64 uses the stripped install-only package from the
[20241219 python-build-standalone release](https://github.com/astral-sh/python-build-standalone/releases/tag/20241219).
The archive checksum and unpacked tree digest are pinned in `tool_versions.conf`.
Windows uses the existing verified embedded Python oracle package. Other hosts
still require an explicit runtime path and SHA-256; no PATH Python fallback is
allowed. Probes and supervision disable bytecode writes to preserve the verified
runtime tree. Linux installation uses the shared transaction lock and recovery,
and superseded registered Linux runtime directories participate in managed
dependency retirement. The download and staging tree are removed after install.

## Opt-in software CPU emulator tests

The test harness requires hardware CPU acceleration by default. For a host without
accessible KVM, explicitly opt into the much slower software CPU path:

```bash
export DXX_ANDROID_SOFTWARE_EMULATOR=1
pwsh -NoProfile -File android/helpers/emu_health.ps1 -Restart -Wait \
  -AvdName Nexus5X_Light_1 -PreferredSerial emulator-5554
```

The opt-in selects `-accel off`, headless SwiftShader, two virtual CPUs and
1536 MiB of guest RAM. Managed startup allows 300 seconds for ADB appearance and
1200 seconds for boot. `emu_health.ps1 -SoftwareCpu` also enables it for one
invocation; an explicit `-TimeoutSeconds` overrides its boot budget. The shared
startup helper preserves explicit timeout arguments. Unset the environment
variable to restore accelerated defaults.

The shared host infrastructure policy also allows 30-second ADB probes,
180-second launcher startup/handoffs and 300-second game-surface startup.
Explicit caller deadlines and in-script gameplay assertions are unchanged.
A software-mode launcher handoff waits for confirmed progress from the same run;
it fails without a force-stop if that handoff cannot be confirmed. This avoids
resetting a slow executor just as it resumes. Physical targets retain their
normal infrastructure budgets even if the environment variable is set.

Boot readiness requires both the Android package service and a writable shared
storage volume. A completed boot property alone is insufficient if user storage
has not mounted. In software CPU mode, boot recovery retries only an explicitly
unmountable `emulated;0` volume at most once per minute, allowing MediaProvider
to finish its slow first start. It verifies storage again after remounting.
Software rendering does not imply software CPU emulation, and
this option does not weaken test assertions or turn off Android security features.
Run device tests serially with a current debug APK and matching instrumentation
APK. Slow guest execution can still exceed individual test deadlines; report
those failures separately rather than claiming a full Android suite pass.
