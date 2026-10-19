# Classic demo security hardening with compatibility corpus

## Objective

Fix the security findings from `android/security_scan_remaining_surfaces_20261018.md`
one narrowly scoped area at a time, with broad regression runs between changes.
Classic `.dem` compatibility is a hard requirement: every valid classic demo
must keep its existing behavior. Malformed values may be ignored, skipped when
the event boundary is known, or rejected cleanly, but must never be reinterpreted
as a different valid event or reach an unsafe operation.

## Compatibility policy

- Treat the existing D1 and D2 binary formats and all valid historical variants
  as immutable
- Never change an on-disk opcode, field width, signedness, or version contract
- Preserve valid unknown/legacy values when the original reader intentionally
  tolerated them and they cannot cause unsafe memory or table access
- Reject before mutation when the record boundary cannot be recovered safely
- Skip only a malformed event whose complete encoded extent is known
- Do not clamp hostile values into a valid index, count, string, or object
- Run the pinned original-demo corpus before and after every classic-demo change
- Add a focused malformed-input test for each repaired field or event family

## Rounds

### Round 1: establish the original classic-demo compatibility oracle

- [x] Inventory every `.dem` supplied by the locally pinned original packages
- [x] Add a manifest with source identity and deterministic decoded-output oracle
- [x] Add a headless D1 structural round-trip validator so compatibility coverage
      is not limited to the existing D2 JSON dump hook
- [x] Add a reusable command-level regression runner
- [x] Register the runner as an explicit host test because it needs caller-visible
      original game data and built D1/D2 executables
- [x] Run the new test twice against the unmodified decoder
- [x] Run the existing classic-demo and catalog validation tests

### Round 2: bound classic-demo strings

- [x] Replace destination-unaware `nd_read_string` calls in D1 and D2
- [x] Preserve every valid encoded string and its exact stream advancement
- [x] Reject or safely ignore over-capacity, negative-length, unterminated, and
      truncated strings according to the caller's recoverability
- [x] Add focused D1/D2 boundary tests and rerun the original corpus

### Round 3: validate classic-demo player counts and player indices

- [x] Bound header/trailer player counts before `Players[]` loops
- [x] Validate every multiplayer event player index before state mutation
- [x] Preserve rewrite/dump stream behavior for all valid indices
- [x] Add malformed index/count coverage and rerun the original corpus

### Round 4: validate classic-demo world and table indices

- [x] Repair fixed-width wall and geometry events: wall hit, trigger, wall toggle,
      effect blowup, wall texture changes, door opening, and D2 cloaking walls
- [x] Bound and stage the D2 initial wall snapshot before mutating wall or segment
      state
- [x] Audit and repair object records and their segment, attachment, player,
      robot, powerup, model, bitmap, and vclip references
- [x] Audit and repair classic-demo sound identifiers and shared logical-to-
      sample translation bounds
- [x] Audit and repair initial, trailer, and event weapon-selection indices plus
      their downstream ammo-array consumers
- [x] Audit and repair initial, trailer, and event laser-level fields
- [x] Audit and repair D1 hostage event/object indices while preserving D2's
      safe opaque identity field
- [ ] Audit and repair remaining table domains
- [ ] Complete remaining event-family mutation tests and rerun the original corpus

### Later security rounds

- [ ] Player-file highest-level count and legacy bounded text readers
- [ ] Full save-body structural preflight and transactional local restore
- [ ] Cooperative packet authority and all-peers-or-none restore
- [ ] Remaining server, relay, extraction, TLS, media, Android, privacy, and
      dependency findings in prioritized order

## Round 1 evidence

The pinned installer extractions, CD-image data tracks, GOG extractions, and
shareware data contain 58 `.dem` files representing nine unique SHA-256
identities. The manifest chooses one canonical source for each identity:

- three D1 registered PC recordings (`descent`, `level18`, and `miniboss`)
- three D1 registered Mac-distributed recordings (`demo1`, `watchme`, and
  `yep9`)
- one D1 Test Flight preview recording
- D2 shareware 1.0 `d2demo`
- D2 registered 1.1 `descent2`

