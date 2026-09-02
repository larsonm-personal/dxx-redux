# Windows-native mission regeneration parity plan

## Implementation status, 2026-09-01

The first production path is implemented:

- `regenerate_all_mission_metadata.ps1` defaults to `-Engine Windows`; the
  retained implementation is available with `-Engine Emulator`.
- D1 and D2 persistent Windows workers compile and call the same
  `jni_level_metadata.cpp` analyzer entry point as Android.
- One D1 and one D2 worker are reused by the batch instead of opening a process
  or console window per mission.
- The Android mission variant policy now lives in the JVM-only
  `:mission-metadata-core` module and the Windows runner reads its precedence
  through `:mission-metadata-cli`.
- Mission descriptor parsing/legacy decoding and checked-in JSON projection now
  also live in `:mission-metadata-core`; Android and Windows call the same
  implementations. The CLI remains alive for the batch.
- Gradle and CMake builds run incrementally by default. `-NoBuild` is an
  explicit opt-out.
- `MissingOnly`, targeted archive lists, and hash-ring samples dispatch through
  the Windows path. A zero-item sample exits instead of accidentally running the
  full corpus.
- `compare_mission_metadata_engines.ps1` retains a focused emulator parity run
  without putting the emulator in the normal all path.
- D1 `trainng.zip` and D2 `mustfind.zip` smoke runs are byte-identical to their
  checked-in JSON, and repeated requests to one persistent D2 worker are
  byte-identical.

The later cleanup phases below remain useful follow-up: move the remaining
archive preparation from PowerShell into the shared Kotlin core, broaden the
emulator parity corpus, delete the now-unused PowerShell projection helpers,
and remove the legacy standalone headless analyzer after its remaining callers
migrate.

### Build-output return-value hotfix

- [x] Reproduce the category 4 failure from the supplied log
- [x] Keep Gradle and native build console output out of PowerShell function return values
- [x] Add a regression test that simulates noisy build output
- [x] Run category 4 through the normal public entry point without `-NoBuild`

## Goal

Design a fast Windows-native replacement for the emulator-backed mission
metadata corpus regeneration that executes the same production C++ and Kotlin
logic, rebuilds stale components automatically, and retains one emulator parity
test without including the emulator in the normal `all` workflow.

## Investigation

- [x] Map the current `regenerate all`, emulator batch, and host analyzer flows
- [x] Identify production logic that exists only in Android/Kotlin or JNI glue
- [x] Define one shared request, result, normalization, and artifact contract
- [x] Design stale-build detection and a persistent single-process Windows runner
- [x] Define focused and corpus commands plus emulator parity coverage
- [x] Record phased implementation, migration, validation, and rollback criteria

## Current state and gap

The default metadata stage in `regenerate_all_regression_data.ps1` calls
`regenerate_all_mission_metadata.ps1`. Mission archives are sent through the
APK, launcher automation, Android import storage, an Android analysis service,
and JNI. Extracted-CD sources alone use the Windows host analyzer.

`regenerate_all_mission_metadata_host.ps1` is useful and fast, but it is not a
parity implementation:

- It reimplements mission variant selection and checked-in JSON projection in
  PowerShell instead of executing `MissionVariantPolicy.kt`,
  `LevelMetadataTargets`, and the launcher serializer.
- `headless_metadata_dump_main.cpp` has its own request setup, level loop,
  mission-intent assembly, failure rows, and serialization alongside the same
  concepts in `jni_level_metadata.cpp`.
- The Android path owns archive inspection, extraction limits, descriptor
  parsing, target construction, HOG selection, HXM staging, and request JSON.
- The shared final JSON normalizer prevents formatting drift, but it cannot
  prevent policy or analysis drift before serialization.

Making the existing host script the default would therefore be faster, but it
would not satisfy the same-code requirement.

## Target architecture

Use one long-lived Kotlin/JVM command-line process as the corpus coordinator
and one persistent native worker for each game:

```text
regenerate_all_regression_data.ps1
  -> regenerate_all_mission_metadata.ps1
    -> Kotlin/JVM mission-metadata CLI
       -> shared Kotlin archive, target, request, result, and serializer code
       -> D1 native metadata worker  <-> shared C++ analyzer
       -> D2 native metadata worker  <-> shared C++ analyzer
       -> shared canonical JSON normalizer
       -> atomic checked-in JSON writes
```

The APK uses the same Kotlin core and C++ analyzer:

```text
Android launcher and analysis service
  -> Android storage and service adapters
  -> shared Kotlin metadata core
  -> JNI adapter
  -> shared C++ analyzer
```

Only platform mechanics remain separate. Archive and mission policy, request
fields, analysis behavior, result parsing, checked-in projection, and final
normalization remain shared.

## Shared contracts

### Kotlin core

Add a Gradle Java-library/Kotlin-JVM module named `:mission-metadata-core`.
The Android app and the Windows CLI both depend on it. Move or extract the
following production logic into it:

