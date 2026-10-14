# Recorded all-weapons qualification

These three format-5 compact recordings and their immutable live-derived oracles
were captured from clean source `7145f682f4dd946ce0a8019d087eba6523794c16`
(tree `913dcf6dea58cc505b95e5faafc385301621d342`), using pinned public shareware assets.
Every demo and oracle is at most 100000 uncompressed UTF-8 bytes.

Native D1 scourge, native D1 bigred, and native D2 honestbob each pass all 40
frame-state/diagnostic comparisons, the complete SIM RNG stream, and the terminal
result on headed and headless native backends. All 16 negative replay controls
retain the original oracle: wrong-profile rejection, D1 variant substitution,
and deletion of the final input-tail grant. Missing-tail controls preserve every
simulation frame while changing the terminal resource result.

The four `captures/*.json` files preserve the live six-phase inventory and actual
Spreadfire/Gauss projectile observations. Imported D1 is a live diagnostic only;
it is not admitted to compact replay or cross-engine parity. D1-profile grants
use 784 Vulcan ammo, including imported D1; native D2 uses 1568. Native bigred's
ordinary letter-bound bomb input remains in its recording.

`qualification.json` is the complete, unchanged runner report: 4 captures,
6 passing positive replays, 16 passing negative controls, precise source/build
identity, and hashes of all original raw evidence. Large raw traces remain
external qualification artifacts; no game assets, binaries, saves, or profiles
are distributed here. `manifest.json` binds every admitted evidence file to its
exact bytes and SHA-256. The report's `published: false` records its original
pre-admission state and is intentionally unchanged.

No retail assets were played and no Android device was tested. Recording-off
retail-mask policy checks do not establish retail-asset playback. This isolated
corpus does not modify the existing public-level replay corpus.

Offline validation: `python3 android/tests/test_check_all_weapons_runtime.py`.
Fresh native qualification: `android/tests/test_all_weapons_runtime.ps1`.
