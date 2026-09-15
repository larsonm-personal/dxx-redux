# Skip empty co-op briefings

## Intent
Levels without authored intro pages or movies use ordinary level synchronization only, while keeping the session briefing preference enabled for later levels.

## Implementation
- Count intro content before normal level loading, using the existing read-only briefing/movie planner
- Carry the host decision in bit 1 of the existing Android CoopBriefings SYNC byte; bit 0 remains the preference
- Apply the decision only on validated SYNC packets, not lobby metadata
- Carry the same decision in reserved byte 18 of the existing prepared secret-campaign transfer for normal advances that bypass UDP level sync
- Return before creating the briefing window, pausing time, or restoring palettes when no intro is scheduled
- Extend the two-device LAN runner with -EmptyBriefing and assert no briefing generation or presentation

## Validation
Passed:
- Scoped mixed-language formatting/lint
- Android assembleDebug (both engines, all configured ABIs)
- Windows build (D1 and D2)
- Two-emulator Maximum (fixed) level 14 with -EmptyBriefing: both peers entered unpaused gameplay with briefings enabled, zero local pages, zero briefing generations/presentations, and no palette restoration
- Two-emulator Maximum (fixed) level 13 with -BriefingPalette: authored presentations and palette restoration passed on both peers
- Two-emulator D1 level 1 with -Briefings: authored presentations and force-launch passed

Logs: temp/empty-briefing-{format,android-build,windows-build,max14-test,max13-test,d1-test}.log
The secret-campaign transfer hook was built and reviewed; a secret-exit advancement was not exercised on-device in this run.
