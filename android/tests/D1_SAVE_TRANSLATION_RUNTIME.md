# Native D1 phase/Fusion checkpoint translation

The `test_upstream_compat` CLI creates native D1 saves and then imports them into
D2's original-D1 mode. Run the two engines in separate empty working directories,
using the same public D1 shareware resource directory containing `descent.hog` and
`descent.pig`:

```sh
# In an empty native output directory
/path/to/d1/test_upstream_compat --d1-save-translation-runtime /path/to/d1-data .
# In an empty imported output directory
/path/to/d2/test_upstream_compat --d1-save-translation-runtime /path/to/d1-data /path/to/native-output
```

This covers the finalized native version21 eight-byte tail: bounded phase then
Fusion-active flag. It writes genuine native saves and opposite-endian variants,
then compares the initial player state and sixteen actual gameplay frames for
inactive, due-now, past, immediately upcoming, future, and inactive-but-charged
Fusion states. The phases 255 and 65535 exercise lower-byte carry and wrap. The
comparison includes phase, position, orientation, velocity, shields, energy,
Fusion charge/activity/projectile count, and both RNG streams.

Public shareware lacks Fusion projectile assets. These explicit synthetic
inventory scenarios use the public laser weapon definition for Fusion in both
engines. They exercise production weapon processing and native serialization;
they do not claim registered Fusion asset coverage.

The D2 consumer also checks rejection of Android20/22 and unknown versions,
every truncation within the final eight-byte tail, invalid phase and flag words
in both byte orders, and transactional rejection without changing objects,
phase, Fusion, or RNG. Native19 prefix controls verify legacy admission, clearing
of previously restored phase offsets, and unchanged unflagged deadline behavior.

Each process writes `d1-phase-fusion-translation.json`; the native producer also
writes each checkpoint, byte-swapped checkpoint, and uninterrupted frame oracle.
The fixture depends on the coordinated phase and explicit Fusion-active changes.
