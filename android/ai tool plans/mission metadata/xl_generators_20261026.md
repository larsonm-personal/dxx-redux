# XL robot generation chunk

Continue the singleplayer roadmap after object events. Branch xl_more_8 is based
on cmake after object-event PR #150 merged. Bounded robot-matcen behavior
is verified within the evidence below. Equipment generators, expanded gameplay
data and object spawn actions remain open in the overall roadmap. No new original
mission is qualified

## Reference audit

Pinned arbruijn/d2x-xl revision 2576a98b5de310e3ac7c9777df2677d1627b8bae

- main/producers.cpp SHA256
  c79263fcd98f686d99dfcc8f5e12aade60f70f75cc25544795fbab71c93318b6
- gameio/loadgame.cpp SHA256
  5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55
- gameio/loadobjects.cpp SHA256
  01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c
- ai/aiboss.cpp SHA256
  6ee8d46cb9211dde69cdd4ed1d2f1a4efde7dc13c862a3ec4c0206f1a2e29e99

DoSpawnBots calls OperateRobotMaker, which selects a robot from the first linked
generator (or an arbitrary eligible robot with no link), then calls the owner's
BossSpewRobot. This spawns at the owner, not at the generator, and requires boss
identity, gate cooldown/caps, intersection retries, lifetime, AI/morph state,
sound and physics. It also suppresses normal boss spew when a spawn action is
attached. Do not alias this action to classic matcen activation

AssignProducer associates source row i with the first robotmaker segment whose
authored producer index is i. Serialized row segment/producer fields are ignored.
Redux's stock mine loader instead renumbers robotmaker segments in geometric
order. Copying XL rows unchanged silently swaps contents for nonmonotonic source
indices. Translate contents into native order while preserving source evidence;
reject missing, duplicate or out-of-range assignments with useful diagnostics

Empty robot lists use a third word derived from initially present nonboss robot
IDs >=66. The handler's short-circuit does not refresh that word for nonempty
lists. CObjectData::Create/Init clears it before the source rows load. Save restore
calls PrepareLevel/LoadLevel and SetVertigoRobotFlags before replacing source
objects with saved live objects. Rebuilding from the source on restore therefore
preserves the pinned lifecycle; it must not follow live removals each frame

Default XL math format 2 uses double sqrt for player distance. The initial timer
boundary is inclusive, the half-morph boundary is strict, failed effect allocation
does not begin a morph, and insane activation checks lives only below insane.
Generated thief/companion health also uses RobotDefaultShield's level/difficulty
scaling. These differences now have production and independent reference controls

Additional lifecycle pins:

- main/gamedata.cpp SHA256
  782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12
- gameio/savegame.cpp SHA256
  4784770058b2d4880c33a81eb1a7611738959113a80bce48de248eee7a54a795
- maths/vecmat.cpp SHA256
  4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335

Contact reference pins:

- include/vecmat.h SHA256
  83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f
- include/maths.h SHA256
  43adf417dd1c282a39c8fed02bdad9d05a7f3ad5850a39a87897fd533eb857cd
- physics/collide.cpp SHA256
  d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9
- physics/physics.cpp SHA256
  2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76

## Work and acceptance

1. Correct source-to-native generator assignment with independent pinned reader/
   assignment comparisons, malformed controls and production-load witnesses
2. Carry the full authored robot selection/resource closure, including implicit
   Vertigo candidates, through activation, morph, combat and exhaustion
3. Match timing, occupancy, population caps, failure/retry and difficulty lives;
   preserve user matcen-mode behavior and isolate stock gameplay
4. Verify active/pending producers, spawned robots, counters, selection and RNG
   across disk and real rewind restores, source reload and stock transitions
5. Add native/reference and registered Android integration, inspect original
   mission requirements and update support evidence only for proven behavior
6. Return to object spawn actions with the required boss lifecycle work rather
   than claiming generator fidelity supplies those semantics

No autonomous navigation. Equipment generators, expanded capacities and invalid
source layouts remain separate requirements. Keep the overall roadmap active

## Assignment checkpoint

Source-to-native remapping and helpful malformed-assignment rejection are
implemented locally. The former swapped-content case fails before the change
and passes afterward. Independent extracted reader/assignment comparisons pass
18 cases across all three layouts. Production trigger activation, three actual
morph spawns, pending disk restore and a stock transition pass. The focused
native sweep passes 13/13. Subsequent controls below complete the bounded
robot-matcen portion; item 6 remains the next dependent work area

## Runtime and integration acceptance

The pinned source matches 96 contact impulse/damage/RNG cases, 42 selection/RNG cases, six default-profile distances,
30 difficulty/lives activations and 45 actual generated-robot shield values.
Implicit high-ID selection uses explicitly synthetic definitions in the native
control; production still requires valid admitted assets and retains expansion
gates. This is not a claim of expanded HAM/HXM support

Native runtime also passes equal-boundary waiting/firing, actual object-table
exhaustion and retry, player/robot occupancy damage, global population suppression
and recovery, three exhausted waves/lives, final disable-frame behavior, user
matcen modes, stock timer/health isolation and source recovery. Seven corrupted
generator restore controls reject before publishing extension event state.
Generator source assignments and candidate robot IDs enter the source identity;
generator saves require the checked extension, and restored table links/contents
must match the source

Android passes 53/53 steps and six production rewind restores, run ID
5852fd1b2af5450f8c6a1947b1edd1f7, elapsed 152589 ms. Pending effects, spawned
robot identities, native combat, disk restore and stock-to-source restoration
pass with exact protected-file hashes. The initial run exposed the test-only
suspension flag resetting on load; the corrected script reapplies that control
before inspecting pending state rather than waiting for production to advance

Tested APK SHA256:
a9df1c3e05375a96e368b641a3cd049307686c06a6d299e774a5ef901d842b22

The maintained [evidence](../../XL_compat/evidence/robot-generators.md) records
fixture hashes, remaining limits and original-level findings. The roadmap remains
active; completing this chunk does not complete the overall gameplay roadmap

The object-event PR's Linux include order was corrected and merged into cmake:
object_trigger.h must precede trigger_state.h because rewind_file.h introduces
engine min/max macros. The old source reproduces CI's error under local GCC;
the corrected production translation unit passes syntax compilation

## Finalization

The user authorized commits and pushes to finalize xl_more_8 and its CI.

1. Rebase only generator commit 515b6693d onto origin/cmake after PR #150 merged.
   Complete: range-diff drops only the already-merged trigger include fix. The
   complete tree difference consists exactly of five upstream settings-test files.
   The original commit remains at backup/xl-more-8-before-cmake-rebase-20261026.
2. Reconcile the final 53-step Android and 96-case contact evidence. Complete:
   the retained result, APK hash, native reference log and protected-file hash
   reports match the evidence above.
3. Finish scoped formatting, both automation catalogs, runner-policy tests and
   the focused native/reference sweep on the rebased tree.
4. Push xl_more_8, open a draft PR to cmake, and monitor its CI to terminal
   success. Inspect and fix actual failures without weakening test gates.
