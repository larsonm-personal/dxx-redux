# D2 briefing FM asset provenance

The earlier statement that D2 briefing has no FM arrangement was too broad.
It describes the tested retail archive, not the song across releases.

Compared local DOS demo, retail CD, OEM, Vertigo, Mac and GOG HOG archives.
Raw per-track note/device counts and archive paths are recorded in
`temp/d2-briefing-origin/report.json`.

- The pinned retail HOG is the European Definitive Collection disc 2 archive,
  SHA-256 `f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703`
- Its `briefing.hmp` is 23,634 bytes, SHA-256
  `310a9ae37e54d0a603cee7d2a76d1b6aa13444514d72f9844a41771fdc5c5684`
  This exact file also appears in the checked original retail CDs, GOG
  archives and DOS demo. Tracks selected for FM contain no note-ons
- The DOS 3-Level Interactive Preview additionally contains `briefing.hmq`,
  34,360 bytes, SHA-256
  `b69ba4082dca80fd6bb966b613d34b980c0d2a250be9a351aa85f4b33d47b022`
  It contains 1,520 FM-selected note-ons. The checked retail/GOG archives
  omit this companion file
- The demo and pinned retail `d2melod.bnk` and `d2drums.bnk` are byte-identical
- The current native renderer, given the demo HOG and `briefing.hmp`, selects
  `briefing.hmq` and successfully renders with ymfm. Given the retail HOG,
  it falls back to FluidSynth. Both ten-second host renders completed

This is an asset availability difference, not evidence that FM hardware cannot
play the song, nor that our converter discarded an existing FM arrangement.
The comparisons establish which files shipped in these archives; they do not
establish the developers' reason for omitting the HMQ from retail packaging.
No imported game assets or playback behavior were changed during this check.
