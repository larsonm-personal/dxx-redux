# Managed mission and MIDI editor fixture continuation, 2026-10-09

Diagnosis only. GQ2-CHUNK-0053 covers complete226-line MissionScannerManagedArchiveTest and84-line MusicEditorArchiveSourcesTest. Preserve existing owner GQR0102/GQF0115; no new finding or implementation status change.

## Source generation and future acceptance

- [ ] Carry one immutable admitted wrapper generation through MissionScanner policy/descriptor scan, ModManager identity, host authorization and actual served stream. Current scan and hash reopen independently; ensureMissionContentIdentity reuses persisted identity on size/mtime equality. hostArchiveAllowed reinspects bytes at start and serve, but each later stream is a separate open. Coordinate existing extraction/publication/reader lifetime BR0103 and launch-generation plan0043 rather than add a second snapshot framework.
- [ ] Retain changed-mtime same-size test; add ordinary small unchanged-size/unchanged-mtime replacement and barriered replacement between scan/hash/policy/open. Require a complete consistent admitted generation or typed rejection, deterministic retry and no prior-generation loss. No unconditional repeated main-thread full hashing; coordinate GQR0103 bounded dispatcher/read-count acceptance.
- [ ] Independently verify whole and every chunk digest, chunk boundaries/count and persisted/reloaded identity for the actual admitted bytes. Current fixture checks presence and equality between application results, not an independent expected digest. Keep synthetic one-byte RL2 admission fixtures; actual native gameplay and host/client transfer require their own integration evidence.
- [ ] Preserve whole-wrapper proprietary-name and opaque-container rejection, including unselected renamed HOG members and forged requirements. Preserve coop/anarchy filtering, enabled state and immutable catalog-list snapshot behavior. These do not prove the backing file generation immutable or arbitrary renamed proprietary payload detection.

## MIDI selection and consumer acceptance

- [ ] Keep exact minimal MIDI byte comparison, MIDI-only routing, builtin D1/D2 ordering and stable preferred source ID. Add only missing owner-level cases: nested/extracted inputs, absent/invalid source fallback and actual imported-source loader routing where necessary.
- [ ] Bind catalog selection, playback bytes and separately loaded metadata to one admitted source generation under GQR0102. MusicPickerPage715/717 currently calls readMidiTrackBytes separately. The direct MIDI API returns owned ByteArray; do not misapply compressed-audio File lease diagnosis to that return value.
- [ ] Preserve existing selected MIDI64MiB/bounded-read repair; complete existing GQR0106 skipped streaming descriptor accounting without duplicate root or reopening selected-path repair. Actual synth/metadata and Android UI acceptance are separate from byte-selection assertions.
- [ ] Preserve existing GQR0107 compressed-audio generation leases across async open/playback/cleanup; this assigned MIDI test never exercises compressed staging or generation eviction.

Only future implementation acceptance is described. No JVM/native/device/network/media/fault/security/allocation/resource probes run here; historical results remain historical. Both assigned sources equal frozen head; zero inherited D1/D2 saving.
