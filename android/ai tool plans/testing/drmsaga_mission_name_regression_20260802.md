# drmsaga mission name regression

## Plan

- [x] Compare the regenerated and previous drmsaga metadata
- [x] Inspect the archive descriptor and relevant regeneration history
- [x] Trace mission-name candidate selection and normalization
- [x] Scan the mission archives for other descriptors with multiple title declarations
- [x] Identify a narrow proposed fix that does not weaken improved names
- [x] Record the cause and result

## Result

Commit `2c89ee33` changed the automation result writer to replace the native, 25-character-limited mission name with the launcher's parsed descriptor display name. This improved most long or non-ASCII names.

`Drmsaga.mn2` is unusual because it declares both `zname = Dr. Moreau's Saga of Death` and `name=drmsaga`. The shared launcher parser and the host regeneration script prefer `name`, so the new result override changed the previous truncated title `Dr. Moreau's Saga of Deat` to the archive-style identifier `drmsaga`.

Only three scanned descriptors declare multiple title keys. The other two have useful `name` values that should remain preferred. The narrow fix is to use an enhanced title only when `name` is merely the descriptor filename stem, ignoring case. For this archive, that produces the full `Dr. Moreau's Saga of Death` while preserving the current Entropy 2 and Omicron Project titles.

## Implementation

- [x] Apply the filename-stem fallback rule in the shared Kotlin parser
- [x] Apply the same rule in host metadata regeneration
- [x] Run existing parser tests and scoped code quality
- [x] Regenerate drmsaga and verify the known multi-title descriptors

The focused Android regeneration passed all 8 automation steps and published `Dr. Moreau's Saga of Death`. Its checked-in JSON changed only `source` and `mission_name`. Direct validation of the host parser produced the same full title while retaining `Entropy 2: Vengeance` and `Omicron Project 1.4c` for the other multi-title descriptors.
