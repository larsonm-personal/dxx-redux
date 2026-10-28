# XL 15/34 format

The reader and bounded translator now understand level 15 with game-data 34.
This combines the existing level-15 geometry and double texture colors with
packed light indices. Source detection drives admission; mission names do not.

## Source boundaries

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
LoadGameData enables bD2XLights for XL levels at game-data version 34.
CLightDeltaIndex::Read packs side into three bits and count into thirteen bits;
its final index is unsigned 16-bit storage. ReadGameFileInfo adds the equipment
header only above level 15. Legacy object-trigger owner heads remain the fixed
700-entry table below game-data 36. CObject::Read has no field-layout transition
between versions 33 and 34. ReadExtras and the extracted ambient flag traversal
retain reserved flags without granting gameplay behavior.

Pinned source SHA-256:

- lighting/light.cpp: ca95b9183729c7f3665ef0706206afce093b53b0d97aa6ba11379dd5fdf5f36f
- gameio/loadobjects.cpp: 01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c
- objects/objectio.cpp: 528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f
- include/segment.h: 4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667

The independent storage adapter compiles the actual CLightDeltaIndex::Read body.
131,072 raw bit patterns exercise the source's old and packed representations;
136 production cases cover both encodings, counts through 8,191, valid/invalid
sides, with separate truncated and overrun cases. All 256 reserved flag bytes
are retained for 15/34; unaudited 15/33 flag cases stay blocked. Positive and
malformed object-trigger owner/link controls also include the 15/34 layout.

## Original evidence and retained failures

The unchanged Apocalyptic Factor XL HOG SHA-256 is
f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db.
The extracted reader evaluates all 676 original light indices: fourteen levels
contain invalid packed ranges. Their bytes resemble older side/count encoding,
but the pinned reference selects packed decoding for their declared version.
The port rejects these ranges with the segment, side, count, first index and
available delta count. It does not silently reinterpret malformed originals.

Alarlof alone decodes. Its original HXM fails existing definition validation,
and the native integration explicitly requires that rejection before starting
the derived fixture. [The candidate report](format-15-34-candidates.json) records
decoding only, with every runtime_qualified value false. All fifteen original
campaign levels therefore retain independent blockers.

## Derived native controls

The fixture removes exactly alarlof.HXM from a copy of the original archive and
uses a single-level descriptor. Alarlof's RL2 bytes remain unchanged; stock robot
definitions replace its unqualified HXM only in this labeled test fixture.

- Derived format34.hog: c7d8f979382f162fc17d4c13976b234ba2dc5f9dbd55b5bde6314230f229a378
- Derived format34.mn2: 8cc94a3d53ce1e251a26d9fa4b2c17e18b595b2dbe60dc1896503271463af5b5
- Original alarlof.rl2: df172e675c3754db2401ba8cbbf7e8747732666d3682918843a7e05d58d1a7e5

Native checks compare all 2,577 vertices, all 803 segments' connectivity and
reserved flags, the counts of 202 walls and 16 triggers, packed-index chunks
and all 5,083 deltas. All 68 light sources change actual UVLs;
ordered off/on application, clamping and repeated-action behavior match expected
values. 6,310 colored corners change through the existing scalar color path.
Disk restoration through stock preserves light masks, UVLs and earned player
resources, and restored lights remain operable. These checks do not establish
full graphical parity or original mission gameplay.

## Android and final checks

The registered Android runner passes all 48 steps on emulator-5554 in 99,957 ms,
run 15ed5158ffb8447e9d26ba627b077211. It uses authored light-off and one-shot
light-on actions, then verifies disk/rewind and stock restoration. The
first script incorrectly attempted to reuse the consumed one-shot action; its
failure was a test assumption, and the revised sequence restores a save made
before consuming it. No production trigger semantics were changed.

Final APK SHA-256:
d345bb1336ca63de471f4bd36a60f12516d2b62e15c4ea183ca3e89b8f062393.
Protected pilot/config/save hashes match after both attempts. Emulator-5556 was
untouched. Introspection reports actual source format versions and per-source
light masks, without changing game state.

All twelve focused native regressions pass in 52.31 seconds, including positive
15/34 object-trigger decoding against the extracted reference, existing Neptune
and Observatory formats, both repair profiles and authored close doors. MSVC
and GCC -Wall -Werror source-oracle builds pass. All twelve private-policy tests
pass; the full private runner now requires 70 core tests, not all run here.
Identical original-audit regeneration preserves canonical bytes and mtime.

Both catalogs pass: 407 top-level entries, 463 support scripts, 99 standalone
JSON tests and 294 standalone PowerShell tests. Scoped mixed-language quality
and explicit pinned PowerShell analysis pass. Original assets remain hash-pinned
and unchanged; only the separately named derived fixture omits the rejected HXM.
