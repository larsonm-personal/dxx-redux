# XL trigger capacity and byte identities

Continue the thirteen-hour run after PR #176 (separate cloak lifecycle). Work in
xl_more_32_triggers, based on xl_more_31_cloaks at
adf2b0a642ef368153513e3f02142af78dd4a130. No merges; keep the user cmake checkout
untouched. Complete foundations and consumers before claiming wider admission

## Source findings

Reference 2576a98b5de310e3ac7c9777df2677d1627b8bae:

- include/trigger.h: MAX_TRIGGERS and MAX_OBJ_TRIGGERS are 254, target count is 10
  SHA256 8f539027666ce768f467226381e82fd62c07a3cd3642fe97f857980091cf4dfd
- gameio/loadobjects.cpp ReadTriggerInfo allocates the declared wall/object table
  dynamically; this alone does not establish safe persistent capacity
  SHA256 01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c
- gameio/savegame.cpp bounds both restored tables to 254, inclusive
  SHA256 4784770058b2d4880c33a81eb1a7611738959113a80bce48de248eee7a54a795
- main/gamedata.cpp Create uses the declared count, but object references use the
  fixed MAX_OBJ_TRIGGERS capacity
  SHA256 782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12

Current gaps:

- Admission and physical wall-trigger arrays stop at 100
- Live wall trigger and controlling-trigger fields are signed bytes. IDs 128+
  therefore need explicit unsigned-byte interpretation with 255 as the missing
  sentinel throughout activation, chaining, validation, metadata and other users
- Save restore installs an unbounded trigger count before reading the table
- XL scheduler/activation arrays and saved extension validation use MAX_TRIGGERS
- The legacy pilot diagnostic is a 64-bit trigger mask and cannot represent all
  IDs; do not confuse that diagnostic with actual activation evidence
- Object-action admission currently allows 255. Audit all object consumers and
  source save bounds before deciding whether this should become the shared 254
  profile; do not treat the source's dynamic allocation as full qualification
- ND_EVENT_TRIGGER stores segment/side/object/shot ints and resolves the wall's
  trigger during playback; it does not contain a signed trigger-ID byte

The original high-wall audit identifies dinter.rl2 and antholgy.rl2 as the two
members with trigger-capacity blockers. Their existing hashes and independent
requirements remain the baseline; removing one blocker does not qualify either

## Implementation and acceptance

1. Audit all trigger/controlling-trigger byte consumers. Preserve packed record
   sizes and sentinel meaning; normalize indices through one checked convention
2. Give XL sufficient physical storage and runtime state for 254 wall triggers.
   Retain the classic/imported-D1 limit of 100 at load and restore boundaries
3. Validate saved counts and table reads before installation. Preserve source
   ownership and scheduler state. Reject invalid references and recover from a
   valid save; do not claim whole-world atomic rollback for legacy saves
4. Derive a 254-trigger fixture from existing owned geometry. Exercise IDs 99,
   100, 127, 128 and 253, both sentinel/invalid boundaries, real wall activation,
   immediate/delayed masters, enable/disable/alternate state and source ordering
5. Verify disk through stock and memory checkpoints, high-ID demo crossing
   dispatch/rewrite where applicable, metadata identities and stock isolation
6. Run extracted-source controls where useful, native boundary/lifecycle tests,
   a registered owned Android integration, scoped formatting and both catalogs
7. Reinspect the two original members without modifying their bytes. Publish
   stable evidence of remaining blockers; no mission-name exceptions

The native implementation now retains all byte identities with 255 as missing,
uses a 254-entry physical table and keeps stock/imported D1 at 100. XL saved counts
must also match the authored table; packed records are read into temporary storage
before installation. The legacy 64-bit diagnostic remains available while new
per-ID diagnostics cover the full table and explicitly reset at level load

Public three-layout boundary/adapter contracts and stock native regressions pass.
The native lifecycle fixture passes real wall dispatch, delayed masters,
enable/disable, alternate/one-shot state, disk through stock, seven bad-save
controls with recovery, metadata snapshots, demo recording/playback/rewrite and
read-only invalid-source rejection. The unowned-adapter test uses a stock mission
to reach the actual 100-trigger guard, rather than stopping at descriptor rejection

The registered Android integration passes all 77 steps in run
3b043b9a8d3e42ce9f055650103b6b38, with protected state restored exactly.
The 1,037 extracted-source cases pass MSVC and strict GCC. Original audits retain
13 Dinter and five Antholgy blocker categories, with both hashes matched.
All seven related native integrations pass after correcting the older object-
sorting control from 255 to the pinned persistent bound 254 and adding a 255
rejection control. Final review also corrected a missing negative-index guard in
object enable/disable links; native controls cover IDs 128, 253, 254 and 255.
The synthetic link controls reload the authored fixture before later disk tests,
retaining its original object-action trailer and binding. Both 254-trigger and
2047-wall metadata snapshots are explicitly checked.
Final formatted-APK run 6efd1d1ffad24c12b1e195bff637fbd9 passes 77/77 in 70508 ms.
Protected app hashes match exactly. Scoped formatting, explicit PSScriptAnalyzer
1.25.0 and both catalogs pass (99 JSON / 467 support / 298 PS / 411 master).
Publication remains pending; no merges are authorized. Object table headroom, remaining actions, expanded gameplay data,
effects and whole-mission qualification remain separate unfinished work

## CI to monitor

PR #175 wide walls: latest head 8a1a93aeadd9a226a61a522231642ae79cf4bc2a,
all seven workflows green at 06:51 UTC
PR #176 cloaks: latest head adf2b0a642ef368153513e3f02142af78dd4a130,
one commit / 30 files; all seven workflows green at 07:28 UTC
