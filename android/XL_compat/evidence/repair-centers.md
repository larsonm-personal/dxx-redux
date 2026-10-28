# Modern XL repair centers

Modern 23/40 function 2 now repairs the local player's shields through a separate
XL path. Redux retains its existing repair implementation. Legacy encodings,
optional ship/producer settings, team ownership and multiplayer remain gated.
Derived controls qualify this feature without qualifying original missions

## Source and profile

Pinned reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

reference_repair.cmake extracts the complete CSegment::Repair body and exposes
its static clock only for sample setup. The function has a column-zero nested
closing brace, so extraction uses its pinned final return rather than assuming
that the first such brace ends the function. The independent adapter links no
production repair helper. The real CObject::HandleSegmentFunction caller is also
extracted, with unrelated unsupported segment damage and refueling inert in
the dry/repair fixture profile

Pinned source SHA-256 values:

- main/producers.cpp: c79263fcd98f686d99dfcc8f5e12aade60f70f75cc25544795fbab71c93318b6
- network/network_init.cpp: 6732bb4033ebfdfc0f17563a04beeed1b5620c53c91e36f7b6e798326158c8a2
- objects/updateobject.cpp: c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac
- include/player.h: 4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4
- render/automap.cpp: 19b7128b48dadea220a085018e0fd3a2ff31d59fa04d60fce78d4bb1f5877be8

The default source profile uses 11 shield units per second. The actual player
caller supplies InitialShield minus current shields, giving a 100-unit cap for
the default ship. Overfull shields remain unchanged, and negative shields do
not resurrect a dead player. Repair does not replenish energy and does not use
the no-damage property to suppress healing

The cue uses SOUND_REFUEL_STATION_GIVING_FUEL at half volume, with a strict
quarter-second interval and the source backward-clock reset. The automap uses
palette RGB (0,15,31), before normal door/secret/revealed overrides. Optional
decorative repair lightning and sparks are deferred

## Native and persistence controls

5,760 samples compare exact amount, final cue clock and cue count against the
extracted source. They cover eight dry/repair/property combinations, five frame
durations, six remaining-capacity values, six game times and four cue clocks.
MSVC and Linux GCC with -Wall -Werror pass

Forty-two actual player frames match the extracted source caller's final shield
and energy values. They check negative, zero, partial, near-full, full and overfull
health without changing energy. Water, blocked and protected-water/lava repair
centers retain independent properties. Physical dry/repair crossings confirm
that segment functions use the pre-movement room and change on the next frame

XL owns the repair cue clock separately. Development trailer schema 10 directly
replaces schema 9 and stores elapsed time rebased onto the restored game clock.
Mission transitions reset the clock. Disk restoration through stock preserves
source function, health and the pending cue deadline. Eight malformed and
cross-feature-corrupted trailer cases reject atomically. Backward time and
overflow boundaries have explicit controls. Stock dispatch leaves the player
unchanged and a real Redux repair call retains its original 25-unit rate

## Assets and original candidates

The derived repair fixture uses the existing water-actor geometry, scaled
fourfold from hash-verified original Neptune bytes. Segments 60 and 62-67 are
repair centers; 61 is dry. Property combinations cover ordinary, water, lava,
blocked, protected, protected water and protected lava. Originals are unchanged

- Original neptunxl.hog: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived repair.hog: cd1ef589c362e9449004e58c740124404c27267ab84a90b17b9397ad1120b031
- Derived repair.mn2: e66dadc31bb929f865299cb914740c23641ce77ae120bf64c04bccf881010a8a

[repair-candidates.json](repair-candidates.json) reinspects 28 surveyed modern
levels with 145 repair centers. Twenty-one decode and seven retain explicit
rejections, including the unsupported extended D2X container. Every record
retains runtime_qualified=false. Counts and decoding are not gameplay evidence

Repeated audit preserves report bytes and modification time. All 15 focused
regressions pass in 161.38 seconds; the extended caller comparison passes in a
subsequent targeted run. All 12 private-policy tests pass, requiring 64 core
feature tests. Both catalogs pass: 403 top-level entries, 458 support scripts,
99 standalone JSON tests and 290 standalone PowerShell tests

## Android acceptance

The registered test_xl_repair_runtime runner checks healing, cap, live mixer
playback, physical entry/exit, mixed protected fluids, automap, disk through
stock and rewind. It stages only pinned derived assets and restores protected
pilot/config/save files. All 70 steps pass on emulator-5554 in 84,977 ms, run
6893e87db894496e915df3db6c320c24. APK SHA-256:
43ac737fc3dc97e684a4f3e9c0bc702a63814de475ad78312b59b50558f94f08

Protected file hashes match before and after; emulator-5556 was untouched. The
first attempt failed during script parsing because four axis steps used value
instead of axis_value. The corrected script completed the whole integration;
the failed attempt restored protected files and provides no gameplay evidence
