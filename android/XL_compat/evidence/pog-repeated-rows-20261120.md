# Repeated bitmap replacements in original POG directories

Original XL LoadReplacementBitmaps in gameio/loadtextures.cpp at 2576a98b5de310e3ac7c9777df2677d1627b8bae reads the directory count, then visits each record in order and installs its image at the record's bitmap index. Dogfight has 8,191 rows targeting 71 distinct slots, QToxic has 4,184 rows targeting 72, and Toxic-D3 has 4,640 rows targeting 74. Each has a nonzero width for every row.

The decoder now bounds directory allocation independently of the registry size. Every record retains its own target, dimensions, encoding, span and pixel-budget checks. The unit control uses two different pixel values targeting the same slot in a registry of size two and checks both replacements in order.

| Original census case                                        | Windows and Linux ordinary result                                                             |
| ----------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| levelpack.7z/missions/diedel/dogfight.mn2:1                 | Loaded and ticked 120 unassisted frames                                                       |
| levelpack.7z/missions/diedel/qtoxic.mn2:1                   | Loaded and ticked 120 unassisted frames                                                       |
| levels_multiplayer/toxic.7z/missions/diedel/XL/qtoxic.mn2:1 | Loaded and ticked 120 unassisted frames                                                       |
| levelpack.7z/missions/diedel/toxic-d3.mn2:1                 | POG decoded. RGBA bitmap 2157 requires an animation clock without a qualified texture effect. |

Both native POG contracts passed. These runs measure startup. Multiplayer scoring and match completion require separate runs.
