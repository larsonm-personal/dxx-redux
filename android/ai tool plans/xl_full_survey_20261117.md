# Full original level survey

Run the ordinary 120-frame readiness probe for every descriptor and advertised
level in the pinned archive inventory, including secret levels. Record each
archive/descriptor/ordinal identity and preserve a private engine log and load
receipt for each failure. The existing 64-case CI gate continues on PRs. The
full survey is an on-demand command that can run one archive or the whole set.

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
