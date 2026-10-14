# Linux native functional configuration campaigns

These opt-in tests exercise actual menus, settings, gameplay, audio and rendered output using public shareware data. They queue ordinary SDL key events and observe engine state. They do not replace Android automation or establish Android/retail coverage.

## Build and runtime prerequisites

The Python runners are **Linux/POSIX-only**: process supervision uses sessions/process groups and `killpg`, and profiles use the Linux `.d1x-redux`/`.d2x-redux` layout. The guarded C instrumentation is disabled by default and is separate from Android automation.

Install the normal native build dependencies described by the repository build configuration: C/C++ compiler, CMake 3.21+, a build tool such as Ninja, SDL 1.2 development interfaces (or SDL12 compatibility), SDL_mixer, OpenGL/Mesa, PhysFS and PNG. The runners require Python 3.10+ and Pillow. Music cases additionally need a functioning SDL_mixer MIDI backend and a SoundFont; the recorded campaign used FluidSynth and TimGM6mb SF2.

From the repository root, configure separate test builds:

```sh
cmake -S d1 -B build-host-probe-d1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DDXX_HOST_CONFIG_PROBE=ON -DOPENGL=ON -DSDLMIXER=ON
cmake --build build-host-probe-d1 --target dxx-redux-d1
cmake -S d2 -B build-host-probe-d2 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DDXX_HOST_CONFIG_PROBE=ON -DOPENGL=ON -DSDLMIXER=ON
cmake --build build-host-probe-d2 --target dxx-redux-d2
```

The executables are `build-host-probe-d1/main/d1x-redux` and `build-host-probe-d2/main/d2x-redux`. Use a separate build with `-DDXX_HOST_CONFIG_PROBE=OFF` for production build checks. Do not relink a binary while a runner is copying it: runners copy and hash each executable before launch, then use that immutable copy.

Provide a working SDL/OpenGL display. An existing X11 display can be used. The recorded environment used SDL12 compatibility backed by SDL2 and Mesa with:

```sh
export SDL_VIDEODRIVER=offscreen
export LIBGL_ALWAYS_SOFTWARE=1
export SDL_AUDIODRIVER=dummy
export SDL_SOUNDFONTS=/absolute/path/to/TimGM6mb.sf2
```

`offscreen` is not supported by every SDL installation; do not interpret a missing display/context as a game test pass. Dummy audio still permits SDL_mixer postmix PCM observation, but does not test speakers. The controller defaults to dummy audio/software rendering only when those environment variables are unset. Audio cases report a missing backend/track as blocked.

## Caller-provided data and output locations

The commands below assume caller-provided public shareware directories:

- `D1_DATA`: original `descent.hog` and `descent.pig`
- `D2_DATA`: `d2demo.hog`, `d2demo.ham` and `d2demo.pig`
- `D12_DATA`: a disposable combined directory containing both sets under their original names; copying or symlinking the verified files is sufficient

No runner downloads retail data. Do not modify the original archives or reuse a personal game profile. Every runner creates a private HOME/profile beneath its output directory. The mission names below are the observed English public-demo names; adapt explicit mission arguments only for fixtures you actually have.

```sh
D1="$PWD/build-host-probe-d1/main/d1x-redux"
D2="$PWD/build-host-probe-d2/main/d2x-redux"
D1_DATA=/absolute/path/to/public-d1
D2_DATA=/absolute/path/to/public-d2
D12_DATA=/absolute/path/to/combined-public-data
RUN="$PWD/temp/host-functional-$(date -u +%Y%m%dT%H%M%SZ)"
DEADLINE="$(date -u -d '+2 hours' +%Y-%m-%dT%H:%M:%S+00:00)"
mkdir -p "$RUN"
```

Use a new output directory for every run. Some helpers reject an existing directory; others expect the caller to supply a fresh one. Keep failed evidence. The deadline is an explicit timezone-qualified ISO-8601 cutoff, not a date embedded in the runner.

## Five campaigns

### 1. Settings/widget/persistence matrix

```sh
python3 android/helpers/host_config_campaign.py \
  --executable "$D1" --data "$D1_DATA" --game d1 \
  --seed 20261014 --hours 2 --output "$RUN/config-d1"
python3 android/helpers/host_config_campaign.py \
  --executable "$D2" --data "$D2_DATA" --game d2 \
  --seed 20261014 --hours 2 --output "$RUN/config-d2"
```

This covers volume, music mode, stereo, filtering, VSync, brightness and mouse-control style. It asserts actual widgets, effective values, A-B-A and fresh-process persistence. `--prepare-only` generates a plan without claiming execution. `--limit 1` is a smoke subset, not complete pairwise coverage. `--replay FILE` accepts a saved result object containing `values`.

### 2. Expanded gameplay/settings matrix

```sh
python3 android/helpers/host_gameplay_campaign.py \
  --executable "$D1" --data "$D1_DATA" --game d1 \
  --mission 'Descent Demo' --version 1 --seed 20261014 \
  --deadline "$DEADLINE" --output "$RUN/gameplay-d1"
python3 android/helpers/host_gameplay_campaign.py \
  --executable "$D2" --data "$D2_DATA" --game d2 \
  --mission 'Descent 2 Demo' --version 2 --seed 20261014 \
  --deadline "$DEADLINE" --output "$RUN/gameplay-d2"
python3 android/helpers/host_gameplay_campaign.py \
  --executable "$D2" --data "$D12_DATA" --game d2 \
  --mission 'D1: Descent Demo' --version 1 --seed 20261014 \
  --deadline "$DEADLINE" --output "$RUN/gameplay-d1-in-d2"
```

