# XL cloak lifecycle

XL wall cloaking now uses a separate runtime for activation, reversal, fade
arithmetic and 100 concurrent entries. Redux retains its existing implementation
and ten-entry limit in the same binary. This completes the method-0/default-speed
cloak profile; it does not qualify entire original missions or other XL settings

## Reference and behavior

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- main/wall.cpp: 05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42
- main/segment.cpp: eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d
- include/wall.h: 08c59bf8c299ea05967f806b92b3d03b4226914ef747ae1ef5afa4154c70c2bd
- render/renderlib.cpp: d3eacfaf28c9687e7e37c3e47333b11d4a026000839231e8a71b443bc86d4252
- include/palette.h: 3394f7accb1e7e53679fe94ad54256134acdf7a5a374d03756cab7c3f1f6d7e9

The oracle extracts actual allocation, lookup, start/caller, frame and WallAlpha
functions. Constants come from pinned headers, independently of Redux. The
source uses FADE_LEVELS 31, not Redux's 34. Opening's second half uses the source
float-to-signed-byte conversion; closing's first half uses its 29-level formula.
Both transition halves use a flat black face under lighting method 0, including
when the live wall type is still CLOSED. Reversal preserves the complementary
clock and captures the current face lighting, including a reversal from the back
side. Repeat requests and the source's type/state guards remain intact

Production intentionally corrects these reference defects:

- Process the shifted live entry after deletion, rather than skipping it
- Record the completed owner before compaction can replace that slot
- On decloak allocation failure, close both wall types and reset their states;
  the source mistakenly assigns a wall type to the state field
- A large final frame explicitly sets the completed type and restores captured
  front/back lighting, even if it skipped an intermediate half
- Missing connected walls are optional, not dereferenced

The 101st opening request retains the source's immediate-open fallback. Physical
storage increases to 100 without changing the packed cloak record. Save restoration
validates count, owners, pairing, uniqueness, transition state and timer before
installing the cloak table. This is not a claim of whole-world transactional
rollback for invalid legacy saves; valid recovery is checked after every rejection

Legacy demo back byte 255 and wide back short -1 represent no back wall. Playback
and rewrite now preserve this meaning and update the front only. This repairs that
consumer; complete XL demo fidelity, including independent back-side lighting,
remains open

## Native acceptance

`xl_cloaks` derives its private fixture from original Neptune geometry through the
existing close-door fixture. It adds 100 closed pairs on existing portals, keeping
all original vertices, segment vertex identities and child connections. Total:
230 walls and 101 possible cloak pairs, with authored OPEN_WALL/CLOSE_WALL actions
on the original pair

- 3,200 actual engine samples match extracted source caller/frame/flat-alpha
  results under MSVC and strict GCC: opening, closing, one/two-sided walls,
  signed cloak bytes, both reversal halves and exact half/full boundaries
- Authored actions and physical/visibility rays run through actual engine frames
- 100 simultaneous entries, a 101st request, repeated requests and back-side
  reversal preserve clocks and owner identities
- Disk restoration through stock preserves all 100 packed entries and earned
  shields/energy; reversed state also survives disk restoration
- Negative/101 counts, invalid front/back, negative/over-duration clocks,
  duplicate ownership and a truncated table reject before installation; valid
  recovery succeeds after all eight cases
- Large final opening/closing steps finish all entries without skipped owners and
  restore every captured front/back light value
- Existing wide-wall and close-door integrations pass, including optional-back
  demo playback and byte-identical rewrite
- The D2 stock compatibility suite passes, including its existing ten-cloak
  capacity/overflow and unrelated door-animation checks

Original Neptune HOG remains
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303

Derived fixture hashes:

- HOG: bcd12bae0b13a1e739a3daaa4ab10a21b8623087902aa94da7b66bd7e09c6bcb
- Descriptor: c5291338fda45ee11516ebffae55826f27e5b64eb51ddac07798e4d6875cb98d

## Android acceptance

The owned `test_xl_cloak_runtime.ps1`/JSON integration is registered with a
600-second master timeout. Final formatted-APK run
5dadbc54a4204b95846396ee8d3301d8 passes all 81 steps in 86134 ms, following the
81-step pass 97004dba2c2d40899f9bed0d7f6054bd in 77471 ms. It covers controlled camera poses, authored opening/closing,
reversal, actual passage and closed-wall blocking, ordinary pause/resume, active
animation disk restore through stock and memory checkpoint recovery

The final actual GL color-array alpha is 0.419354856 for cloak value 18, matching
1 - 18/31. The captured active clock is 39846/65536 ticks, with one paired entry.
Opening and closing samples complete without mixer failures. The runner verifies
the actual draw value numerically and preserves a screenshot; this is not a claim
of full screenshot parity with the reference renderer. Stock mode reports no XL
draws or active wall transitions. Protected player/configuration/mission hashes
match byte-for-byte before and after the run

The scoped mixed formatter and pinned PSScriptAnalyzer 1.25.0 pass. Both required
catalogs pass: 99 standalone JSON tests, 466 support scripts, 297 standalone
PowerShell tests and 410 top-level master entries. Final formatted native wall,
cloak and stock tests and the Android build pass

## Remaining scope

Other lighting methods, optional slow motion, colored/team walls, wider authored
flags, remaining trigger/object capacities and broader original-mission
qualification remain separate work. No campaign is promoted solely because its
cloak records load or this derived fixture passes
