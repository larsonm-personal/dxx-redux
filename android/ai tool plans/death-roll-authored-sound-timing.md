# Death-roll authored sound timing

## Approved policy

D2 simulation uses original high-memory, non-low-memory sound identity and 22 kHz bank metadata, independently of playback settings. Archive priority wins before rate preference; a higher-priority authored 11 kHz bank must beat lower-priority stock 22 kHz data. Public demo embedded 11 kHz metadata and installations with only an 11 kHz bank remain supported. Loose authored PCM duration is captured before backend resampling, including odd sample counts. Preserve original fixed-point division and strict early/late cadence formulas

Original source at videogamepreservation/descent2, revision 75ba13c44259f1e40c0df70961c78abba898eafe: AI2.C lines2181-2197 derives death-roll cadence from sound duration; INFERNO.C lines403-406 selected playback rate based on memory and flags; BMREAD.C line323 selected separately stored RAW/R22 sources

## Implementation

- D2 runtime sidecars hold base and optional overlay durations without changing digi_sound layout or disk formats
- Canonical bank order, names and count are independent of the playback registry because HAM sound references are numeric; differently ordered or shorter playback banks cannot change semantic identity
- Sound registration initializes authored timing; retail bank headers are read in canonical source order without loading duplicate PCM
- The canonical reader validates header and payload bounds and fixed-point representability; unavailable or invalid metadata keeps the original one-second fallback, while a valid zero length remains zero
- The dying-frame consumer resolves ordinary Sounds identity, independent of AltSounds playback substitutions
- Shared PCM loading selects semantic override priority separately from playback priority, and records duration even for low-memory omitted samples
- Metadata-only lookup uses canonical names and handles canonical-only slots without replacing or owning playback PCM
- Overlay reset and registry reset clear the corresponding metadata; normal mission source-change detection includes canonical source filename and origin
- No audio device is required: initialization belongs to resource loading rather than playback startup
- Native D1 and imported-D1 fixed historical boss timing are untouched; no RNG classification, replay/save format, FX serialization, or PR46 signature changes

## Validation

- Durable host test: canonical bank/rate and lowmem matrix, actual dying-frame branch, original odd-length rounding, valid zero and invalid fallback, overlay reset, registry reset, 11 kHz-only fallback, malformed canonical headers, normal mission HOG mount/unmount with unchanged 11 kHz playback origin
- Additional paired-bank controls deliberately reorder the playback names and omit its second sample; canonical numeric identities and durations remain unchanged
- Existing embedded-HAM reload test also checks public-demo duration without retail banks
- Durable mod_sounds fixture: actual odd-length PCM loader across both rates/backends and omitted/non-omitted samples, then exact strict dying-frame boundary and total RNG-call assertions, followed by metadata restoration
- Lightweight host composition runs the same durable mod fixture function against the actual production loader and dying-frame implementation. Its unchanged-AI negative control fails the boundary assertion
- Separate process matrix with PR44 composition retains identical duration65538, six SIM draws and final SIM state4265552588 for both rates/backends
- Shared loader compiles for both D1 and D2

Validation reuses existing engine objects to avoid a large build in the disk-constrained workspace. It is not a fresh full current-tip build, Android-device run or real-mission gameplay reproduction. No publication until independent review and final verification
