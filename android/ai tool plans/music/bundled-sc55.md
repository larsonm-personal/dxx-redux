# Bundled SC-55 soundfont

Replace the bundled MIDI bank with the tested nitro-shoe SC-55 1.34 release.
Preserve AdLib as the renderer default and both reset presets. Move the existing
TimGM6mb bank into optional downloads with its full offline license and existing
Info/deletion behavior. Do not change saved custom soundfont selections.

1. Update the central source/hash pins, build download inputs and update checker
2. Update bundled/catalog metadata and retain attribution and license notices
3. Verify native rendering against the new asset, JVM download/storage behavior,
   installed catalog/Info, TimGM6mb download, and reset/preview/library behavior

Upstream declares CC BY 4.0 for SC-55, but credits Microsoft/Roland, Creative and
other borrowed samples. Those underlying rights have not been independently
verified; retain this distinction in the Info notice. TimGM6mb remains GPL-2,
available as an optional upstream download at the user's explicit request.

Completed: central pin and update checker now follow SC-55; Gradle tracks the
font URL/hash as inputs. The bundled entry is fixed and non-removable, and
TimGM6mb downloads retain the full existing GPL-2 notice. Existing selections
and AdLib/reset defaults are preserved.

Verification: all three Android ABIs build; 15 soundfont JVM tests and three
native rendering/loading contracts pass with the new bundled bank. Checked the
APK's actual SF2 SHA-256 and both license assets. On emulator-5554, catalog Info
passes 25/25 steps; actual TimGM6mb download, checksum, saved Info and MIDI
preview pass 22/22 steps. Profile integration passes persistence, both resets,
bundled preview and confirmed deletion/fallback (19/19 library UI steps).
Artifacts: `temp/bundled-sc55/`. Updated APK installed on emulator-5554.
