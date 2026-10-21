# Closed testing release

1. Inspect existing publishing scripts and live Play tracks (done: alpha draft, internal 27170)
2. Add a closed testing menu option with Release defaults and global version allocation (done)
3. Report draft/review outcomes accurately and document the repeatable process (done)
4. Extend publishing integration coverage, format, and run relevant checks (done; analyzer limitation below)
5. Build and submit the alpha release; report the verified outcome and any Console prerequisites (uploaded and verified; first rollout blocked by app draft state)

## Implementation and verification

- Action 4 selects Release signing and alpha; custom closed track names can be supplied
- Version allocation checks all tracks and uploaded bundles, including internal testing and drafts
- Deployment distinguishes committed changes from tester availability, reports review submission requirements, and fails on a remaining draft
- Fixed single-track API response handling in the existing deployment picker
- Expanded the existing publishing integration runner with closed-track workflows and real deploy-script simulations
- Passed all 23 menu scenarios, global version allocation, and completed/review/draft/transport deployment simulations
- Deployment source contracts and both automation catalog checks passed
- Scoped UTF-8/Prettier checks and PowerShell formatting/parsing passed; PSScriptAnalyzer lint raises an internal InvalidOperation error on this host, including outside the sandbox
- No connected browser is available for Console-only setup; service account API access works
- Building Release 27171 from commit 092f4c7d2 plus the existing working tree changes
- Signed Play AAB and companion APK built successfully for all three ABIs
- Actual artifact integration check passed: separate package IDs, version 1.1 (27171), APK signature, identical engines, and no Play SDKs in direct APK
- Uploading AAB SHA-256 f892c8fcdafdf2dff22042c00099a854010ca290e62d22caf52f681905409e00 to alpha

## Live outcome and next action

- Google accepted and committed alpha release v27171 with status draft
- A fresh API edit confirmed alpha contains version 27171 in draft and its remote bundle SHA-256 matches the local AAB exactly
- Google rejected promotion with HTTP 400: "Only releases with status draft may be created on draft app."
- The wrapper correctly failed instead of claiming the release was available to testers
- Complete the app Dashboard setup in Play Console, then open Testing > Closed testing > Alpha, review saved release v27171, configure countries/testers, and start rollout/send for review
- Reuse the existing uploaded bundle for the first rollout; no rebuild or upload is needed to resolve app setup
- Once app setup is complete, future releases use ./android/0_upload_to_test.ps1 -Action 4
- Artifact: android/build-outputs/deploy-release-v27171-41f37aa262e94025a4fb3181a5bc9b09.aab
- Companion: android/build-outputs/deploy-release-v27171-41f37aa262e94025a4fb3181a5bc9b09-universal.apk
