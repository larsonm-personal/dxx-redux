# UUD2SP Sound Fixes and Restorations

[Xfing's UUD2SP 1.4](https://descentbb.net/viewtopic.php?t=21386) restores robot and door sounds, including D1 effects, and corrects several sounds that played too fast at 22 kHz

The author's original `UUD2SP1_4.dxa` contains standard `descent2.ham`, `descent2.s11` and `descent2.s22` replacements, so its format is supported by vanilla Redux. This is a source-level compatibility assessment, not a vanilla executable acceptance test. Replacing the entire HAM conflicts with other HAM mods

Our `uud2sp-sounds.dxa` preserves both sound banks byte-for-byte and replaces the complete HAM with 307 tested field changes. It requires the Android Redux semantic patch loader and D2 retail v1.2 data. Door clip 9 timing follows Xfing's combined UUD2TP+SP version. The original RTF, source hashes and conversion details remain in the archive

Put UUD2SP above D2 High-Res Sounds to retain UUD2SP's effects. Reversing their order uses high-res PCM wherever a replacement exists; UUD2SP's extra sounds and assignments remain. UUD2TP can be placed above or below UUD2SP. Restart the game session after changing mods, then use Mod Details to inspect loaded and played sounds

From the repository root:

```powershell
./game_data/mods/xfing/d2x_sp/convert-uud2sp-ham-patch-dxa.ps1
./game_data/mods/xfing/d2x_sp/verify-uud2sp-ham-patch-dxa.ps1
./game_data/mods/xfing/d2x_sp/verify-uud2sp-uud2tp-ham-composition.ps1
python game_data/mods/d2x-xl/prepare_texture_release.py --publish
```

Conversion needs the ignored original archive, its extracted `uud2sp/` directory and the standard retail `DESCENT2.HAM` under `game_data_to_copy_to_emulator/temp/`. Parameters can override these paths. Repeating an unchanged conversion preserves archive bytes and modification time; publication replaces only changed assets at stable URLs

Runtime acceptance:

```powershell
./android/tests/test_android_mod_sounds.ps1 -Serial emulator-5586
./android/tests/test_uud2sp.ps1 -Serial emulator-5586
```

The native test loads all 208 samples at both rates and compares their bytes against the original author archive. It exercises logical door, robot and weapon events through the real SDL mixer with a dummy output device, both mod orders, reloads and disabling. The launcher test imports actual packs, composes UUD2TP with UUD2SP in both orders, exercises high-res PCM priority and re-enters a real mission

The sound-bank usage reporting, priority correction and combined-row patch fix require an updated APK; downloading a new DXA does not update the engine or launcher

Verified and [published](https://github.com/larsonm-personal/dxx-redux/releases/tag/texture-packs-v1) on 2026-10-11: 6,986,269 bytes, SHA-256 `951f181efdc53df186ed2b6d8bb30d18664e04d7957bfedb8bacb00753822b4f`

Acceptance evidence: `android/temp/uud2sp-native/` (11 D2 cases and the D1 inert case), `android/temp/uud2sp-launcher/` (five launcher scenarios after mission re-entry) and `android/temp/uud2sp-unit-final.log` (10 mod-manager tests). The original and converted HAM comparison covers all 307 changed fields; combined semantics match Xfing's combined release where the patches overlap
