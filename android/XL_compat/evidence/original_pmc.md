# Original PMC admission and resources

The original `pmines.hog` SHA256 is
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`.
Admission now recognizes stationary camera objects with the decoded 16-field AI
payload and 33-field polygon payload, behavior 128/129, and ordinary object-bound
monitor triggers. Other actor shapes, flags, timed bindings and wall-camera
viewpoints remain rejected. PMC3 still requires behavior 135 and fixed-end
lightning qualification.

The PMC1/2 cameras are authored as behavior 128, which stays still. Pinned
`CCamera::Rotate` advances only behavior 129 (`AIB_NORMAL`). Live checks must
preserve the original still-camera matrix, rather than require visible rotation.

## Original Vertigo dependency

PMC1 contains original robot IDs 68 and 76. The base D2 table does not contain
these robots. Pinned `gameio/loadgame.cpp::LoadModData`, stage zero, tries
`LoadRobotExtensions("d2x.ham", ..., 2)` for D2 independently of the enhanced
mission descriptor flag. This is an original asset dependency, not a replacement
robot or rewritten mission.

Audited source revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`

- `gameio/loadgame.cpp`: `5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55`
- `gameio/loadmodeldata.cpp`: `4b191d4abf1a3a68348b468bd6db77da805c2334ba9d49d623697c294c9bfb8c`
- `gameio/loadobjects.cpp`: `01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c`

The XL mission initialization hook uses the existing validated Redux extra-HAM
decoder before capturing the mission resource baseline. The qualified original
extension is 147471 bytes, SHA256
`faa67189ccf5c7d6cdc8e37fa009fb21a1d57ee2c6c212ce091642e4366e3c0f`.
It adds 12 robots and three weapons. Missing optional expansion data leaves the
base definitions intact; levels requiring its robots remain blocked by ordinary
dependency checks. A present unqualified extension is rejected before decoding.
Ordinary non-XL mission loading retains its existing rules.

PMC1 robot 90 stores model 168 while its saved POF-name table has only 166 entries.
Both pinned `CObject::Verify` and Redux `verify_object` handle polygon robots
before saved-name remapping and replace their model from the loaded robot
definition. Preflight now follows that order for polygon robots; all other
saved-name bounds and the resolved model's dependency checks remain active.

PMC1 camera 102 retains serialized model zero when `robot09.pof` has no loaded
name match. Pinned `CObject::Verify` likewise retains the saved index in this
case; `InitCamBots` reads its radius before installing the camera addon. Camera
preflight now preserves that valid index, while still checking model bounds and
dependencies. This loading branch was source-audited; the existing compiled
camera initialization oracle covers the subsequent radius and addon ordering.

The original extension can be loose or inside an owned `d2x.hog`. Archive loading
uses a temporary private mount. Android separately mounts the selected mission's
parent after recording the descriptor search root; addon ownership recognizes
that exact selected parent as well as the existing descriptor/archive/base roots.
Unrelated search-path shadows remain rejected. The playtest installer stages
`missions/d2x.ham` and `missions/d2x-xl.hog` and backs up existing copies.

## Validation boundary

PMC1 has 11 and PMC2 has 25 robots authored with behavior 135 (`AIB_STATIC`).
Pinned `ai/aiinit.cpp::InitAIObject` sets `CT_NONE` and `MT_NONE` before ordinary
initialization, even when the robot's melee profile subsequently changes its AI
behavior byte. The XL-only initialization hook now preserves these static
displays; ordinary projectile collision and damage remain active. This branch
was source-audited against SHA256
`e37a1a2a94df3f72e20916366f3225d7dc2bd44359979a95ea0b442b6e87c570`.
Native controls cover static/ordinary initialization and damage in XL and stock,
plus a static object's full save through a stock level. Device probes check every
original static owner before and after restart and save restoration.

PMC1 places five extra-life pickups at the spawn. Restart checks compare with the
captured life count and require exactly one life consumed; they do not assume the
opening hub's three-life starting count after these ordinary pickups.

Original PMC1 has 50 lightning bolts around fixed-point Y = -68691002. Their
shared segment-light position sum can exceed 32-bit range before averaging.
The original source accumulates signed 32-bit coordinates; this undefined
overflow is deliberately repaired with a 64-bit sum and the same final integer
average. Sample count, colors, summed alpha and brightness are unchanged.
Existing source comparisons cover the defined range; a separate original-
coordinate regression checks both signs and repeated reads outside that range.
This is a numerical repair, not a claim of source parity for undefined overflow.

Inventory controls reject unsupported camera shapes and bindings. Native
projectile controls check malformed extension rejection without definition
mutation, mission reload recovery, original extension loading, and the separately
mounted selected-parent layout. Existing camera combat and full save/stock/restore
controls run with and without the original extension.

The protected Android PMC scenario uses the unchanged original mission, authored
exit dispatch, fixed firing positions, ordinary input, death/restart, disk saves,
monitor pixel sampling and graphics context recreation. These controls do not
establish human traversal through every door, objective or secret. The original
PMC1 bundle was delivered from ea25a8586a95aacee34382ba9be1e7adad72c444; PMC2
device qualification below covers the subsequent silent-mapping fix.

## Original PMC2 silent sound mapping

PMC2's original HXM replaces robot 65 (model 92, 100 shields) and sets its see,
attack and taunt cues to logical sound zero. The HXM SHA256 is
`5c803307f6af290f86eac47ae90ba233f2ca73a38e42e3f94ed4d98de724c4e8`.
The trusted original sound table maps that entry to 255. Pinned
`CAudio::XlatSound` explicitly returns -1 for
255 in either the alternate or main mapping table, so this is a silent cue, not a
missing sample. Resource admission now recognizes that sentinel before checking
the resolved sample. Invalid logical/table indices and mapped sounds with absent
sample data remain rejected. No HXM bytes or runtime sound mappings are changed.

Audited `audio/objectaudio.cpp` SHA256:
`e44484af297ce0c20c92feb00bfb2795316980434e3f1e1d85194c1d46b992e3`.
Native controls exercise ordinary/low-memory silence, alternate silence, a real
mapped sample, and missing-sample/invalid-index rejection. The 114-step lifecycle
run `7790e5cf218c4acbb74ac37d082c8fc1` passed in 221309 ms. The expanded 117-step
run `1886847229b64b34a0e74a480f10918e` passed in 229829 ms, including ordinary
combat against original active PMC1 robot 89 and PMC2 HXM robot 174. Both levels
passed camera contacts, monitor sampling, static-display preservation, exact
death/restart life consumption and full disk restoration. PMC2 additionally
passed background/context recreation and retained the PMC3 admission gate.
Protected before/after app-data hashes matched in both runs. Only x86_64 was
device-tested; universal packaging and CI are recorded separately.