All six registered D1 recordings complete the new structural rewrite path
twice with exact frame, final-state, source-size, and rewritten-size results.
The D2 shareware recording still produces 556 JSONL records, 2,054,225 bytes,
SHA-256 `b6beb87e19a78a55a60c31bc74c626e1ba40b4504bd54d601e6c27c9dfb12afa`,
with 554 decoded frames and 4,544 emitted objects. The D2 registered recording
produces 635 records, 6,025,093 bytes, SHA-256
`835a64585932924c155b98576f2b10db234a36d252cf6b2f9935658e8655b918`,
with 633 decoded frames and 15,468 emitted objects.

The D1 Test Flight recording is version 5/game type 1 and the current reader
does not complete it. Round 1 pins its source identity but does not call that a
passing playback oracle. Treat the hang as a pre-existing compatibility and
denial-of-service fix target: it must eventually reject cleanly or play using
the historical format, never be silently reinterpreted.

Round 1 validation:

- All 11 original installer package SHA-256 values matched the import oracle
- `test_classic_demo_compatibility.ps1` passed two exact decodes for eight
  supported originals, pinned the ninth pre-release source, and verified that
  D1 rejects identical source/result paths without changing the recording
- The D1 host executable rebuilt successfully with the configured MSVC x86
  toolchain; normal playback decoding was not changed by the validator hook
- `test_classic_demo_dump_transaction.py` passed after correcting its stale
  invalid-count case from the valid boundary 255 to `MAX_WALLS + 1` (256)
- `test_classic_demo_json` passed through the D2 CTest build
- `test_classic_demo_wall_validation.c` compiled with MSVC `/W4 /WX` and passed
- `test_shareware_test_data.ps1 -RequireData` passed all D1/D2 fixture checks
- `test_validate_automation_catalog.ps1` passed with 95 standalone JSON tests,
  425 support scripts, and 259 standalone PowerShell tests
- `test_run_all_tests_catalog.ps1` passed with 363 top-level entries and 425
  support scripts

The existing host-extract CMake tree could not regenerate because its configured
GCEM source directory no longer exists. The isolated wall test was therefore
compiled directly with the configured MSVC toolchain; this did not weaken its
warning or runtime assertions.

## Round 2 evidence

D1 and D2 now use one header-only counted-string reader from `android/`. It
interprets the encoded length as an unsigned byte, stages and consumes the full
declared payload, checks the destination capacity and trailing NUL, and commits
to the destination only after all validation succeeds. Required header strings
reject the recording. Malformed HUD and multiplayer callsign events are ignored
after their known record extent is consumed during ordinary playback; rewrite
and diagnostic modes reject them so they cannot normalize malformed input into
a different valid recording. Trailer callsigns use the same bounded reader.

Round 2 validation:

- Both D1 and D2 rebuilt with the configured MSVC x86 toolchain after formatting
- `test_classic_demo_string` passed in both build trees, including zero, 255,
  over-capacity, unterminated, missing, and truncated length/payload cases
- The D2 transactional dump test rejected all malformed required mission-string
  cases, preserved the prior output, and leaked no temporary file
- The D1 command-level runner rejected six malformed required mission strings,
  preserved every mutated input, produced a failure receipt, and did not time out
- All eight supported original recordings retained their exact pinned results in
  two consecutive runs; the D1 Test Flight v5 identity remains the known baseline
  target rather than being silently treated as a supported format
- D2 `test_classic_demo_json` and both classic-demo string CTests passed
- The scoped mixed-language quality pass completed with all checks passing
- Both automation catalog tests passed with 363 top-level entries, 425 support
  scripts, 95 standalone JSON tests, and 259 standalone PowerShell tests

## Round 3 evidence

D1 and D2 now validate player counts before header and trailer `Players[]`
loops, validate every recorded multiplayer event player index before state
mutation, and prevent connect events from moving the active player count outside
1 through `MAX_PLAYERS`. The checks use player-array capacity rather than the
current active count so valid sparse and dynamically changing classic player
slots keep working. Known-width malformed events are fully consumed and ignored
during ordinary playback or D2 diagnostic dumping. D1 rewrite validation rejects
them so malformed input cannot be normalized into a valid recording. D2 also
preflights the fixed trailer player-count position before committing diagnostic
JSON output, without invoking broader trailer state restoration that belongs to
the next hardening round.

