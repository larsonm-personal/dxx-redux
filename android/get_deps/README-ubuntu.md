# Fresh Ubuntu Android Dependencies

From the repo root:

```bash
./android/get_deps/get_all.sh
source android/set_vars.sh
```

`get_all.sh` installs small Linux host prerequisites, installs PowerShell if `pwsh` is missing, creates `dependency_base.txt` with the platform default if needed, then downloads the pinned JDK, Android SDK, NDK, emulator image, Gradle wrapper, and formatter tools. To do the PowerShell step by itself first, run `./android/get_deps/get_powershell.sh`.

To use a different dependency directory:

```bash
printf '%s\n' "$HOME/local" > dependency_base.txt
./android/get_deps/get_all.sh
```

The helper scripts also have Debian, Fedora, Arch, macOS, and Windows path handling where practical. Ubuntu is just the shortest fresh-VM path.
