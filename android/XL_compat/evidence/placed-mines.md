# Placed-mine model admission

The original Pumo `mvlev6.rl2` stores object 4 as weapon 51 with polygon model
index 0, named `robot09.pof` in its saved model table. Requiring that saved name
rejected the authored transition from level 5 before the ordinary loader could
normalize the mine.

At pinned D2X-XL revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`gameio/loadobjects.cpp` lines 158-165 replace `SMALLMINE_ID`'s model from the
weapon definition and normalize its physical properties. File SHA256:
`01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c`.
`include/laser.h` defines that placed-mine ID as 51. Redux already performs the
same replacement for `PMINE_ID` in `d2/main/gamesave.c`.

XL admission now resolves exactly this weapon ID through `Weapon_info` after
validating its weapon dependencies. The resolved model still passes the normal
model dependency checks. Saved model index bounds and other objects' model-name
resolution remain checked. This does not install or substitute external assets.

The original-route Android probe checks every placed mine on arrival in level 6
for the weapon model, polygon radius, mass, drag and free-spinning flag, and
requires a nonempty mine population. It uses original mission files and authored
exit dispatch, not an earned player traversal.

The first corrected Android route reached level 6 and passed model/physics
assertions, then rejected departure while capturing projectile histories.
This exposed a second representation mismatch. XL `ReadObjectInfo` clears the
object array before `CObject::Read`; level weapon records contain only parent
fields. `physics/collide.cpp` uses `nLastHitObj` as a count, so zero means empty.
Redux's similarly named field is a last victim index, where -1 means empty.
Fresh modern XL level loading now initializes that index to -1 for weapon
objects with weapon control. It does not relax save validation or discard saved
histories: the save loader restores its object records and XL history afterward.
The route probe also checks the empty sentinel on the original placed mines.
Native earned-history save/restore passes, including stock isolation, duplicate
hits and fourteen malformed-history controls. The source reference consumes the
completed native samples: 1,920 impacts and 1,650 history controls pass. The first
CTest wrapper invocation failed only because its reference executable was absent;
the native owner completed successfully, and the built reference then passed
without repeating that gameplay run.

Android `test_xl_pog_runtime.ps1 -OriginalHub -MissionFlow` passes all 41 steps
in 202.834 seconds. Original authored exits take the route
1 -> 2 -> 3 -> 4 -> 14 -> 5 -> 6 -> 7 (`tulev1.rl2`), followed by hub checkpoint
recovery and GPU checks. The level 6 control also removes the required weapon
model temporarily, verifies explicit admission rejection, restores the model
and verifies recovery. Protected before/after hashes match.

Receipts are under `temp/xl-skybox-20261031`:
`android-original-mine-history-flow.log`, `mine-history-native-test.log` and
`mine-projectile-reference.log`. Native gameplay details are in
`temp/xl-particles-20261030/contracts-windows64/projectiles/controls.log`;
Android artifacts are in `temp/xl-original-flow-android`.
