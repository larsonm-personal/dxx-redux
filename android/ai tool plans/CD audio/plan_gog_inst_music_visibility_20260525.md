GOG INST music visibility plan

Context:
- The macOS GOG Descent 2 installer can extract redbook audio as .gog and .inst files.
- These files are bin/cue equivalents and should appear in both the Android launcher music list and the in-game music player.

Steps:
- [completed] Find the installer extraction outputs, launcher music discovery filters, and in-game cue/bin lookup filters.
- [completed] Add .gog/.inst support at shared extension or pair-discovery points without breaking existing .bin/.cue behavior.
- [completed] Extend or add focused tests covering GOG-style pairs in launcher and engine-facing music discovery.
- [completed] Run targeted tests and code quality checks where practical.

Notes:
- The probed Mac D2 package extracts DESCENT_II.gog but not DESCENT_II.inst.
- The Windows GOG installer has both DESCENT_II.gog and DESCENT_II.inst.
- The fix preserves actual GOG/INST filenames and synthesizes the known D2 cue for the Mac package.
- Resume follow-up fixed the remaining launcher gates that checked findGogPair() before cue synthesis.
- Targeted JVM tests and android\run-code-quality.ps1 -Fix both pass.
