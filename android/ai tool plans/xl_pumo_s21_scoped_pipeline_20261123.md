# Original Pumo S21 scoped-image pipeline, November 23

## Result

The connected SM-G996U (Android 15) repeated the original Pumo v1.1 archive
(SHA-256 `82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf`),
Rookie, 640 x 480, hub-to-Crauphel route. The isolated diagnostic APK
`com.dxxredux.app.nsdtest` version 32219 has SHA-256
`159bab6f4d1914460560caae2f4eaab8320193e24cede8ad251ea3e70913fba3`.
Both candidate runs passed all 35 scripted actions, including a missile in
Crauphel room 129 before and after quickload, and restored the protected
profile hashes. The original human touch shot remains a separate check.

| S21 phase                        | Serial 32217 | Pipeline 32219 first | Pipeline 32219 repeat |
| -------------------------------- | -----------: | -------------------: | --------------------: |
| Hub scoped images                |     3,193 ms |             2,553 ms |              2,500 ms |
| Hub prebriefing resources        |     8,081 ms |             7,401 ms |              7,336 ms |
| Crauphel scoped images           |     5,586 ms |             3,953 ms |              3,809 ms |
| Crauphel prebriefing resources   |     9,199 ms |             7,556 ms |              7,428 ms |
| Crauphel quickload scoped images |     5,234 ms |             3,542 ms |              3,591 ms |

These are device log phase times, not stopwatch or human touch observations.
The two Crauphel prebriefing gains are 1,643 and 1,771 ms against this serial
baseline. The sum of decode and hash worker times can exceed wall time because
two images are processed concurrently.

The change overlaps decoding and hashing for at most two already-read scoped
images of 8 MiB or less. Source lookup and reads remain on the preparation
thread. Results enter the retained image arena in source order, keeping
binding indices and source SHA-256 identities stable. Larger images retain the
serial path. No decoded image survives its resource owner or adds a persistent
cache. Pumo prepared 80/80 hub images and 143/144 Crauphel images on this
bounded path.

An earlier decoded-image reuse trial retained no images at the level-clear
boundary (`reused=0` on hub, Crauphel and quickload). It gave no speed benefit
and was discarded before this change.

## Verification and limits

- Scoped code quality and the Windows D2 headless-route build passed.
- Android Debug built for arm64-v8a, armeabi-v7a and x86_64.
- The original Pumo native `-mod-data-test` passed, covering repeated level
  preparation, changed source rejection, stock restoration and save identity.
- The original Pumo texture dependency audit prepared Crauphel twice and
  checked identical source hashes, pixels and separate ownership.
- Both protected S21 diagnostic routes passed 35/35 and restored the profile.
  The first and repeat log SHA-256 values are
  `970524d3ac552a13a6d5d11ae7d502a1314ffda78587e87d5c08f681f01a9aff`
  and `fbd6e229bdc3cdc299bcfa4f2fb6cb6ed06f7fb96b11e1afa9897add3f683f3f`.

The serial reference is the clean 32217 run reported in
[model reuse](xl_pumo_s21_model_reuse_20261123.md). The profile was protected,
and private game assets, saves and logs remain in ignored local workspaces.
The ordinary kit was rebuilt from the later stack tip as version 32320 and
installed on the S21 with a matching package hash. Its physical touch report
on the missing bar and missile exit remains open.
