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
