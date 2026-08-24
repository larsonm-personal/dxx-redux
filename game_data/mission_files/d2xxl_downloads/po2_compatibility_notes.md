# Plutonian Outbreak 2 compatibility notes

This is a handoff for `po2.7z`. The launcher should continue accepting the archive as downloaded. Do not repack it or rename its internal files merely to make them look like a conventional Rebirth mission.

# original webpage
* https://web.archive.org/web/20010412075344/http://www.xtremebuilder.com/po2.shtml
  * lists mod changes, many of which needed its custom executable

## Bottom line

PO2 combines ordinary Descent 2 mission assets with data intended for its custom executable. Rebirth can already use much of the artwork, briefings, levels, POG textures, and HXM models, but it does not currently reproduce the total conversion's custom gameplay. D2X-XL appears to have added several targeted PO2 fixes rather than generic support for every original feature.

The launcher can expose the top-level DOCX readme and preserve the nested mod files. Its generated `descent.sng` alias helps locate the music without changing the archive. The PMH and PTI files must remain unapplied until there are safe, explicit loaders for them.

## What is in the archive

- `pluton2.mn2` defines eight regular levels and one secret level. Two additional one-level missions are also present, so the archive contains 11 maps and does not exactly match the advertised ten-level count.
- Standard POG, HXM, PCX, TXB, and CTB assets carry much of the custom visual and briefing content.
- HXM files for levels 1 through 6 replace the player model with four distinct payloads, matching the advertised four ships visually.
- `mods/pluton2/pluton2.pmh` is a complete version 3 `HAM!` data set, not an XHAM extension. Compared with stock D2, it has 67 robots, 84 weapons, 49 powerups, and 193 models. Many stock robot and weapon records are changed.
- `pluton2.pti` is a TXB-encoded replacement game-text table. It includes ten renamed primary and ten renamed secondary weapon slots. It was input for the custom executable patch, not a normal D2X runtime file.
- The S11 and S22 banks have the stock count of 183 sounds, but 43 sound payloads differ, including long environmental loops.
- HMQ music files use the same `HMIMIDIP` container as HMP. Rebirth currently filters them by extension, leaving levels 2, 4, and 6 without their archive-only custom tracks and selecting the smaller HMP variant for level 8.
- `.clr` files are D2X-XL texture-light color data. Rebirth currently ignores them. Bundled generated lightmap or mesh caches are not required for basic mission loading.

## Feature assessment

- Custom textures, screens, briefings, cockpit art, and many model changes should work through standard formats. Empty-face submodels and unusual door backing textures need runtime coverage because D2X-XL records PO2-specific fixes for both.
- Custom robot AI, weapons, strengths, sounds, and related behavior live mainly in the currently ignored PMH, so present Rebirth behavior falls back to stock data.
- The four ship models are evidenced in HXM files, but PMH has only one player-ship physics record. Different ship handling was probably hardcoded in the custom executable.
- The marketing claim of 25 weapons does not mean 25 selectable slots. The decoded text table still exposes ten primary and ten secondary slots; extra HAM records include projectiles and child weapons.
- The archive has multiple objects that stock data treats as Guide-Bots, but PO2 removes the standard companion flag. Rebirth has a single global buddy and escort state, so real multi-wingman and coop support requires an engine redesign.
- Ordinary level triggers should work. The advertised objective system, including a live objective bar, has no script or configuration in the archive and was likely hardcoded.
- The original launcher, CD player, and multiplayer security are packaging or network features, not mission content, and do not warrant PO2-specific engine emulation.

## Important safety warning

Do not alias or rename `pluton2.pmh` to `descent2.ham`. PO2 contains 84 weapon records while Rebirth's current `MAX_WEAPON_TYPES` is 70. Loading it through the full-HAM path without validating every count can overrun fixed tables. A diagnostic direct-load attempt stalled and was terminated.

## Recommended implementation order

1. Accept HMQ as HMP-compatible music and load mission-scoped S11/S22 sound banks.
2. Build a validated mission-scoped full-HAM loader with complete stock-data rollback. Raise the weapon limit to at least 84 only after auditing every fixed-size consumer, object ID, save/demo format, and multiplayer serialization path.
3. Add a mission-local PTI text provider for weapon and HUD names without replacing application-wide localization.
4. Regression-test every PO2 level with PMH, HXM, POG, music, and sound active, including transitions back to stock missions.
5. Find and compare the original patched executable or source diff before attempting objectives, per-ship mechanics, or wingmen. Express recovered behavior as generic mission data or scripting, not filename-specific logic.
  a. AI coding tools are getting better and better at automatic decompilation. comparing to a d2 executable of that era or a very early d2x executable probably makes the most sense
6. Consider `.clr` colored lighting later as an optional rendering enhancement.

## D2X-XL references

- [D2X-XL history, version 1.15.149](https://www.descent2.de/d2x-history.html#v1.15.149)
- [D2X-XL history, version 1.18.67](https://www.descent2.de/d2x-history.html#v1.18.67)
- [D2X-XL modding guide](https://www.descent2.de/d2x-modding.html)
