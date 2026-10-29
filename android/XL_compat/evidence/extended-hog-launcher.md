# Extended HOG launcher integration

## Scope

The engine index, launcher JNI and C music catalog now use `hog_format.h` for
DHF/D2X signatures, signed member sizes, per-record long names and filename
validation. The small JNI library does not depend on either game engine. Kotlin
receives headers and bounded payload streams, with no whole-HOG byte-array
bridge. Payload consumers retain independent entry/count/expansion budgets.

Mission import, asset catalogs, HOG metadata, soundtrack discovery and music
staging now use that reader. Both signatures accept mixed short/long records.
Selected long names are not aliases for short header names. Duplicate names,
malformed trailing records and truncated payloads cannot publish an imported
mission or a staged preview. C music lookup validates the entire archive too.
Nested DXA/HOG discovery streams its payload instead of retaining a second full
archive in memory. Selected MIDI data still has a 64 MiB limit.

The blanket extended-HOG import refusal is removed. A valid D2X container report
allows further analysis while keeping gameplay unknown and `playable: false`.
A signature with malformed member bounds still reports a blocking error.
Contained level versions, features and resource closure retain independent gates.

## Original Pumo distribution

The optional `ExtendedHogOriginalPackageTest`, selected with
`-PxlOriginalPackage=<path-to-pmines_v11.7z>`, passed without skipping. It uses the
unchanged 442181363-byte download through production import and catalog APIs.
The extracted bundle is 1677820774 bytes. It verifies 205 HOG members and all 14
descriptor level names, catalogs 303 playable-format music entries, stages an
original compressed track with identical bytes, and removes its preview cache.
Cataloging these tracks does not establish mission-specific playlist selection
or playback of every track. Original-package evidence here is host-side, not an
Android import of the complete Pumo distribution.

- Distribution SHA256: 82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf
- 196505200-byte HOG SHA256: 6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043

All fourteen Pumo levels still reject level version 27. This increment does not
decode their gameplay, establish a complete requirement inventory or qualify
the campaign. Version-25/27 layout and geometry work is the next dependency.

## Controls

Public JVM controls cover both signatures, 1/12/13/14/254/255-byte names,
one-byte reads, empty members, every partial truncation, invalid names/sizes,
case-folded duplicates, budgets, stream ownership and input failures. A generated
196505200-byte payload verifies bounded streaming without allocating that whole
payload. Import integration covers exact resource bytes, catalog/staging reuse,
cleanup and failed-publication isolation. The optional original-package test is
separate from these public fixtures.

The normal-APK runner `test_xl_hog_launcher.ps1` imports a derived ZIP through
the launcher, checks HOG metadata, compares JNI/C MIDI bytes, starts both the
native MIDI and Android compressed-audio preview paths, verifies cache cleanup,
and starts its uniquely named XL mission. The engine checks the long-name
resource bytes and mission ownership. It restores protected player, mission and
configuration files and compares their hashes before reporting success.

The fixture preserves every original Neptune member payload, adds deterministic
MIDI/WAV entries and uses a distinct xhogi descriptor to avoid selecting the
previous native test's xhogs mission. It is a derived container control, not
original Neptune or Pumo campaign qualification.

- Derived HOG SHA256: 950a734f22acd0f8cf3ac8ca4e20d4f5856c11967b6759d4a269b69ba798bc1a
- MIDI SHA256: d89ca207a031da8c90fb3d2c1a84ed251ad377f3a61f275a9c86fc5c53d99126
- WAV SHA256: 5ba66f0b9a8dd400b3037d65e5febd0620a46b9f03b9f913e00e02fca7830685

The source oracle still compares 144 records with the pinned original XL reader.
The rebuilt native engine passes archive lookup, byte identity, independent
streams, malformed isolation and disk restoration through stock Counterstrike.
The C music catalog passes MSVC and strict GCC/UBSan controls, including read
lifetime. The full launcher JVM suite finishes successfully with 1238 tests,
zero failures/errors and seven skips. The optional original-Pumo test passed
separately with zero skips. Focused native header/source/inventory contracts pass
3/3 in 12.21 seconds; rebuilt engine container lifecycle passes in 3.73 seconds.

The final normal APK passes all 17 Android steps in 42787 ms, run
`6ace63bef76442f582a0375a88b87c2e`. All 27 protected file hashes match after
restoration. APK SHA256:
`d37e7b53fbcd0d2cd70be216c3bcffe693df7b888244810803b1c04ce198f91b`.

An earlier attempt selected the old normal app after building a separate CI
package; it is not counted. A subsequent functional pass lost adb during
cleanup. Its backup was restored and all protected files verified; the runner
now uses fresh per-run result directories and reports success only after
restoration. The final run above completed without recovery.