Round 3 validation:

- The real D1 `yep9.dem` four-player network recording retained its exact pinned
  result in two consecutive corpus runs
- D1 and D2 rejected header and trailer counts 0, 9, and 255; synthetic count 8
  recordings were accepted without clamping or format changes
- Cloak, decloak, death, kill, connect-new, connect-existing, reconnect,
  disconnect, and score events were tested with indices 8 and 255; D1 validation
  rejected them and ordinary D2 dumping safely ignored each complete event
- All nine multiplayer event layouts accepted the maximum valid index 7
- A connect-new event at the full eight-player count was consumed and ignored
  without growing the count or changing D2 diagnostic output
- `test_classic_demo_player` passed in both D1 and D2 build trees, covering count,
  index, add, and remove boundaries
- `test_classic_demo_dump_transaction.py` passed all mission-string, player-count,
  player-index, connection-transition, wall-count, truncation, alias, and output
  transaction cases
- The complete D1 CTest suite passed 81 of 81 tests and the complete D2 CTest
  suite passed 92 of 92 tests
- The original-demo corpus passed again after the full builds: six supported D1
  recordings and two D2 recordings retained exact results across two runs; the
  D1 Test Flight v5 identity remains the known baseline target
- `test_shareware_test_data.ps1 -RequireData` and both automation catalog tests
  passed with 363 top-level entries, 425 support scripts, 95 standalone JSON
  tests, and 259 standalone PowerShell tests
- The scoped mixed-language quality pass completed with all checks passing

## Round 4 wall and geometry slice evidence

Fixed-width wall and geometry events now consume their complete record before
admission. Rewrite validation checks segment and wall fields against format
capacities because it intentionally does not load the full level. Live playback
checks segment and side fields against the loaded world before dereferencing
them and also verifies wall, connected-segment, connected-side, wall-animation,
and texture references at their use sites. Recoverable malformed events are
ignored during playback and D2 diagnostic dumping; D1 rewrite validation rejects
them. No hostile value is clamped or substituted.

D2's initial wall snapshot now validates its count and remaining encoded extent,
reads every record into temporary storage, validates texture and loaded-level
wall references, and only then commits the snapshot. A bad later record cannot
leave an earlier wall record partially installed. Diagnostic dumping continues
to consume the snapshot without publishing it.

Wall and geometry slice validation:

- `test_classic_demo_world` passed in both build trees for negative, zero,
  maximum, maximum-plus-one, orientation-bit, and sentinel boundaries
- D1 rejected malformed segment, side, player, object, primary texture, and
  overlay texture fields across 11 wall/geometry event layouts and accepted the
  corresponding maximum valid format boundaries
- D2 consumed and ignored 13 malformed wall/geometry event layouts and their
  valid boundary counterparts with byte-identical diagnostic JSON output
- D2 rejected initial wall counts -1, 256, and `INT_MAX`, plus every truncation
  of the count and first seven-byte wall record, without replacing prior output
- The original-demo corpus retained exact results across two runs for all six
  supported D1 recordings and both D2 recordings; the D1 Test Flight v5 source
  identity remains pinned as the known baseline target
- The complete D1 CTest suite passed 82 of 82 tests and the complete D2 CTest
  suite passed 93 of 93 tests, including normal-playback classic trigger coverage
- Shareware data validation and both automation catalog checks passed with 363
  top-level entries, 425 support scripts, 95 standalone JSON tests, and 259
  standalone PowerShell tests
- The scoped mixed-language quality pass completed with all checks passing

## Round 4 object-record slice evidence

D1 and D2 now decode each nontrivial object record into temporary storage and
publish it only after the complete record is structurally valid. Recorded
render, object, movement, and control discriminators are checked before their
payload layouts are used. Segment and first-vertex references are checked before
short-position expansion, and attachment links are deferred until after every
remaining field has passed validation. Malformed orphan or corrupt attachment
chains can no longer reach `obj_attach` assertions or index `Objects[]`.

