# UUD2SP support and recommended release

- [x] Verify original DXA, existing semantic conversion and UUD2TP composition against author sources
- [x] Finish repeatable packaging and any sound-bank priority/use reporting needed for our build
- [x] Verify real engine loading, rates, playback, priority and reload behavior with original sound payloads
- [x] Add the verified pack to the repeatable release and recommended popup, with credits and concise description
- [x] Run scoped quality checks and record results and remaining limits

Keep the original sound banks rather than duplicating their PCM payloads or rebuilding unrelated texture packs
Vanilla compatibility is a source-level assessment unless explicitly exercised in a vanilla executable

Completed: 208 original samples at 11/22 kHz, 11 D2 native cases, five launcher import/re-entry scenarios, ten mod-manager unit tests, scoped formatting and both test catalogs passed
UUD2TP composition now merges changed row fields rather than concatenating replacements that can restore stock sound assignments
Published `uud2sp-sounds.dxa` is 6,986,269 bytes; a downloaded copy matches SHA-256 `951f181efdc53df186ed2b6d8bb30d18664e04d7957bfedb8bacb00753822b4f`
An unchanged conversion preserves bytes and mtime, including after normalizing script line endings; repeat publication verified 16 unchanged assets
Evidence and repeatable commands are in `game_data/mods/xfing/d2x_sp/README.md`
No APK was published: the popup, sound-bank usage/priority and composition fixes require an updated build