Eleven axes cover graphics effects, cockpit/reticle, control sensitivity and actual difficulty. Each case includes live options, automap, a real ship turn followed by save restoration, rendered gameplay, relaunch and baseline restoration. Save-file existence alone is never the restoration oracle. This campaign mutes music/SFX and does not claim audio correctness.

### 3. Keyboard binding/cancel/real-input regression

```sh
python3 android/helpers/host_binding_campaign.py \
  --executable "$D1" --data "$D1_DATA" --game d1 \
  --mission 'Descent Demo' --version 1 --output "$RUN/binding-d1"
python3 android/helpers/host_binding_campaign.py \
  --executable "$D2" --data "$D2_DATA" --game d2 \
  --mission 'Descent 2 Demo' --version 2 --output "$RUN/binding-d2"
python3 android/helpers/host_binding_campaign.py \
  --executable "$D2" --data "$D12_DATA" --game d2 \
  --mission 'D1: Descent Demo' --version 1 --output "$RUN/binding-d1-in-d2"
```

Checks cancel, new-key persistence, removed-key no-motion, previously unused-key motion after rebinding, full keyboard-map restoration and clean control-window exit. Navigation uses observed binding neighbors rather than assumed screen coordinates.

### 4. Music state/PCM/source campaign

```sh
python3 android/helpers/host_music_campaign.py \
  --executable "$D1" --data "$D1_DATA" --game d1 \
  --rounds 5 --seed 20261014 --timeout 240 --output "$RUN/music-menu-d1"
python3 android/helpers/host_music_campaign.py \
  --executable "$D1" --data "$D1_DATA" --game d1 --gameplay \
  --mission 'Descent Demo' --mission-version 1 --timeout 240 --output "$RUN/music-game-d1"
python3 android/helpers/host_music_campaign.py \
  --executable "$D2" --data "$D2_DATA" --game d2 --gameplay \
  --mission 'Descent 2 Demo' --mission-version 2 --timeout 240 --output "$RUN/music-game-d2"
python3 android/helpers/host_music_campaign.py \
  --executable "$D2" --data "$D12_DATA" --game d2 --gameplay \
  --mission 'D1: Descent Demo' --mission-version 1 --timeout 240 \
  --reference-music-report "$RUN/music-game-d1/music-report.json" --output "$RUN/music-game-d1-in-d2"
```

`--playlist` and `--missing-source` are separate bounded music variants. Audio oracles observe actual mixer output after drain/sample windows, not merely configuration values. Converted-MIDI source fingerprints distinguish the native D1 and D2 soundtracks. The native results do not cover Android ymfm/TSF or speaker output.

### 5. Rendered filtering-change proof

```sh
python3 android/helpers/host_render_campaign.py \
  --executable "$D1" --data "$D1_DATA" --game d1 --output "$RUN/render-d1"
python3 android/helpers/host_render_campaign.py \
  --executable "$D2" --data "$D2_DATA" --game d2 --output "$RUN/render-d2"
```

Asserts successful GL readback, expected 640x480 RGB bytes, nonblank spatial content and a meaningful nearest/trilinear image difference. This is a rendering-capability/filter-change proof, not exhaustive visual correctness.

## Two cross-mode integrations

Persistent-menu activation, mission-picker cancellation and rendered returns:

```sh
python3 android/tests/test_host_mode_menu_return.py \
  --d1 "$D1" --d2 "$D2" --d1-data "$D1_DATA" --d2-data "$D2_DATA" \
  --combined-data "$D12_DATA" --output "$RUN/mode-menu-return"
```

Independent native soundtrack references, D1 -> D2 -> D1 source restoration and loose override precedence (requires the two native gameplay music reports above):

```sh
python3 android/tests/test_host_music_source_routing.py \
  --executable "$D2" --data "$D12_DATA" \
  --native-d1-report "$RUN/music-game-d1/music-report.json" \
  --native-d2-report "$RUN/music-game-d2/music-report.json" \
  --output "$RUN/music-source-routing"
```

Both use one disposable D2 profile/process for cross-mode restoration, rather than treating independent isolated profiles as a shared-state transition.

## Offline oracle controls

The four explicitly allowlisted offline control suites require only Python 3.9+ and its standard library, with assertions enabled. They run with `python -S` and do not require Pillow, game binaries, SDL, fixtures, a display or a device. The core/host-only PowerShell owner is:

```sh
pwsh -NoProfile -File android/tests/test_host_campaign_oracles.ps1
```

It runs only configuration, gameplay, music and rendering oracle controls. It does not discover or launch neighboring integration scripts. The actual OpenGL capture/PNG integration helpers still require Pillow. Offline controls validate rejection logic and plan coverage; they do not replace actual engine runs.

## Bounds, outputs and interpretation

- Every operation and game process is bounded. The settings matrix uses `--hours`; gameplay requires `--deadline` and has a 300-second per-case bound. Other runners have fixed or explicit short case bounds.
- The gameplay matrix stops at its first failed case. A cutoff prevents new cases; a current case may finish within its own bound. Incomplete execution exits nonzero and is not a functional pass.
- `Ctrl-C` interrupts a runner; session cleanup terminates its owned child process group. Do not use a global kill command or shared profile.
- Outputs include `trace.jsonl`, engine logs, private profile, executable/source identity where provided, planned cases, per-case result and summary. Render cases also retain images and GL metadata; music cases retain `music-report.json`.
- Assertions reject crashes, sanitizer evidence, stale/hung observations, wrong state and late teardown failure. A surviving process alone is not success. Missing display/audio/assets are blockers, not passed cells.
- Report the actual binary hash and fixture identity. These tests can use immutable binaries from before later working-tree edits; a current source checkout is not proof that an older binary tested those edits.
- The dated campaign report is `android/ai tool plans/testing/host_configuration_results_20261014.md`. It separates committed fixes, verified coverage, unvalidated work and Android/retail gaps.
