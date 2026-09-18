# Mission assets and preview objectives

- [x] Pass the active mission asset context to background metadata workers and include it in failure-cache identity
- [x] Separate preview display visibility from route exploration and retain closed remote-shot door objectives
- [x] Load the selected preview descriptor directly to avoid ambiguity with its installed copy
- [x] Add focused regression coverage for isolated mission mounts and Maximum S5 objectives
- [x] Run scoped formatting, relevant native/Android builds and tests

Evidence: Enemy Within Rebirth level 1 logs report missing level01.rl2 in the background worker and reuse that failure on resume. Maximum S5 preview marks both sides of the required door visited before selecting the live route

Validation: 1,050 JVM tests passed; D1/D2 Windows builds and metadata scan tests passed; native certifier regression passed; Maximum S5 host simulation confirmed all three objectives; Android x86_64 build passed; Enemy Within emulator regression passed all 27 steps, producing background metadata and selecting switch 2 followed by the gold key. Maximum S5 preview matched the canonical objective sequence, kept Open door selected, rendered correctly, responded to camera input and closed through its explicit command

Separate finding: Android KEYCODE_BACK did not close the preview in emulator-5554. Recorded in outstanding_bugs.md; the general smoke test still checks Back by default, while the focused objective runner uses CloseWithCommand
