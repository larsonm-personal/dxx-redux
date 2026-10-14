# Host CPU texture transition fixture

This fixture compiles the production Android `shared/pngfile.c` against both
D1 and D2 headers, with real PhysFS, the pinned stb image decoder, and the pinned
KTX-Software library. The only replacements are logging callbacks. ASan and UBSan
instrument the fixture, production decoder, KTX library, and the optional pinned
PhysFS static library. Unused ASTC dependency code is not instrumented.
LeakSanitizer defaults off because
executor ptrace prevents its reliable operation.

## Run

From the repository root with native development dependencies available:

```sh
cmake -G Ninja -S android/tests/host_texture_transitions/physfs \
  -B temp/host-pinned-physfs320-build
cmake --build temp/host-pinned-physfs320-build --target host_physfs_version -j 4
ASAN_OPTIONS=detect_leaks=0 temp/host-pinned-physfs320-build/host_physfs_version
cmake -G Ninja -S android/tests/host_texture_transitions \
  -B temp/host-texture-transitions-build \
  -DJSON_INCLUDE_DIR="$PWD/buildd1-test/_deps/nlohmann_json-src/single_include" \
  -DPHYSFS_INCLUDE_DIR="$PWD/temp/host-pinned-physfs320-build/_deps/physfs-src/src" \
  -DPHYSFS_LIBRARY="$PWD/temp/host-pinned-physfs320-build/_deps/physfs-build/libphysfs.a"
cmake --build temp/host-texture-transitions-build \
  --target host_texture_transitions_d1 host_texture_transitions_d2 -j 4
python android/tests/host_texture_transitions/run.py \
  --assets temp/overnight-assets --build temp/host-texture-transitions-build \
  --out temp/host-texture-transitions-results --steps 128 --samples 32 \
  --exhaustive-payloads --exhaustive-mounts --expected-physfs-version 3.2.0
```

Python requires Pillow. The runner needs the public D1/D2 128/256 KTX2 texture
DXAs already present locally; it does not download or modify them. CMake uses
repository dependency URLs and SHA-256 pins. The PhysFS helper statically links
the same 3.2.0 version as Android and prints actual header/runtime versions;
`paths.txt` lists its exact include/archive locations. Omitting those CMake paths
uses the host's installed PhysFS instead. Results always record actual compiled
and runtime versions, require them to match, and can require a specific runtime
version through `--expected-physfs-version`. Call the repository artifact
retention helper before creating a new timestamped output directory.

## Checks

- Independently parse ZIP, KTX2 headers, original dimensions, mip index and exact
  compressed payload bytes using Python; compare production CPU loader results
- Independently decode public PNG mask pixels with Pillow and compare every byte
- Resolution 128 -> 256 -> 128, disable/re-enable, repeated reads, ordered pack
  overlap, two game namespaces, imported D1 under `d1-mods`, case/slash aliases,
  and deterministic seeded mount/unmount transitions
- Deliberately omit cache clearing to prove stale file-index and cached-miss
  detection, then recover with production `clear_texture_lookup_cache`
- Deliberately corrupt only expected source, dimensions, or output bytes and
  require the comparison to fail; source archives remain unchanged
- Optional exhaustive pass through every eligible KTX2/PNG member: both D1
  archives with D1 headers; all four archives with D2 headers
- Optional exhaustive ordered subsets with launcher-compatible namespaces:
  five D1 states and 65 D2 states, including no packs and both resolutions enabled
- Explicit negative-control checks remain enabled under Python optimization;
  subprocess timeouts retain partial logs and an `incomplete` summary

D1 packs mount at root for D1. D2 packs mount at root for D2, with D1-only packs
under `d1-mods`, matching `ModManager.writeEnabledModPaths`. Priority permutations
exercise the loader contract; the fixture does not claim every ordering is
selectable through the launcher UI.

Scenarios, complete observations, logs, asset hashes, unique state/transition
counts, and summary remain in the output directory for deterministic replay.
Rerun a binary with `GAME-scenario.json RESULT.json` to replay one game directly.

## Boundaries

KTX2 payload bytes remain compressed ETC2. This does not verify GPU texels,
renderer upload/residency/readback, presentation output, menu bindings, or
whether production engine lifecycle callers clear caches. Cache omission cases
are deliberate invalidation controls, not evidence of a product bug. These
public PNGs are grayscale masks, including transparency; color PNGs and
downsampling beyond 2048 pixels are not covered. D1XR PCX/FNT presentation decoding is outside this loader.
No malformed binary generation or mutation is performed.
