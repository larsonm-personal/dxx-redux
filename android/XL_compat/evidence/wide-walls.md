# Wide wall identities

XL levels can now retain up to 2047 walls through the ordinary validated loader.
Admission is feature-based. Stock levels and imported D1 saves retain their
255-wall cap. No original campaign becomes qualified from this change alone

## Source and representation

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- main/side.cpp: 5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989
- include/wall.h: 08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd

The extracted CSide::ReadWallNum uses unsigned shorts from level version 13,
bytes before that, and 65535 for no wall. The source maximum is 2047. Eighty
width/identity/sentinel cases pass under MSVC and GCC with strict warnings

Redux already stores live side, active-door, cloaking and stuck-object wall
identities in shorts. Physical wall storage grows without changing the packed
wall/save record. The native adapter uses private compiled-mine tag 0x58 and
16-bit wall references only above 255 walls. Smaller tables retain the existing
byte encoding. This private adapter is not a public level dialect or a portable
stock level. The loader accepts it only during the validated XL load, then
clears that permission on success, failure and mission cleanup

Save restoration checks negative and over-capacity wall counts before reading
the array. Cloak demo event 53 carries short identities starting at ID 255;
classic event 44 and its byte payload remain unchanged for IDs 0 through 254.
Recording, playback, rewriting and invalid-identity controls cover both forms.
This is a specific demo consumer correction, not complete XL demo qualification

## Native and Android controls

`xl_inventory_contract` covers wall counts 1, 254, 255, 256, 257 and 2047,
adapter widths and identities, 2048-wall rejection and invalid references

`xl_wide_walls` derives a fixture from original Neptune geometry and the existing
close-door fixture. It shifts authored walls to IDs 2017/2018 in a 2047-row
table; added rows are unreferenced normal walls. It verifies:

- Normal startup, private load-context cleanup and high-ID side/door identity
- Authored opening, automatic waiting, closing, actual animation and collision/
  visibility rays, with active-door timer and identities preserved
- Earned shields/energy and a disk restore through stock; restored closing
- High-ID cloak activation and exact partial-animation restoration
- Actual demo writer, frame decoder and byte-identical rewrite at IDs 0, 254,
  255, 256 and 2017; negative/out-of-range front and back IDs cannot alter walls
- Negative and 2048 saved counts rejected before wall-array reads, followed by
  successful recovery from the valid save; stock independently rejects negative
  and 256 counts and recovers from its valid save
- Source over-capacity, invalid-reference and truncation failures without world
  mutation; a copied private mine marker rejects outside its owned load

The final native controls pass. Existing close-door, HXM and water-arena controls
also pass. The source comparator passes on MSVC and GCC

The registered `test_xl_wide_wall_runtime.ps1` owns its 80-step Android JSON
integration and a 600-second master timeout. Run
`3c7b3bd5ae2544c08669abbe3fe0429a` passed all 80 steps in 96341 ms on the final
APK, following the earlier 80-step pass. It covers
authored activation, obstruction from both sides, physical crossings, visibility,
audio, memory checkpoints and disk restoration through stock. Protected player,
configuration and mission-file hashes match before/after. The captured scene
renders; the screenshot is not a reference-rendering parity claim

Derived Android fixture hashes:

- HOG: 50687295e2f20c3cd910d56a6b1cb378b7998d0a7e87a7fd3bbfa1f8d08526d5
- Descriptor: 4d910c3847b73544164eb561c0acceb7da7caae8f8980de81dffe92945f9d8b6

Original Neptune HOG remains
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303

## Original survey reconciliation

[The original-member audit](wide-wall-originals.json) records all 21 high-wall
members from nine HOGs in levelpack.7z. Every RL2 SHA-256 matches the independent
user evidence bundle. Archive SHA-256:
b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719

Fresh production `xl_inspect` inspection fully decodes 17. Source inventory and
the production `runtime_blockers` function agree that wall capacity is no longer
their blocker; all 17 still have other blockers. Four remain rejected:

- speed.rl2 and control2.rl2: invalid reactor trigger count
- subzero.rl2: movement layout 255 at object 4
- control.rl2: invalid packed-light range

The JSON distinguishes inspection findings from runtime blockers. It is not an
asset-aware startup test or proof of gameplay. Effects, event variants, segment
behavior, other capacities and resource validation remain independent

## Remaining work

The source supports 100 concurrent cloaks while Redux currently supports 10.
That concurrency is not solved by widening wall identities and is not qualified
here. Trigger count/identity widths, object headroom and other tables also remain
unfinished. XL multiplayer remains gated. No mission-specific exception or
rewriting of original input was used to erase these requirements
