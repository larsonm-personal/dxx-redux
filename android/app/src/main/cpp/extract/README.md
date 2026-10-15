# Desktop extraction and shared-code tests

The default CMake configuration is the full host profile. It downloads verified third-party
sources and includes optional codec, synthesis, JSON, archive corpus, and real-media tests.

## Download-free Linux profile

For a small headless checkout, configure the same entry point with:

```sh
cmake -S android/app/src/main/cpp/extract -B /tmp/dxx-extract-light \
  -DDXX_EXTRACT_DEPENDENCY_LIGHT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/dxx-extract-light --parallel 2
ctest --test-dir /tmp/dxx-extract-light --output-on-failure
```

This profile needs only CMake, a C/C++ compiler, and a build tool. It neither finds nor fetches
third-party libraries. It reuses the original test targets, compile definitions, and assertions;
Release builds retain test assertions. Generated CUE/ISO fixtures stay in the build tree, with
checked-in CUE fixtures copied there. The source checkout does not need to be writable.

On Linux the current inventory is **58 registrations: 36 executed and 22 explicitly skipped**.
Skipped registrations carry the `full-profile-required` label and print their test name and reason.
CTest's successful exit means the executed tests passed; it does not mean optional coverage ran.
The light profile runs HFS and STI2 in the isolated build tree and does not stage source-checkout
media at their relative lookup paths. They therefore normally report **eight internal real-media
skips**, even if the source checkout contains discs. Use the full profile for those checks. These
internal skips do not appear in CTest's skipped-registration count.

The profile covers synthetic CUE/ISO, HFS, STI2, malformed StuffIt, SOW Huffman/integrity,
extraction budgets, graphics persistence, Linux directory sync/rollback, rendering helpers,
MIDI/HMP metadata, UTF-8/SHA-1, SAF contracts, bounded reads, demo limits, and pilot transactions.
It does not cover JSON-backed tests, PHYSFS declarations, zlib/LZMA, StuffIt corpus/demo archives,
Chromaprint/PCM audio, TinySoundFont/FluidSynth/ymfm, or real SOW media. Use the default full
profile with the pinned dependencies and appropriate legally obtained media for those tests.
Public installers are not a substitute for the retail-media oracles. No media is downloaded by
this profile.

To verify configuration, the no-dependency-cache contract, build, registration count, and CTest
results together (Python 3 and CTest with `--output-junit` support are required):

```sh
python3 android/app/src/main/cpp/extract/test_dependency_light_profile.py \
  --build-dir /tmp/dxx-extract-light
```

Use a fresh build directory when switching profiles. The default remains
`DXX_EXTRACT_DEPENDENCY_LIGHT=OFF`; the light profile never silently substitutes system library
versions for the full profile's pinned dependencies.
