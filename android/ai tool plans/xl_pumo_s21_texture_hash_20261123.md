# Pumo S21 scoped texture hashing, November 23

## Cause

The connected SM-G996U (Android 15) ran the original Pumo v1.1 archive in
the isolated `com.dxxredux.app.nsdtest` package. On the protected Rookie,
640 x 480 hub-to-Crauphel route, preparation of Crauphel's 144 authored
scoped images took 7.077 seconds. SHA-256 alone took 3.152 seconds, followed
by decode at 2.273 seconds and file reads at 1.433 seconds. The previous
`picosha2::hash256_hex_string(vector)` path passed the whole TGA to
`hash256_one_by_one::process`, which copies all input bytes into its working
vector before hashing and erasing them. Its memory and copy costs grew with
the image size.

## Change

Pass each retained image to the same SHA-256 state in at most 64 KiB batches.
The digest, the retained source identity and the later source check are
unchanged. The maximum transient hash buffer is bounded by one batch.
Android phase timing separates header scans, file reads, decode, shrink and
hash for follow-up work; source admission and renderer ownership are still
fresh. This layer does not cache decoded images or weaken validation.

## Matched S21 measurements

All runs used the same original archive, episode entrance, ordinal, Rookie,
resolution and 35-step hub-to-Crauphel script. The baseline was the #726
source with phase timing only, diagnostic version 32190. The candidate was
diagnostic version 32190, APK SHA-256
`6c468078f2ee7df49199455a772fafe7ae452bb800d11f10b9513a41f7d5b273`.
The APK hash distinguishes these builds despite the same version code.

| Milestone | Timed baseline | Candidate | Candidate repeat |
| --- | ---: | ---: | ---: |
| Hub pre-briefing check | 9.566 s | 8.400 s | 8.405 s |
| Crauphel pre-briefing check | 11.468 s | 9.508 s | 9.491 s |
| Crauphel scoped image SHA-256 | 3.152 s | 1.165 s | 1.162 s |
| Crauphel quickload wait | 22.077 s | 20.092 s | 20.054 s |

The two candidate routes both passed 35/35 steps, including the room 129
missile shot before and after quickload. Their protected profile hashes were
restored. Private receipts are in ignored
`temp/xl-pumo-s21-20261122/texture-profile-phone-route`,
`texture-batch-phone-route` and `texture-batch-repeat-phone-route`.
Windows D2 and all three Android ABIs built, and scoped code quality passed.
The test APK, archive, saves and logs remain out of Git.

The measured reduction is 1.96–1.98 seconds, about 17%, in the Crauphel
pre-briefing check, and 1.99–2.02 seconds in quickload. These timings are
from automated steering on this S21. A human report on the separate SM-S931U
and a wider set of levels remain open.