Robot, powerup, player/co-op/ghost, weapon, clutter, polygon-model, texture
override, explosion-delete-object, vclip, and vclip-frame references are now
bounded before table or object-array access. Polygon model submodel counts are
also constrained to the fixed angle-array capacity. Empty encoded orientation
matrices and unsupported discriminator values reject cleanly instead of reaching
debug traps. D2's special reactor robot remains supported without its former
reader-side debug trap.

Rewrite and diagnostic modes use fixed format capacities where the validation
path intentionally has no live level/table count. Live playback continues to
use the tighter loaded-world and loaded-table bounds. The first compatibility
run caught this distinction in D1: `Num_vclips` is zero during rewrite even for
a valid retail recording, so the final check uses `VCLIP_MAXNUM` and
`VCLIP_MAX_FRAMES` in capacity-only mode while preserving loaded `Num_vclips`
and per-clip frame counts during playback.

Object-record slice validation:

- D1 rejected 17 hostile object layouts covering invalid render/object/control/
  movement discriminators, hidden unknown objects, empty orientation, segment,
  robot, powerup, player, weapon, clutter/model, texture override, vclip/frame,
  and explosion-delete-object values
- D2 rejected the same malformed layouts without replacing an existing JSON
  result or leaking a temporary output
- Valid probes retained the maximum format segment, player model and texture
  sentinel, vclip zero/frame zero, the two-byte hidden `OBJ_NONE` record, and a
  safely ignored orphan attachment
- Both host executables and every dependent headless/test target rebuilt
  successfully
- The complete D1 CTest suite passed 82 of 82 tests and the complete D2 CTest
  suite passed 93 of 93 tests, including normal-playback classic trigger coverage
- The original-demo corpus retained exact results across two final runs for all
  six supported D1 recordings and both D2 recordings; the D1 Test Flight v5
  identity remains the known baseline target
- Public D1/D2 shareware data validation and both automation catalog checks
  passed with 363 top-level entries, 425 support scripts, 95 standalone JSON
  tests, and 259 standalone PowerShell tests
- The scoped mixed-language quality pass completed with all checks passing

Object-adjacent items left in the ledger:

- Polygon-model texture ranges and `ObjBitmapPtrs`/`ObjBitmaps` relationships
  originate in HAM/PIG/mod tables rather than object demo records; keep them in
  the later mod-table validation slice
- Object size, life, frame-time, and physics-vector magnitude policies need a
  separate compatibility study before imposing numeric limits
- Remaining event-table domains remain in later Round 4 slices

## Round 4 sound-domain slice evidence

D1's shared `digi_xlat_sound` boundary now rejects logical identifiers outside
the fixed `Sounds[]` and `AltSounds[]` capacity before either table is indexed.
Both D1 and D2 also reject a translated sample identifier outside
`GameSounds[]`; this covers malformed mod-provided sound maps at the common
consumer without changing any valid mapping. D2 retains its existing extended
D1-in-D2 logical-sound path instead of imposing the smaller native D2 logical
range on valid extensions.

The D1 classic-demo rewrite validator now checks the sound identifier in sample,
3D sample, one-shot 3D sample, and object-linked sound events. Ordinary playback
still consumes and ignores a complete event with an invalid identifier. Both
readers now check for truncation after consuming the full fixed-width linked-
sound record and before rewriting or using any field.

Sound-domain slice validation:

- D1 rejected logical sound identifiers -1, 250, and `INT_MAX` in all four
  recorded sound event layouts and accepted logical identifier 249, the maximum
  classic D1 boundary
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- The complete D1 CTest suite passed 82 of 82 tests and the complete D2 CTest
  suite passed 93 of 93 tests
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 363 top-level entries, 425 support scripts, 95 standalone JSON
  tests, and 259 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 event-opcode slice evidence

The classic readers now distinguish defined event records from unknown event
bytes at the switch boundary. `ND_EVENT_SOUND_ONCE` was defined in both classic
formats with the same four-byte sound payload as `ND_EVENT_SOUND`, but neither
reader consumed it. Both engines now preserve and play that historical event
with its one-shot semantics. D2 also consumes a standalone
`ND_EVENT_SECRET_THINGY` record using its documented four-byte payload; the
usual trigger-adjacent form remains unchanged.

