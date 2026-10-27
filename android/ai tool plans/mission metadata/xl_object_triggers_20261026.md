# Object-attached trigger chunk

Continue the singleplayer gameplay roadmap after wall scheduling. Work on this
coherent chunk until runtime, persistence and integration evidence are complete

1. Decode bounded object action records and owner references for 15/33, 17/36
   and 23/40. Preserve raw fields, separate reference normalization from source
   evidence and compare ordering with the pinned loader
2. Implement object events through production robot damage, removal and reactor
   destruction calls. Distinguish damage-threshold teleport/spawn actions from
   removal actions and frame autoplay; preserve actor and action order
3. Carry supported leaf actions, chains and flag transitions through shared
   dispatch. Object teleports use segment centers and retain orientation/motion,
   unlike wall teleports. Audit nonplayer action and child dispatch semantics
4. Persist object ownership/consumption, action flags/values, autoplay and last
   teleport destination with source identity and atomic validation. Verify disk,
   rewind, source changes, object slot reuse and stock isolation
5. Compare independent pinned reference controls, exercise real combat/removal
   and combined chains in native and Android integration, then update admission
   and compatibility descriptions only for verified requirements
6. Inspect original mission requirements and retain useful evidence; derived
   controls alone must not qualify an original mission or the whole roadmap

## Reference audit

Revision 2576a98b5de310e3ac7c9777df2677d1627b8bae

- `gameio/loadobjects.cpp` SHA256
  `01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c`
- `main/trigger.cpp` SHA256
  `74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852`
- `objects/updateobject.cpp` SHA256
  `c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac`
- `physics/collide.cpp` SHA256
  `d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9`

Object records have 16-bit flags and occupy 53 bytes. All records precede owner
references. Game 40 stores one signed object index per trigger; older versions
store ignored previous/next links and an object index. Pre-36 also retains 700
signed heads; game 36 retains a signed count and sparse object/head pairs. The
reference rebuilds groups by sorting owner then action type, ignoring old links

`CTrigger::Read` resets object master values and overrides object times to -1,
except countdown/message/sound. These source fields must remain visible even
when runtime normalization makes them inactive

`DoExecObjTrigger` selects teleport/spawn-bot actions for damage events and all
other actions for removal. Threshold compares truncated remaining-shield percent
against `10 * (10 - authored value)`; values at least one fixed unit are first
converted to integer. Successful nonpermanent damage activation clears value
before dispatch. Damage hooks run only for nonlethal local-player robot damage

Removal consumes ownership even when an action is disabled or not selected for
that event. Reactor destruction activates before later cleanup. Object autoplay
is frame-driven, executes with the owner and disables afterward, including
permanent actions. Supported object actions do not retain ordinary wall delays

Object teleport picks a destination segment, avoids the previous destination
when there are multiple links, and moves only when changing segments. It retains
orientation and velocity. Boss teleport also rebuilds boss state; this requires
explicit qualification rather than silently using wall teleport behavior

Object master dispatch passes object-trigger context into linked wall actions,
while Operate computes an index using the object array. The pinned release
CArray::Index subtracts unrelated pointers on that path; debug builds instead
reject a foreign pointer. Audit the intended child semantics and explicitly
document any correction rather than treating this undefined indexing as an
independent gameplay oracle. Equal owner/type groups also need the reference
quicksort's tie ordering, not an assumed stable sort

## Current acceptance state

In progress. The local runtime admits verified native leaf requirements;
object masters and the other documented requirements remain gated. Android,
the remaining independent comparisons and full admission/browser evidence are
required before this chunk is complete

Parser checkpoint: five focused native CTests pass, including all three
derived reference layouts and the original GlassHouse records. Scoped mixed
formatting/lint passes

Runtime checkpoint: nonlethal local/nonlocal damage, actual lethal explosion/
cleanup, removal, reactor, autoplay, alternation, ordered wall enable/disable,
disk/source/stock restoration and slot reuse pass. All 1456 production selection
samples match the pinned event/shield oracle; 16 corrupt block controls reject
atomically across wall and object arrays. The focused native selection passes
11/11, the shared x86_64 Android APK builds, and scoped formatting/lint passes.
Continue this same chunk with object
master context, independent teleport/sort controls and Android disk/rewind tests;
do not publish this checkpoint as completed object gameplay support
