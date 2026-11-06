# LAN discovery fixture admission

- Inspect run 42201763286 at cmake 02bb982 and current remote refs
- Recover retained public evidence using the existing evidence export workflow
- Give the discovery owner its own pinned shareware fixture and matching engine mission key
- Preserve admission validation and all discovery, join, resume, screen-off and reconnect assertions
- Exercise fixture setup and negative controls without devices, run catalog and scoped quality checks
- Push a new fix branch and open a draft PR against cmake; device verification remains required

Evidence: export run 42252004291 recovered 69 bounded files from nightly-results-android-dual
The prior launch-preparation owner passed D1/D1, D2/D2 and D2/D1 sessions
Discovery then requested d2, while the retained data was shareware (d2demo)
MissionCatalog.find requires an exact key; defaultMissionRequirement falls back to a builtin without size/hash, which MissionRequirement.isValid rejects
This is a fixture ownership bug, before discovery begins, not failed emulator provisioning
