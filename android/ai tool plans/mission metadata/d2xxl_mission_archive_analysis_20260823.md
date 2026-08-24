# D2X-XL mission archive continuation analysis

Date: 2026-08-23

Source: `C:\Users\first last\Desktop\d2x-xl levels`

This extends `sorting_notes.md` and `mission_sort_manifest.csv` from the
2026-08-16 first pass. The first pass correctly found the byte-identical and
repackaged duplicates, but conservatively retained every package whose HOG
differed. This pass compared standard HOG members, level payloads, HXM headers,
archive layouts, and generated native level metadata.

## Import format findings

The launcher supports ZIP, 7z, and RAR containers, but a mission HOG must use
the standard `DHF` HOG format. `MissionAssetCatalog.readHog()` rejects every
other HOG signature. The game-side mission path has the same practical
requirement.

### Importable standard HOG archives

All archives in the first-pass `vanilla-rebirth` directory except `panic.zip`
have a valid descriptor and a same-directory, same-stem `DHF` HOG containing
all referenced levels. The host native analyzer successfully processed the
focused near-duplicate samples tested below.

All eight archives in the first-pass `duplicates` directory also pass this
structural check, but they remain duplicates.

Two archives in the first-pass `d2x-xl` directory use standard HOGs:

- `D2-XL.7z`: valid `DHF` HOG and supported descriptor, 9.4 MiB unpacked;
  verified importing in its original downloaded form on the emulator
- `sphere-1.51.7z`: valid `DHF` HOG and descriptor pairing, but 692.4 MiB
  unpacked, of which 671.3 MiB is D2X-XL cache data. Its descriptor uses the
  D2X-XL-specific `d2x-name` field, which the current mission parser does not
  support, so its original archive currently ends at a generic import failure.

For admitted missions, the launcher retains the original archive, stages its
playable mission and supporting files internally, and ignores a generated
top-level `cache/` tree. Sphere descriptor support remains intentionally
separate from this cache handling.

### Not importable as missions without conversion

These archives contain HOGs beginning with the D2X-XL-specific `D2X` signature,
not `DHF`. The launcher cannot catalog the levels and the game cannot use them
as ordinary mission HOGs:

- `anthology.7z`
- `BelialSystemXL.7z`
- `boilpnt.7z`
- `dinter_multilevel-2.0.7z`
- `lor-xl.7z`
- `pmines_v11.7z`

All six original downloads were manually selected on the Android emulator and
displayed: `This level pack uses the D2X-XL extended HOG format, which DXX Redux
does not currently support`.

`pmines_v11.7z` is also 1.6 GiB unpacked and contains a 187.4 MiB extended HOG.
It is under the general 2 GiB extraction ceiling, but it will not be admitted
as a playable mission and its HOG exceeds the 64 MiB per-entry metadata staging
limit. Supporting these six requires a D2X extended-HOG converter, not merely a
ZIP/7z repack.

`panic.zip` contains only `Panic.gro`, a legacy ACE archive. It has no directly
visible descriptor or HOG and remains outside the current as-is import scope.

### As-is handling of generated cache data

Many D2X-XL downloads include generated top-level `cache/` trees. The source
archive is preserved unchanged, but the launcher now omits that tree from its
internal extracted launch bundle. Notable avoided extraction costs include:

- `revodrav.7z`: 91.2 MiB cache out of 93.7 MiB unpacked
- `vignettes.7z`: 73.1 MiB cache out of 78.6 MiB unpacked
- `diehard.7z`: 52.4 MiB cache out of 55.3 MiB unpacked
- `ironstar.7z`: 44.4 MiB cache out of 45.8 MiB unpacked
- `bahagad.7z`: 44.6 MiB cache out of 49.1 MiB unpacked
- `sphere-1.51.7z`: 671.3 MiB cache out of 692.4 MiB unpacked

Mission descriptors, HOGs, docs, music, and mod assets outside that generated
tree continue to be staged. Users do not need to alter or repack the download.

## Duplicate and variant classification

### Confirmed duplicates from the first pass

The first-pass duplicate calls remain correct:

- `af.7z` -> `af-d2x.zip`
- `entropy2.7z` -> `Entropy2.zip`
- `icerealm.7z` -> `icerealm.zip`
- `lostlvls-d1.7z` -> `D1Lost.ZIP`
- `lostlvls.7z` -> `Lostlvls.zip`
- `mandrill.7z` -> `Mandrill.zip`
- `odyssee.7z` -> `odyssee.zip`; both level payloads are exact, with only TXB
  and embedded packaging/tool differences
- `po.7z` -> `plutonionOutbreak.zip`

### Definite gameplay or mission-set variants worth retaining

