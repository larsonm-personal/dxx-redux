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
