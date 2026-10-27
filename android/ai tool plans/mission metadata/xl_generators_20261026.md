# XL robot generation chunk

Continue the singleplayer roadmap after object events. Branch xl_more_8 starts
above the three object-event commits in PR #150. Bounded robot-matcen behavior
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

The pinned source matches 42 selection/RNG cases, six default-profile distances,
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

Android passes 50/50 steps and six production rewind restores, run ID
6c855bcc862b4d2580374bdbfb4ca907, elapsed 129572 ms. Pending effects, spawned
robot identities, native combat, disk restore and stock-to-source restoration
pass with exact protected-file hashes. The initial run exposed the test-only
suspension flag resetting on load; the corrected script reapplies that control
before inspecting pending state rather than waiting for production to advance

Tested APK SHA256:
485d72a608a627a9418a71fd50068bb3bbaa2544f8b5e1e06642754108f511a6

The maintained [evidence](../../XL_compat/evidence/robot-generators.md) records
fixture hashes, remaining limits and original-level findings. The roadmap remains
active; no commit or push is authorized by this chunk's completion

Also found and corrected locally the object-event PR's Linux include order:
object_trigger.h must precede trigger_state.h because rewind_file.h introduces
engine min/max macros. The old source reproduces CI's error under local GCC;
the corrected production translation unit passes syntax compilation. No changes
from this continuation have been committed or pushed, per the user's instruction
