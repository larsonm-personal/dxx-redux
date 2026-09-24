# Preview fallback and MIDI settings

Show a bold per-song notice when the user selected FM but the preview's actual
renderer is SF2. Keep the soundfont name in the existing playback status. Clear
the previous status before starting another playback attempt.

Audit music preferences in combined JSON exports/imports and both full presets.
Renderer, selected soundfont, reverb and chorus were already registered, and
both presets already reset effects to true/true through SoundfontStore. Exports
previously omitted preferences never explicitly saved: include effective MIDI
defaults so importing an untouched setup restores those choices as well.

Validate independent effect combinations and untouched-default exports in JVM
tests, rerun the existing soundfont persistence/reset tests, and build the APK.
FM precision/resampling and MIDI gain are fixed renderer choices, not additional
user preferences. Downloaded font files remain separate from JSON settings.

## Completed

- Added bold notice for selected FM / actual SF2, retaining the active font name
- Included all four MIDI defaults in exports even before first preference save
- Confirmed both full presets invoke the existing true/true effect reset
- All 24 targeted JVM tests passed (export/import, import loader, soundfont store)
- Scoped formatting/lint and all-ABI debug APK build passed
