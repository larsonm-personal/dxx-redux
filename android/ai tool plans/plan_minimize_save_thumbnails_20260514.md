# Minimize Save Thumbnail Follow-Up

## Goal

Restore thumbnails for Android minimize-triggered save games without regressing the existing resume-offer flow.

## Local hypothesis

- D1 and D2 Android direct-slot autosaves currently force blank thumbnails unconditionally in `state_android_save_to_slot()`, so minimize and exit autosaves can never carry a real preview even when the current frame is still readable.
- The launcher resume-offer selection already scans all metadata-backed single-player `.sg#` saves and chooses the newest by save time, so no selection change should be needed unless validation disproves that.

## Cheap check

- Remove the forced blank-thumbnail flag in both D1 and D2 Android direct-slot save helpers.
- Rebuild Android and run a focused autosave path check, then inspect the newest candidate metadata or save preview path for `has_thumbnail=true`.

## Steps

- [x] Patch D1 and D2 Android save helpers so the Android metadata trailer carries the save thumbnail
- [x] Build Android debug
- [x] Run a focused HOME or minimize autosave validation and inspect thumbnail presence
- [x] Record the confirmed outcome in this tranche plan

## Outcome

- The launcher resume-offer selection already scans metadata-backed `.sg#` saves across both games and is not restricted to the autosave slots.
- The missing thumbnail bug was not only the old blank-thumbnail flag. The actual root cause was that the Android save metadata trailer never received thumbnail bytes, so `ResumeSaveBridge` always saw `has_thumbnail=false` even when the save body had a preview image.
- D2 HOME-background validation now produces `resume_candidate.save_kind = auto_minimize`, `resume_candidate.has_thumbnail = true`, `thumbnail_width = 100`, and `thumbnail_height = 50`.