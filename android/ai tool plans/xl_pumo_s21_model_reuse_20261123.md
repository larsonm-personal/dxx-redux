# Original Pumo S21 model reuse, November 23

## Result

The SM-G996U on Android 15 repeated the original Pumo Rookie, 640 x 480,
hub-to-Crauphel route with the original v1.1 archive (SHA-256
`82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf`).
Each run used the isolated `com.dxxredux.app.nsdtest` package, imported through
the launcher picker, passed 35/35 ordinary game actions including a missile
before and after quickload, and restored the protected profile hashes. The
ordinary input kit and human touch report remain separate coverage.

| Diagnostic APK          | Crauphel owned models | Crauphel level resources | Crauphel prebriefing check | Quickload owned models |
| ----------------------- | --------------------: | -----------------------: | -------------------------: | ---------------------: |
| 32215 baseline          |                879 ms |                 3,789 ms |                   9,475 ms |                 836 ms |
| 32216 candidate, first  |                659 ms |                 3,589 ms |                   9,233 ms |                 608 ms |
| 32216 candidate, repeat |                664 ms |                 3,594 ms |                   9,221 ms |                 616 ms |
| 32217 cleaned source    |                667 ms |                 3,612 ms |                   9,232 ms |                 626 ms |

The baseline APK SHA-256 is
`e12e5df936771f76dbe44442b3d73f6fb96d3a64b15b89272a7fe6f37c6f5e7b`;
the repeated candidate is
`51e42532d25dfbcfbdfa166d70ef9d07c835e1cbadf569254ae8553880113f5d`;
the cleaned final diagnostic is
`cabc22fd2b5b07edeed2780410efe0706630d3655e5d0e9ea2d0a3aecfe94854`.
Timings are device logs for one scripted route, not a physical-touch latency
claim. The two candidate prebriefing gains against this baseline were 242 and
254 ms; the cleaned build gained 243 ms.

Pumo loads 36 owned global models with 67 skins at mission start, then
reprepares them for each level. On Crauphel, 27 global models have unchanged
model and skin bytes, while nine level-local models need fresh decoding. The
new path shares only the published CPU model of the same ID when the current
global model file and every resolved global skin file still hash to the
published source map. It rereads each file, keeps level-local folders on the
normal path, and holds no additional cross-mission cache. A changed skin
falls back to decode and changes the source identity.

Other measured costs remain: scoped images take about 5.2–5.6 seconds on this
route; named audio takes about 2.2 seconds. A bounded classic WAV hash trial
changed the roughly 395 ms hash phase by only 15–20 ms and was discarded.
Initial bitmap/sound bank verification took about 480 ms, while same-bank
quickload verification took about 2 ms. A later speed tranche should examine
verified scoped-image reuse without weakening source checks or increasing the
resource bound.

## Validation and limits

- Scoped code quality passed for the runtime and native model test.
- Windows D2 headless metadata and route targets built.
- The original Pumo native `-mod-data-test` passed against a disposable copy
  of the private inputs. It checks unchanged global owner reuse and that a
  changed model skin cannot retain the old source identity.
- Android Debug built for arm64-v8a, armeabi-v7a and x86_64. Final diagnostic
  version 32217 passed the protected 35-step S21 route with profile hashes
  restored.
- The exact original human SM-S931U Android SDK 37 touch missile exit at
  Crauphel room 129 remains unverified on an ordinary kit. Automated steering
  and fixed-position shots do not reproduce every human aim and impact.

Private archives, game data, profiles, saves and phone logs remain only in
ignored local workspaces and on the build machine.

## Hosted workflow admission, November 23

On this PR's first head, the private XL smoke
[run 45166807311](https://github.com/larsonm-personal/dxx-redux/actions/runs/45166807311)
failed before game execution: the hosted apt mirror stalled and the build job
was canceled after 30 minutes. The sampled and expanded readiness check runs
retained two `ERROR` badges. Attempt 2 of that workflow concluded success,
but its admission step explicitly said "Skipping private XL qualification
below the native stack tip"; no private gameplay test ran on the retry. The
public selected sweep
[run 45166775272](https://github.com/larsonm-personal/dxx-redux/actions/runs/45166775272)
also needed an infrastructure retry after Kotlin plugin resolution and apt
timeouts; its selected jobs passed on attempt 2. The stale private error
badges require a fresh PR head and workflow admission evidence, rather than
being reported as passing tests.