A truly unknown opcode has no recoverable record extent. Both readers now stop
with `nd_playback_v_bad_read` instead of falling through a no-op `Int3()` and
reinterpreting following payload bytes as new events. Rewrite and diagnostic
paths therefore reject the input without publishing a normalized or partial
result, while all defined records retain their encoded widths and values.

Event-opcode slice validation:

- D1 accepted opcode 6 at sound identifiers 0 and 249, rejected invalid sound
  identifiers through the existing sound-domain policy, and rejected unknown
  opcode 255 without changing the source recording
- D2 consumed opcode 6 and standalone opcode 48 with byte-identical diagnostic
  output, while unknown opcode 255 failed transactionally without replacing a
  prior output or leaking a temporary file
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 weapon-domain slice evidence

D1 and D2 now stage the initial primary and secondary weapon selections and
publish them only after checking the corresponding fixed player-array capacity.
The same checks protect the final trailer state used by end-of-demo seeking.
D2 diagnostic dumping now preflights both trailer weapon selections in addition
to its multiplayer player count before committing JSON output.

Fixed-width weapon-selection events are fully consumed and then checked. The
registered format validates both the new selection and the recorded old
selection used by rewind. Invalid events are ignored during ordinary playback
and D2 diagnostic dumping, while D1 rewrite validation rejects them. The
historical rule that any nonzero weapon-type byte selects the secondary class is
preserved; safe legacy values are not narrowed to exactly `1`.

Primary and secondary ammo events now verify the currently selected weapon
before indexing either ammo array. D2 computes its Gauss-to-Vulcan shared-ammo
index only after that validation. All four ammo fields are consumed before an
invalid current selection is ignored, preserving the known event boundary.

Weapon-domain slice validation:

- The shared boundary test passed in both games for negative, zero, maximum,
  maximum-plus-one, shareware two-byte events, registered old selections, and
  nonzero legacy secondary types
- D1 rejected invalid primary and secondary selections in both initial and
  trailer state, accepted the maximum valid index 4, rejected invalid new and
  old selections in all four registered event cases, and accepted type 255 as a
  safe secondary event
- D2 rejected invalid initial and trailer indices transactionally, accepted the
  maximum valid index 9, and consumed and ignored all four invalid event forms
  followed by primary and secondary ammo events without changing diagnostic
  output
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 83 of 83 tests and the complete D2 CTest
  suite passed 94 of 94 tests
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 363 top-level entries, 425 support scripts, 95 standalone JSON
  tests, and 259 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 laser-level slice evidence

D1 and D2 now validate recorded laser levels before publishing initial or final
player state and before a value can become a laser weapon-table selector. D1
accepts the complete native range 0 through 3. D2 accepts 0 through 5, including
the two super-laser levels; this capacity-based rule also preserves safe D1-in-
D2 and mod recordings without inventing a narrower mission-dependent policy.
No invalid value is clamped or substituted.

Laser-level events retain their fixed three-byte layout. Both the old value used
by rewind and the new value used by forward playback must be safe. Ordinary
playback and D2 diagnostic dumping consume and ignore a complete malformed
event. D1 rewrite validation rejects it so malformed input cannot be normalized
into a valid recording. Required initial or trailer state rejects cleanly, and
D2 diagnostic dumping preflights the final trailer level before publishing its
transactional JSON output.

The first original-corpus run caught a test and implementation offset error in
that preflight: trailer laser level follows both ammo arrays, at byte 38 in D1
and byte 58 in D2, rather than immediately following the weapon selectors. The
incorrect probe read the D2 retail recording's primary-ammo byte value 32 and
rejected it. The offset was corrected before final validation, and the original
D2 retail recording again produced its exact pinned output.

Laser-level slice validation:

- `test_classic_demo_laser` passed in both build trees for negative, zero,
  maximum, and maximum-plus-one levels plus invalid old and new event values
- D1 rejected initial and trailer levels -1 and 4, accepted 0 and 3, rejected
  four invalid rewind/forward event combinations, and accepted level 3 events
- D2 transactionally rejected initial and trailer levels -1 and 6, accepted
  level 5, and consumed and ignored all four malformed event combinations with
  byte-identical diagnostic output
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 363 top-level entries, 425 support scripts, 95 standalone JSON
  tests, and 259 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 hostage-domain slice evidence