- `bahagad.7z`: the descriptor for Bahagad Outbreak is byte-identical to the
  repository descriptor, but `world2s.rl2` and `world2s.hxm` differ. Native
  metadata finds 23 triggers in the incoming secret level versus 26 in the
  repository version. The package also adds `Bahagad (easy mode)`, an
  eight-level mission with changed level payloads. This is not a metadata-only
  duplicate.
- `diehard.7z`: all 19 levels differ and native metadata reports a changed
  object count in every level. It also adds per-level CLR data.
- `eaf.7z`: the five levels have the same high-level topology as `EAF.zip`, but
  every level and robot patch differs. Incoming HXM robot replacement counts
  are 22, 20, 25, 27, and 3, versus 43, 43, 43, 44, and no fifth HXM in the
  repository package. This can change gameplay.
- `eaf2.7z`: all ten level and HXM pairs differ. Incoming HXM replacement counts
  differ from the repository in nine of ten levels. This can change gameplay.
- `phobos-e.7z`: all six level payloads differ. It adds HXM files for both
  secret levels and changes HXM replacement counts in two regular levels.
- `ironblade.7z`: the same eight level names are present, but only one level is
  byte-identical. Native metadata finds a matcen difference in `hosthell.rdl`
  and a texture-use difference in `fadeout1.rdl`.
- `ironstar.7z`: the same eleven level names are present, but only one is
  byte-identical. Native metadata finds a trigger-count difference in
  `dropshf8.rdl`.
- `maximum.7z`: this is a variant of `descent_maximum_fixed.zip`, not a new
  campaign identity. It contains the same 30 campaign and 6 anarchy level
  names, but many incoming levels have different object counts. Preserve only
  if the older/non-fixed variant itself is wanted.
- `orionneb.7z`: this is an Orion variant. Five of eight levels are
  byte-identical to `Orion.ZIP`; native metadata finds a different secret count
  in `level04.rdl`.

### New mission payloads with no repository level match

- `D1-levelpack.7z` contains 17 missions. `SUPER_S` is an exact repository
  payload and `ERTHSTRK` has an exact level with only a three-byte TXB change.
  The other 15 one-level missions have no repository level hash match, so the
  package is useful after removing the two duplicates if desired.
- `harqyjia.7z`
- `norep.7z`
- `po2.7z` with three missions
- `saturn.7z`
- `tt.7z`
- `D2-XL.7z`
- `sphere-1.51.7z`

The six extended-HOG missions listed earlier also appear distinct, but they are
not usable until converted.

### Probable duplicate conversions rather than new gameplay

- `entropy.7z`: all five level payload hashes exactly match `Entropy.zip`.
  Descriptor, briefing, music, and other auxiliary members differ, but the
  levels are duplicates.
- `revodrav.7z`: all 30 level names match `revodrav.zip`; two are byte-identical
  and the native analyzer reports the same high-level topology for all 30.
  The other RDL bytes and cache differ, consistent with an editor/rendering
  conversion. Keep only if D2X lighting or presentation differences matter.
- `vignettes.7z`: 26 of 27 level payloads are byte-identical to
  `Vignettes.zip`. Only `level46.rdl` differs, and its native high-level metadata
  is unchanged. This is not a materially new mission set.

These three can move to a `probable-duplicates` bucket if the database is meant
to contain distinct layouts/gameplay rather than every historical port build.

## Verification performed

- Inventoried all 35 incoming ZIP/7z containers, including entry counts,
  unpacked sizes, maximum members, roots, caches, and nested archives
- Extracted and hashed 108 incoming descriptor/HOG payloads and 347 repository
  payloads
- Parsed every standard HOG member and compared level hashes independent of
  archive, descriptor, and HOG filenames
- Checked descriptor/HOG pairing and referenced-level presence for every
  standard incoming HOG
- Ran focused host native metadata analysis successfully for Bahagad, Diehard,
  EAF, EAF2, Entropy, Ironblade, Ironstar, Phobos-E, Revodrav, Vignettes,
  Maximum, and Orion Nebula
- Imported the original `bahagad.7z` on the Android emulator, discovered both
  contained campaigns, and completed metadata analysis for both
- Verified with JVM coverage that the original 7z remains stored unchanged,
  ordinary supporting files are staged, and generated top-level cache files
  are not extracted
- Removed all temporary hardlinks from `game_data/mission_files`; retained
  generated analysis artifacts only under `android/temp`

Standard-`DHF` ZIP/7z packages with supported descriptors can therefore be
handled as users find them on the web. The remaining format boundaries are the
six extended-`D2X` HOG packages, Sphere's D2X-XL-specific descriptor syntax,
and the ACE-wrapped Panic package; support for those would require
format-specific work intentionally deferred here.
