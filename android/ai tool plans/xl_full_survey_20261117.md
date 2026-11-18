# Full original level survey

Run the ordinary 120-frame readiness probe for every descriptor and advertised
level in the pinned archive inventory, including secret levels. Record each
archive/descriptor/ordinal identity and preserve a private engine log and load
receipt for each failure. The existing 64-case CI gate continues on PRs. The
full survey is an on-demand command that can run one archive or the whole set.

To group a completed run, use
`python android/tests/xl_readiness_full_report.py --work RUN --catalog INVENTORY --output GROUPS.json`.
Use the exact inventory supplied to that run. The report checks the inventory
hash, each case receipt, and the simulation proof for every pass. It publishes
the case IDs, outcomes, completed frames and first blocker. Full engine logs
stay in the run directory. A later blocker can surface when an earlier one is
repaired.

The proposed hosted full survey uses the `xl-full-survey` PR label. Add it
before the next XL asset request on the owned stack tip. The controller reads
the label from the same open PR whose head it verifies. On adoption in
`cmake`, a labeled request will measure every case in the pinned inventory
and upload `xl-full-readiness-groups`. Additional local distributions need
their own inventory and local inputs. The ten-minute 64-case job reports
independently. A manual dispatch on a feature branch exercises the ordinary
engine build only.

First qualify the runner against the existing binary and authentic staged
inputs. Count completed, failed and untested cases against the inventory. Group
failures by their first actionable engine diagnostic, inspect representative
level records and qualify fixes with targeted reproductions and the ordinary
probe. Rerun affected families and the original 64 to verify recovery.

## November 17-18 execution window

Target November 18 at 10 AM PST. Reserve 6-10 AM PST for validation, CI
repair and handoff. Work from the published 64-case stack rooted in cmake
d85d6afc5209a0c772fe18f814c4533839a9eea2. The immediate parent is #582.

1. Inventory all 251 local downloads by content hash. Match the 23 pinned
   archives and enumerate additional local distributions. Run all 1,111
   pinned and 543 additional advertised cases, including secret ordinals.
   Record the packages without usable descriptors separately.
2. Reconcile receipts one-to-one with both inventories. Inspect the first
   failure for each case. Distinguish rejected formats, mission containers,
   resources, engine behaviors, failed simulations and incomplete measurements.
   Preserve release variants and identify further blockers exposed after repairs.
3. Qualify the contained-object death path against the XL reference, then run
   AF-XL levels 6, 7 and 9. These levels contain robots with authored loot
   types outside the two spawnable classes. Preserve authored records, native
   save state and random stream behavior during death and restore.
4. For the 17 Belial System load failures, compare duplicate-name selection
   with the pinned source's archive sort and binary search. Check inspector,
   game, Android import and music owners before changing production selection.
   Investigate other shared families in descending impact once the census finishes.
5. Publish tested checkpoints to the fork. Record exact candidate CI, frozen
   8 and 64 results, original failing-case reruns, and remaining work.

## AF-XL robot loot

The additional af-xl.7z distribution passed 12/15 on the baseline.
Alarlof, Cefispar and Lakshmid stopped on five robots. Their contained-object
types were 8, 9, 51 and 19. The pinned XL source at 2576a98b resolves these
records in main/dropobject.cpp (DropPowerup and CObject::CreateEgg).
Only robots and powerups have a spawning branch. The default returns -1
on the first attempt, without allocating an object or drawing randomness.
In effects/fireball.cpp the level-version-10-and-later death path then
checks the robot definition independently. Chance and quantity use Rand(n)
in include/maths.h. An initial candidate that only owned the empty-drop
path passed 13/15 and identified the second requirement in levels 6 and 9.

