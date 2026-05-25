# Demo Installer Launch Offers Plan

## Goal
Make the hosted Mac demo installers the recommended no-data path in the Android setup screen. D2 should use the existing demo offer to download and install the D2 Mac preview, D1 should get a matching offer for the D1 Mac shareware demo, and one persistent preference should control whether either offer appears.

## Tasks
- [x] Inspect current setup sections, resume/preferences UI, and demo installer import helpers
- [x] Verify hosted installer hashes against known package metadata
- [x] Add a shared demo-offer preference with per-offer dismissal behavior
- [x] Route D2 and D1 demo install buttons through download plus StuffIt extraction into the default set
- [x] Add the Game Preferences switch/slider below the resume-offer setting
- [x] Add focused unit coverage for offer visibility/preference logic where practical
- [x] Review and tighten the previous runtime demo audio guard
- [x] Run formatter and focused validation

## Notes
- Hosted `Descent.Shareware.sit` SHA-256 matches known `Descent Shareware.sit`: `f45c338df4bc4ceda38e6541f14b8dc93b543fd07d90a2c5d5118d2001c12ad2`
- Hosted `Descent.II.Preview.sit` SHA-256 matches known `Descent II Preview.sit`: `4b5b7739b9da59472bcdca92f23957f90247bedd84ef8bded57d37d5d229f6d6`
- D2 and D1 offers share `show_demo_installer_offer`; either Stop showing this button turns it off for both
- Focused validation passed: scoped `android\run-code-quality.ps1 -Fix`, `:app:testDebugUnitTest --tests com.dxxredux.app.DemoInstallerOfferTest --tests com.dxxredux.app.DemoInstallerPackagesTest`, `run-windows-build.ps1 -Target d1`, and `run-windows-build.ps1 -Target d2`
