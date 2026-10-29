# Native extended HOG container foundation

## Implemented scope

D2 now mounts DHF and D2X containers through a shared native streaming index and
read-only PhysFS adapter. The same index drives native inventory inspection.
Each member independently selects its short name or the 256-byte long-name field
using the signed size. Long entries under DHF are supported too; the short name
is not an alias. Ordinary DHF remains supported and D1 retains its original
archiver. Filesystem registration happens before mounting any D2 archive.

The index reads headers without copying payloads and checks all extents against
the source length. It rejects truncated fields/payloads, INT_MIN sizes, ambiguous
case-folded names, empty/control names, path separators and unterminated long
names. The INT_MIN rejection avoids source signed-negation overflow. Names are
flat archive members, as in the currently admitted resource namespace; nested
resource paths and broader filename encodings remain separate work.

This is a native container milestone, not end-to-end Android import support.
The launcher's import, asset catalog, metadata and music readers still need
integration. Its existing extended-HOG import gate remains. Compatibility text
now distinguishes native support from that unfinished launcher path. Contained
levels and resources retain independent admission checks.

## Source and native evidence

Pinned source revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`io/hogfile.cpp` SHA256:
87919a369a1e9501ae3520a85c382f8114dfd54939639026a3ea3bd88eae1339.
The actual `CLevelHeader::Read` function is extracted by
`tests/reference_hog.cmake`. MSVC and strict GCC/UBSan match 144 member names,
signed sizes, modes and offsets. Strict GCC compilation of the production
PhysFS adapter also passes.

Public contracts cover 48 mixed archives across both signatures, name lengths
1/12/13/14/254/255, payload sizes 1/2/256/4096, empty short members, every partial
header/payload truncation, duplicate names and malformed names/sizes. Exact member
boundaries remain valid shorter archives. Native inspection and indexing agree
on payload bytes and selected names.

The real-engine test rebuilds Neptune's container without changing any original
member payload. It checks both D2X and DHF long-name variants, a long auxiliary
resource, case-insensitive lookup, independent stream cursors, bounded seek/EOF,
enumeration, rejection of unmount while files are open and cleanup afterward.
Every mounted original member matches its original bytes. Ordinary mission
startup and disk restoration through Counterstrike retain source identity,
earned lighting and owning archive identity. Malformed archives cannot replace
the live mission's resource ownership.

All 77 configured XL tests pass in 394.31 seconds. The final D2 stock control
passes in 48.49 seconds. Final public index, source-oracle and inventory controls
pass together in 18.72 seconds. Both automation catalogs pass with 417 top-level entries, 473 support
scripts, 99 standalone JSON tests and 304 standalone PowerShell tests. All twelve
private-suite policy tests pass in the configured Linux environment. An initial
Windows invocation lacked CMake on PATH and is not counted as a passing run.

## Android acceptance

The normal APK test stages a protected, pinned fixture directly into the test
mission directory. It does not exercise or claim launcher import. The fixture
checks actual long-name resource bytes and owning archive identity, state
disturbance, memory checkpoints, disk restoration through stock and cleanup.

The first Android attempt exposed missing D2 registration: filesystem setup is
compiled in the misc library, which lacked the game target's D2 identity define.
The library now carries that identity. The startup diagnostic identifies the
selected HOG reader, and mission mount errors include PhysFS's failure reason.

The final registered Android runner passes all 50 steps in 85132 ms, run
6f3d55d496674e2daeea1c4c841efddb. Protected player/configuration/mission file
hashes match after cleanup. The APK SHA256 is
e0988562302190f3922f0edf5a154ebaaca3ffeef38365057bb8039f93017f33.

- Original Neptune HOG: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived xhogs.hog: 1b8b75b829a865191b3dd6686d3eababfd9de692464a1a5dcc5b1410e13bc6c0
- Derived xhogs.mn2: a4a1be94729745d47fb34b5b83beeff85ed4379f3126c30556a2351ea611f589

## Original container reconciliation and remaining work

[extended-hog-originals.json](extended-hog-originals.json) records the D2X
container in the pinned 131-level survey: Playground's playgdd3.hog. Its archive
and surveyed level hashes match. All five members are indexed and playgdxl.rl2
now decodes; effects and object events retain runtime blockers. No gameplay or
campaign qualification is granted. Other separately distributed extended
archives have not been covered by this survey result.

Next, connect the launcher readers to the native format definitions and verify
real package import, metadata, music staging and resource cleanup before removing
the import gate. Long level/mission identifiers, nested resource namespaces,
remaining resource formats and contained gameplay requirements are distinct
unfinished work. No autonomous navigation was used.
