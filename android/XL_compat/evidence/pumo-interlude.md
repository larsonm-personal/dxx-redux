# Original Pumo Mineral Valley interlude

Tranche 118 extends the existing protected MissionFlow owner through the original
level-nine exit into level ten, mvend.rl2. It preserves the original HOG,
descriptor and mod resources. No geometry, triggers or mission routing are
rewritten, and the test does not navigate the mine autonomously.

The controlled route is 1 -> 2 -> 3 -> 4 -> 14 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10.
Exit dispatch bypasses traversal and presentation menus but uses the actual
authored trigger and ordinary level/cache loading. Level nine's trigger 27 is an
exit with destination 10. Level ten has 19 segments, five walls, an ambient-fan
owner at object 1 in segment 3 and one automatic message trigger. It has no exit
trigger. Its text thanks the player for finishing Mineral Valley and directs
them to the main menu; this is not evidence that later Pumo chapters are playable.

The message table is mvend.msg, SHA256
32e49a35fd856a74c5f5c334f18f2b0305b81de2d49b7f80c8b80165b14aa440.
The check requires automatic display of message 1 and the exact source-decoded
text, including its line break.

The lifecycle check captures the ambient owner's activation and mission progress,
sets player energy to 77, writes a checkpoint, disables the fan and changes energy
to 19, then restores. It checks activation, mission progress, energy and the
external audio owner's resource/identity relationship. Finally it restores the
saved hub and repeats the existing saved-state and GPU checks.

The final protected device run passed all 117 steps in 585901 ms, run ID
715aad35a8d44a85b5a6e15d000e7aea. It covered the original route, spray and lightning
checkpoints, lightning pixels and located light before/after actual context
recreation, automatic interlude text, ambient-fan checkpoint recovery, and the
saved hub's state and GPU checks. Final Android build passed in 2m1s; both
automation catalogs passed (427 top-level entries, 498 support scripts).
The runner exited successfully and restored byte-identical protected app-data
hashes. Renderer and native validation from unchanged prerequisites was reused.

The first run stopped at the existing level-nine scene assertion, before reaching
the interlude: run 7151e19d950840cd9e0fc6b0680444da, step 78/117, 373960 ms.
Its first sampled batch changed 4793 pixels but found zero lit segments. The probe
had completed on pixel changes alone. It now completes successfully only after
both pixel and located-light witnesses, with a finite 64-batch bound. The source
light sampler can reject nodes outside connected mine geometry while the bolt
remains drawable. Production lightning simulation and rendering are unchanged.
Protected app-data hashes were identical after the failed run's restoration.

The second run reached level ten and automatically displayed message 1 (31 draw
calls, elapsed 842 ms), but stopped at an incorrect expected string in the new
probe. Raw MSG bytes 5c 6e decode to a newline; escaped diagnostic output had been
misread as an additional literal backslash. The test expectation was corrected.
The message implementation and original data were unchanged.

Full campaign traversal, combat, secrets and optional 3D powerup presentation
remain unqualified. Later camera, bubble and additional lightning profiles still
have independent admission blockers.