The hostage identity has intentionally different semantics in the two classic
engines. D1 uses the recorded value as a ten-slot hostage index and its rescue
consumer reaches a debug trap for values outside 0 through 9. D1 now validates
both rescue events and decoded `OBJ_HOSTAGE` IDs before they can reach that
consumer or the `Hostages[]`-based rendering domain. A complete malformed event
is ignored during ordinary playback and rejected during rewrite validation; a
malformed object record rejects because its remaining extent depends on its
decoded type tuple.

D2's `hostage_rescue` deliberately ignores the recorded identity. Restricting
that field to D1's range would reject safe historical or mod data without
removing any unsafe operation, so D2 continues to accept the complete encoded
integer domain. The same compatibility rule applies to D2 hostage object IDs,
which are not used as a D1-style hostage-table index. Tests explicitly preserve
this asymmetry rather than forcing a convenient shared bound.

Hostage-domain slice validation:

- D1 rejected rescue IDs -1, 10, and `INT_MAX`, accepted IDs 0 and 9, rejected
  hostage object IDs 10 and 255, and accepted object ID 9
- D2 accepted rescue identities -1, 10, and `INT_MAX` with byte-identical
  diagnostic output to identity 0, and accepted an opaque hostage object ID 255
- The synthetic object fixtures were corrected to include the orientation bytes
  required by `RT_HOSTAGE` and `RT_MORPH`; the corrected boundary records pass
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 level-domain slice evidence

Level changes and the final trailer level are now admitted before they can
select mission arrays or level-specific robot metadata. D1 rejects encoded
level zero, which has never represented a valid D1 level, but its rewrite
validator preserves every other signed-byte value because it intentionally
does not load custom mission metadata. Normal D1 playback additionally checks
both the old and new event levels against the loaded mission before either can
be selected by forward or reverse playback.

Native D2 preserves its historical compatibility exception for early
D2X-Redux recordings that encoded level zero. Those bytes remain unchanged,
normal playback retains its existing level-one interpretation, and rewrite
uses level one only for the internal robot-metadata lookup that previously
asserted or indexed `Level_names[-1]`. D1-in-D2 recordings do not receive that
D2-only exception. All other D2 event and trailer levels must be within the
loaded mission before decode, rewrite, or transactional diagnostic output.

Level-domain slice validation:

- The shared boundary test passed in both games for secret, regular, zero,
  mission-range, and old/new event combinations
- D1 rejected zero in both event fields and the trailer while accepting the
  full nonzero signed-byte encoding range in rewrite validation, avoiding a
  custom-mission level cap without metadata
- D2 transactionally rejected out-of-mission initial old/new and trailer
  levels without replacing prior output, while native legacy-zero variants at
  all three positions still converted successfully
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 frame-navigation slice evidence

Classic frame back-links are now treated as untrusted seek distances. Both
engines require the signed link to be at least the fixed 11-byte frame header
and require its subtraction to remain inside the file before seeking. The
trailer path validates this invariant before rewrite validation can accept the
recording or playback can use the link to find the last frame. Rewind applies
the same check on every hop and stops playback cleanly if a link is unsafe.

The readers do not require a back-link to equal a recomputed frame size. That
would impose a new canonical-encoding rule and could reject a historical demo
whose link is unusual but remains memory-safe. Registered trailer byte counts
are now read as their encoded unsigned 16-bit type and bounded against the file
before their trailer-start seek. Checked 64-bit PhysFS positions replace the
previous unchecked signed-relative arithmetic in these paths.

Frame-navigation slice validation:

- The shared boundary test passed in both games for links below the 11-byte
  structural minimum, the minimum valid link, an exact file-start target,
  an underflowing link, an invalid origin, and a missing result pointer
- D1 rejected trailer links 0, 10, and -1 without changing the hostile source,
  while D2 rejected the same mutations transactionally without replacing a
  prior output or leaking a temporary file
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 frame-metadata arithmetic slice evidence

