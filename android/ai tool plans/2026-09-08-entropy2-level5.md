# Entropy2 level 5

Reproduce the level 5 timeout, diagnose the blocked route using engine logs, implement a general correction in shared source, and verify deterministic completion and the nine-mission corpus. Keep implementation out of headers and leave outstanding_bugs.md untouched.

The baseline times out after 4,657 frames with no completed objectives. The actor reaches the red-key carrier in segment 259. Diagnostic damage logging confirms robot 79 (id 42) has negative shields and a started death roll, but sandbox normalization left control_type at CT_NONE. Its death animation advances only through AI dispatch, so the explosion and key drop never occur.

The simulation now restores CT_AI for a frozen robot with an active death roll, allowing normal engine death processing and content release. Living robots retain the existing sandbox behavior.

The level completes deterministically at 4,338 frames, including key pickup, triggers 20 and 24-28, the boss, and the exit. The repeat integration test also covers Counterstrike 10's three key carriers, which retain their 9,173-frame completion. The Windows D2 build and all 49 native tests pass, as do scoped formatting/lint checks. This source is used by the D2 simulation, including D1-in-D2 verification; the D1 native target does not compile it. No route metadata change is required.

Artifacts: `temp/entropy2_l5_probe_current.log`, `android/temp/entropy2_l5_probe_current`, `temp/entropy2_l5_fix_build.log`, `temp/entropy2_l5_integration.log`, `temp/entropy2_l5_ctest.log`, and `android/temp/core_nine_entropy2_l5_verified`.

Full corpus verification: 189/209 pass, up from 188, with no lost passes. Only Entropy2 level 5 changes status. All six Entropy2 levels now pass; its normalized simulation JSON is updated from the verified run.
