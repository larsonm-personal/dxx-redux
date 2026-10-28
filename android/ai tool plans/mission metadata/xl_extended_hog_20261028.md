# Extended HOG container and resource foundation

## Goal and base

Continue from xl_more_37_flickers, e4123219c5dff041c4363126ea95ff089b254af6,
PR #182. Support source container semantics through a common native index and
the production filesystem, then connect the launcher import/catalog paths.
Container recognition must never qualify the contained gameplay automatically.

## Source audit

Pinned XL revision 2576a98b5de310e3ac7c9777df2677d1627b8bae accepts DHF and D2X
signatures. Each member starts with a 13-byte short name and signed 32-bit size.
A negative size adds a 256-byte long name and its magnitude is the payload size.
This decision is per member, including under DHF; short and long entries can mix.
Lookup uses the selected long name, not a second short-name alias.

The engine currently depends on the stock PhysFS HOG reader. Native metadata
also rejects D2X. Android independently reads HOG headers in asset catalogs,
metadata, import classification and music staging, and explicitly rejects D2X
imports. Updating just one signature check would leave these paths inconsistent.

## Dependency order and acceptance

1. Add one streaming native index implementation: bounded offsets, signed sizes,
   selected names, exact truncation checks and unambiguous case-folded names.
   Reuse it for native inspection and a read-only PhysFS archiver. Preserve
   ordinary DHF behavior and the owning archive identity used by resource checks.
2. Exercise real mounts, lookup, seek/read/duplicate, enumeration, resource
   ownership, malformed rejection, unmount/reload and stock transitions. Compare
   selected names, sizes and offsets with the pinned source reader, and inspect
   hash-matched original extended containers without promoting their gameplay.
3. Route Android import, metadata and music consumers through the same native
   format definitions. Remove the blanket import rejection only when the complete
   launcher path is verified. Until then, report native container support and
   launcher support separately rather than claiming a usable import path.
4. Run native and normal-APK integration, stock isolation and relevant tooling
   checks. Record evidence and remaining work; publish reviewable dependency
   steps without substituting parser success for mission acceptance.

No autonomous level navigation. Multiplayer, contained unsupported gameplay and
resource formats retain independent detection and helpful rejection.

## Status

Native index, filesystem integration, source comparisons and native lifecycle
checks are implemented. All 77 configured XL tests and all 50 Android steps pass.
Android registration needed the D2 identity define on the misc library; the final
normal APK verifies it. Launcher integration remains the next dependency step and its gate
is retained. See [container evidence](../../XL_compat/evidence/extended-hog.md).

## Follow-up distribution findings

The separate original-distribution audit in
[extended-hog-distributions.json](../../XL_compat/evidence/extended-hog-distributions.json)
changes the next acceptance fixtures. Use the 196505200-byte Pumo Mines HOG to
verify streaming catalog/import/music behavior, retaining its version-27 gameplay
rejection. Anthology adds a version-25 reader requirement; Boiling Point, Lost
Relics and Pumo Mines add 21 version-27 levels. These formats need source-reader
and boundary controls after the container/launcher foundation.

Belial has conflicting duplicate briefing names. Audit pinned CHogFile::QuickSort
and BinSearch to determine the selected payload before replacing the current
explicit ambiguity rejection. Identical duplicates alone are insufficient: its
five belials.txb entries contain three different payloads.

Launcher consumers to connect: MissionAssetCatalog.readHog,
GameFileMetadata.summarizeHog, MissionZipMusic's HOG catalog,
MissionZipMusicStageManager.readFromHog/extractFromHog,
ModManager.hogHasSoundtrack and native hog_midi_catalog. MissionZip and
SetupActivity's blanket diagnostics should change only after those consumers and
actual package import are verified. Keep format interpretation native, expose a
stream/header or file-descriptor interface, and preserve extraction budgets,
member ownership, case rules and cleanup. Do not duplicate the binary format in
another Kotlin reader.
