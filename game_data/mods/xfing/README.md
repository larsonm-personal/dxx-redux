# Xfing plain texture DXA conversion

This workspace converts the Xfing/UUD D1 and D2 texture packs into plain texture DXA archives for DXX Redux/Rebirth installs.

Tracked files:

- `convert-xfing-minimal-dxa.ps1`
- `verify-xfing-minimal-dxa.ps1`
- `xfing_minimal_dxa_lib.ps1`

Ignored files under `dxx_tp/` include the source packs, temporary extraction data, and generated DXAs.

Default usage from the repo root:

```powershell
.\game_data\mods\xfing\convert-xfing-minimal-dxa.ps1 -Game both
.\game_data\mods\xfing\verify-xfing-minimal-dxa.ps1
```

The default output directory is `game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/`.

D2 conversion also needs Pumo's original 2015 D1 palette for D2 as a baseline. Download `D2-DESCENT1PIG.zip` from [Pumo's announcement](https://www.descentbb.net/viewtopic.php?t=21897) and extract its `DESCENT.PIG` into `dxx_tp/baseline-d2-descent/original-2015/`, or pass `-D2DescentBaselinePig`. The original ZIP SHA-256 is `91ff279ae2949e6656a0e741d77dd21032bf0d1a41552d237a0cb70f6c95e9a7`; the PIG SHA-256 is `58868c54cf32c83a411f008cba7753d879217fb1aae5a51527695e156c4560e5`. The later Dropbox download currently contains the already-patched UUD2 PIG and is not a baseline

The seventh palette is optional game content: its conversion source is required when building a complete pack, but players do not need it to play the six stock palettes. Missions using `descent.256` still need their matching custom palette/PIG assets

Both packs are included in the [recommended mods release](https://github.com/larsonm-personal/dxx-redux/releases/tag/texture-packs-v1) and the launcher's recommended downloads. Put UUD above high-res texture packs to retain its corrected artwork; putting high-res first can cover the visible UUD images while its indexed and semantic fixes remain active. The updated Android engine is required for the indexed, level-surface and animation fixes; mounting the archive in an older engine does not establish support

See [AUDIT.md](AUDIT.md) for source-to-output, real gameplay and release-download evidence. Publication is repeatable with `python game_data/mods/d2x-xl/prepare_texture_release.py --publish` from the repository root