The native death owner reuses its powerup spawning, replacement and tracing.
XL admission checks the original object and the actual staged robot definition.
Secondary robot, thief and companion profiles remain gated for inert placed
contents. Native controls exercise delayed deaths at three probabilities,
spawn counts and SIM draws, plus uninterrupted, warm and cold save futures.
The developer-OFF Windows candidate passed all 15 original levels with binary
097fe2a9f0e449b1c0d660544956430cb1b4acd50bfa835d05c03e922ba5a646.
The separate native controls deliberately set up robot state and deaths.
The older levelpack distribution has independent light-index failures.

## Absent trigger sections

ReadTriggerInfo in pinned gameio/loadobjects.cpp resets the object-trigger
count and references before testing the section offset. A directory entry
with offset -1 and count zero therefore describes an empty trigger set.
The inventory used to report an unknown capability for that case. Positive
counts still require data, and present sections still require a complete
object count and bounded targets. Layout controls distinguish all three
cases across four modern formats. The native 120-frame reruns use original
levels from the pinned level pack and additional local distributions.

## Format 26 with game data 40

The additional inventory exposed 24 originals with this version pair. The
pinned reader uses the same wide producer/segment fields and explicit side
corners as version 25. Fog records begin at version 27. The ordinary engine
now admits 26/40 through the same geometry, resource and behavior checks.
Fourteen originals completed 120 frames. Ten reached downstream blockers.
The extracted source agreed on 23 original layouts. Speed stopped earlier on
its invalid reactor trigger count.

Windows native controls exercised geometry, lighting, transitions, rejection
and restoring earned state through stock. Six derived levels supplied 48,406
independent layout comparisons. The damage controls covered 8,400 native
frames. Android ran 87 steps on emulator-5554 in 149.602 seconds, run
9b67af2c019748a19c410b6d3352d4ed. The first device attempt stopped at the
first transition while graphics was still paused. The scenario now waits for
resumed simulation before requesting that transition. The second run passed
using APK f077ae866068cf0b3f5e941352147d3cc3c510068e3af2076ea8e0cd7df91de6.
The before and after protected player/config manifests matched. These device
controls use a labeled derived mission and explicit test actions.

## Original capture and replay

The Debug lifecycle owner accepts `-runtime-layouts-test original-v26-replay`
with `-mission`, the original staged package in `-extra-dir`, a private
`preview-user` inside it and the verified retail runtime in `-hogdir`.
It starts level 1 at Hotshot with SIM seed 1 and records 120 fixed frames
with neutral controls. It reloads the recorded checkpoint and compares the
object, world, native XL and simulation RNG states using the existing
continuation comparison. The initial-state diagnostic records the placed
robot definitions and the first simulated frame when playback stops early.

KKR-XL level 1 matched all 120 frames, the embedded result and the
continuation comparison. Capture and replay ended at SIM state 4015624929
after 32 draws. KAK-XL level 1 completed the capture and stopped on the
next input boundary during same-process checkpoint replay. Both starts had
SIM state 1, zero draws, player belief in segment 338 and the same robot
definition flags. In the first frame, two thieves entered attack and consumed
116 SIM draws on capture. Both stayed in wait and consumed zero on replay.

A separate instrumented run traced the gameplay distance cache. The original
queries from segments 419 and 79 to 338 missed, returned -1 and stored
65536000. The replay queries hit those retained entries and returned 65536000.
The thief checks distance against 500 units before building a path. The
reference `ai/thief.cpp` also uses that comparison, and
`main/findpath.cpp` distinguishes a first failed search from a cached
distance. The production follow-up is to retain and restore the gameplay
cache at the checkpoint boundary. Acceptance includes nonempty rings,
expired entries, cross-level identity, old recordings, and warm and fresh
process replay. The diagnostic added no random draws or replay reseeds.

Both saved recordings also ran from a fresh process with the ordinary Windows
OpenGL executable, SHA-256
7c80ed41fe671b6748f525e36ca9279b4c0ce2d3256a24018860be52efaf8921. Each
completed 120 frames, matched its embedded result exactly and reproduced its
entire captured RNG trace byte-for-byte. The ordinary true-headless timeline
guard rejected both recordings before replay. The separate headless
load/tick test passed both levels.