Recorded frame numbers and frame times remain fully opaque on disk and are
preserved by rewrite. Their playback-only arithmetic is now total over the
complete encoded 32-bit domain. Decrementing `INT_MIN` frame numbers saturates
instead of invoking signed overflow. Nonpositive recorded frame times are
ignored for scheduling, positive cumulative time uses saturating addition, and
interpolation is skipped when its recorded-time divisor is nonpositive.

These rules do not reject or rewrite a noncanonical historical record. They
only prevent hostile timing metadata from causing signed-overflow undefined
behavior, division by zero, or backwards cumulative scheduling. Valid positive
times retain their exact previous arithmetic until the old signed `fix` range
would have overflowed.

Frame-metadata arithmetic slice validation:

- The shared boundary test passed in both games for negative, zero, ordinary,
  and saturating time advances plus ordinary and `INT_MIN` frame decrements
- D1 preserved synthetic frame records with times 0, -1, and `INT_MAX`, plus an
  `INT_MIN` frame number; D2 decoded the same four layouts successfully through
  the live arithmetic path and published complete transactional output
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 signed-scaling arithmetic slice evidence

Classic short-position and fixed-point event decoding no longer left-shifts
signed recorded values. Negative orientation bytes, relative coordinates,
velocities, compact object lifetimes, homing distances, and D2 cloaking-light
values are valid encodings, but shifting them invoked C undefined behavior.
Both readers now use exact multiplication by the same power-of-two scale, so
every representable historical value retains the same intended bit value.

Relative positions additionally combine demo offsets with level vertex
coordinates. That addition is now evaluated in 64 bits and admitted only when
the final coordinate is representable as a 32-bit `fix`; an overflowing
demo-and-level combination rejects the staged object instead of overflowing
before publication. Signed player-flag packing and D1's shareware header repair
also use unsigned bit assembly before converting back to the stored integer.

Signed-scaling arithmetic slice validation:

- The shared boundary test passed in both games for negative scaling, exact
  `INT32_MIN`, positive and negative coordinate overflow, invalid scale, and a
  missing result pointer
- D1 rewrite validation and D2 live diagnostic decoding accepted a synthetic
  object with -128 orientation components and `INT16_MIN` coordinates and
  velocities, followed by an `INT16_MIN` homing-distance event
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed

## Round 4 event-counter arithmetic slice evidence

Demo-controlled death, kill, team-kill, multiplayer-score, and player-score
events no longer rely on signed overflow or implementation-defined narrowing.
The shared helpers perform explicit unsigned modular arithmetic and reconstruct
the corresponding signed 16-bit or 32-bit value without an out-of-range signed
conversion. This preserves the two's-complement wraparound behavior of the
classic x86 builds for every encoded value instead of rejecting, clamping, or
rewriting unusual historical counters.

Only playback state arithmetic changed. Event opcodes, field widths, signedness,
stream advancement, and rewrite bytes remain unchanged. Forward and reverse
playback use paired add/subtract helpers, including `INT_MIN` score deltas whose
negation would itself be unsafe.

Event-counter arithmetic slice validation:

- The shared boundary test passed in both games for ordinary negative deltas,
  16-bit and 32-bit positive and negative wraparound, and subtraction across
  both signed boundaries
- D1 preserved synthetic death, kill, multiplayer-score, and player-score
  boundary records, including initial 16-bit totals at `INT16_MAX` and score
  deltas that force 32-bit wraparound
- D2 decoded the same multiplayer counter boundaries through live playback and
  separately decoded a single-player score sequence guaranteed to cross a
  32-bit boundary regardless of its initial score
- All six supported D1 recordings and both D2 recordings retained their exact
  pinned results across two consecutive runs; the D1 Test Flight v5 identity
  remains the known baseline target
- Both full host build trees completed successfully
- The complete D1 CTest suite passed 84 of 84 tests and the complete D2 CTest
  suite passed 95 of 95 tests
- The D2 transactional dump suite passed without output replacement or leaked
  temporary files
- Public D1/D2 shareware-data validation and both automation catalog checks
  passed with 364 top-level entries, 426 support scripts, 95 standalone JSON
  tests, and 260 standalone PowerShell tests
- The scoped mixed-language quality pass and `git diff --check` passed
