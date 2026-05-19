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