- Mission descriptor parsing and mission-set construction
- Mission variant precedence and unsupported-variant decisions
- Archive path normalization, collision checks, extraction budgets, and size
  limits
- `LevelMetadataTarget`, prepared target construction, and request JSON
- Native result models and parsing
- Checked-in mission and level projection, including music and mission intent
- Canonical ordering rules before the shared final JSON formatting step

The core must not import `android.*`. Define narrow interfaces for archive
streams, free-space checks, temporary work directories, cancellation, clocks,
and progress. The Android module supplies the existing storage and service
implementations. The CLI supplies ordinary JVM filesystem implementations.

Use the already pinned Kotlin and `kotlinx-serialization-json` versions. Avoid
a second DTO set for the CLI. The same request and result types must compile
into both applications.

### Archive adapters

Retain one shared archive policy above a `ReadableArchive` interface:

- Android uses its existing ZIP, Commons Compress 7z, and native RAR adapters.
- Windows uses ZIP and Commons Compress where equivalent, plus the repository's
  pinned 7-Zip 26.02 binary for formats requiring the native backend.
- Both adapters feed identical normalized entry records into the core and are
  subject to the same entry count, expansion, memory, path, collision, and
  unsupported-format decisions.

The parity test must cover every supported archive family. A backend difference
is acceptable only below the shared entry-stream contract, not in mission
selection or output policy.

### C++ analyzer

Extract the common implementation currently split between
`jni_level_metadata.cpp` and `headless_metadata_dump_main.cpp` into shared
`level_metadata_analyzer.{h,cpp}` code. It owns:

- Runtime initialization after platform filesystem setup
- Per-request mounts and guaranteed cleanup
- Mission loading, level enumeration, and missing-level classification
- Level statistics, replacement/HXM handling, GuideBot route analysis, and
  mission-intent accumulation
- Failure rows, result schema, and serialization
- Cancellation and progress callbacks

JNI becomes a thin string/context adapter. The Windows worker becomes a thin
console/protocol adapter. D1 and D2 remain separate executables because their
engines are compile-time variants, but both compile the exact same shared
analyzer source used by Android.

## Native worker protocol

Use newline-delimited, compact JSON on redirected standard input and output.
Each message has `schema`, `type`, `request_id`, and `game`. Message types are:

- `hello`: worker build identity, game, analyzer schema, and cache generation
- `analyze`: the exact prepared request produced by the Kotlin core
- `progress`: level and planner progress using the existing checkpoint fields
- `result`: the exact native result object
- `cancel`: cooperative cancellation for the active request
- `shutdown`: clean worker exit

Keep logs on stderr so stdout remains machine-readable. The CLI starts one
hidden D1 worker and one hidden D2 worker and reuses them for the whole corpus.
It restarts a worker after a crash or poisoned cleanup state and fails only the
active mission. An A, B, A repeat test must prove that reuse does not leak
mission, mount, robot, music, or route state.

## Build and stale-source behavior

Do not invent timestamp checks. The Windows wrapper always invokes incremental
build systems unless `-NoBuild` is explicitly supplied:

1. Set JDK 21 and run the Gradle CLI distribution task. Gradle rebuilds only
   changed Kotlin or resource inputs.
2. Invoke `run-windows-build.ps1` with a new metadata-only product selector.
   CMake and Ninja rebuild the D1/D2 workers only when their transitive native
   inputs changed.
3. Launch the CLI from its Gradle install distribution and pass the resolved
   worker paths explicitly.

Add build identity fields to the worker handshake and run summary. Refuse a
worker with the wrong game, protocol, analyzer schema, or cache generation.
This detects accidentally mixed binaries without maintaining another source
hash system.

## Commands and integration

Keep `regenerate_all_mission_metadata.ps1` as the stable public entry point.
Give it an explicit engine selector during rollout:

```powershell
# Default after parity promotion
.\android\helpers\regenerate_all_mission_metadata.ps1

# Focused native diagnosis
.\android\helpers\regenerate_all_mission_metadata.ps1 `
    -Engine Windows -ArchiveName Obsidian.zip

# Retained Android implementation
.\android\helpers\regenerate_all_mission_metadata.ps1 `
    -Engine Emulator -ArchiveName Obsidian.zip
```

Preserve `-MissingOnly`, runtime sampling, source selection, output summaries,
failure JSON, and atomic regression copies in both modes. The normal `All`,
`Metadata`, `MissingMetadata`, and 45-minute paths always use Windows after
promotion. No `adb`, emulator startup, APK install, or Android window is allowed
from those paths.

The old emulator batch remains callable and maintained. It is not invoked by
the normal all-category run.

## Emulator parity test

Add one integration entry point,
`android/tests/test_mission_metadata_windows_android_parity.ps1`. One test
invocation may contain a small fixture per game so that it covers both native
binaries while paying emulator startup cost only once.

The fixture set should cover:

- D1 and D2 descriptors and normal/secret level numbering
- Multiple mission descriptors and variant masking
- ZIP and 7z staging
- Custom HXM or replacement metadata
- Music metadata and mission mode declarations
- A missing optional secret and one stable failure classification

The test builds the APK, CLI, and workers; runs the same fixture inputs through
both paths; applies the same normalizer; and requires byte-identical checked-in
projection plus identical per-target status. It also runs each Windows target
twice to prove deterministic worker reuse.

Keep this test in the Android integration suite and pre-release validation, not
inside `regenerate all`. A smaller no-emulator contract test runs on every host
test pass and compares the Android and CLI serializers against stored raw native
result fixtures.

## Implementation phases

### Phase 1: Freeze contracts and fixtures

- Capture canonical prepared requests, raw native responses, and checked-in
  projections from representative D1 and D2 missions.
- Add serializer parity tests before moving code.
- Add request schema, analyzer schema, and worker protocol versions.
- Record current emulator runtime and warm/cold host runtime.

Exit criterion: the fixtures detect a changed target, request, result, or JSON
projection independently.

### Phase 2: Share Kotlin policy and serialization

- Create `:mission-metadata-core` and move pure models and policies first.
- Introduce platform interfaces and keep Android behavior unchanged.
- Move archive scanning, target construction, request preparation, result
  parsing, and checked-in serialization in small reviewed slices.
- Replace PowerShell projection and variant mirrors with calls to the core CLI.

Exit criterion: existing Android tests pass and the JVM core produces the same
requests and projected JSON as the launcher for all frozen fixtures.

### Phase 3: Share the native analyzer

- Extract the common analyzer from JNI without changing its request schema.
- Make the Android JNI wrapper call the shared entry point.
- Make the D1/D2 console workers call the same entry point.
- Remove duplicated headless mission intent, level loop, failure-row, and
  serialization code only after parity tests pass.

Exit criterion: raw Android and Windows native results are equivalent for the
fixture set before Kotlin projection.

### Phase 4: Build the persistent Windows CLI

- Add `:mission-metadata-cli` with the Gradle application plugin.
- Add archive-source manifests, focus filters, sampling, missing-only behavior,
  progress, cancellation, summaries, atomic output, and hidden worker startup.
- Reuse one D1 and one D2 worker and add crash restart plus A, B, A isolation
  tests.
- Add metadata-only native build targets and incremental wrapper integration.

Exit criterion: a warm full corpus uses one JVM and at most two native workers,
opens no console windows, and produces a complete artifact summary.

### Phase 5: Shadow corpus and promote

- Run Windows generation without copying results and compare the complete
  normalized corpus against the current checked files.
- Classify every difference as an actual shared-code correction or a parity bug.
- Run the emulator parity integration test on the fixed fixture set.
- Make Windows the default for `Metadata`, `MissingMetadata`, `All`, and the
  45-minute sampler while retaining `-Engine Emulator`.
- Update runtime estimates after a successful full run.

Exit criterion: zero unexplained normalized differences and no emulator process
or `adb` call in the default metadata stage.

### Phase 6: Remove mirrors, retain the oracle

- Delete the PowerShell mission projection and variant-precedence mirrors.
- Keep orchestration PowerShell thin: build, invoke, report, and select mode.
- Keep the emulator implementation and parity test working in CI or scheduled
  Android integration runs.

Exit criterion: there is one Kotlin policy/serializer implementation, one C++
analyzer implementation, and two thin platform adapters.

## Acceptance criteria

- Default mission metadata regeneration never starts or contacts an emulator.
- Android and Windows execute the same Kotlin target/request/result/serializer
  code and the same C++ analyzer code.
- Windows supports the same archive families, descriptor modes, variants,
  custom definitions, music metadata, missing inputs, and failure states.
- Parity fixtures produce byte-identical normalized regression JSON.
- Worker reuse is history-independent and deterministic.
- Incremental builds are automatic; `-NoBuild` is opt-in only.
- A warm full corpus is at least four times faster than the recorded emulator
  baseline, with a stretch target below 15 minutes on the reference PC.
- The retained emulator parity test remains runnable with one command and is not
  part of the normal `all` invocation.

## Risks and controls

- Windows and Android floating-point or filesystem behavior may expose real
  engine nondeterminism. Fix shared engine behavior rather than adding output
  exceptions.
- Archive backends may report unknown sizes differently. Normalize entry facts
  at the shared adapter boundary and test the same malicious and unusual
  archives on both platforms.
- Long-lived engine globals may leak between requests. Keep strict per-request
  mount cleanup, poison on cleanup failure, restart on poison, and require A,
  B, A result equality.
- Moving Kotlin files can accidentally pull Android UI or service dependencies
  into the core. Enforce a JVM-only core compilation and forbid `android.*`
  imports there.
- During migration, dual writers could recreate JSON churn. Only the shared
  serializer may create checked-in mission JSON; platform layers return typed
  targets or raw native results.
