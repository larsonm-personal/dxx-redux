# Chunk 0387 physical-target graphics eligibility candidate, 2026-10-11

Diagnosis and implementation planning only. No canonical finding ID is admitted by this plan. Proposed severity P2/high; authored tooling only, no inherited-file saving.

## Source-backed trigger and result

The master initializes the selected target before classification. A physical target forces SingleDevice. test_graphics_recovery is not in the two-emulator, server, extract or host-only lists, so it receives Requires=emulator. This requirement is reused for physical single-device tests. The physical exclusion list contains only xcrash native report, cooperative save compatibility and LAN active discovery. ExtendedGraphics retains graphics recovery. Invoke-SingleTest forwards the selected physical serial to its Serial parameter; the entry forwards it to run_graphics_recovery_tests.ps1, whose emulator-only guard throws before creating output or changing fault state.

Consequently an explicit physical target with -ExtendedGraphics can schedule an incompatible fixture and record a failure before graphics assertions. Automatic extended graphics sampling can also enable this route. This is a source witness; no actual suite, fixture or device execution is claimed. Upstream filtering beyond the named admission route is not claimed fully reviewed.

## Ownership reconciliation

BR-0539 owns serial identity and bounded setup waits; the current route uses the selected serial and correctly rejects physical fault injection. GQF-0299/GQR-0285 owns automatic sample eligibility for explicit caller-artifact probes missing Aab/Apk. This fixture requires an emulator target rather than an external artifact. Neither definition directly describes the incompatible physical graphics scheduling root. Current BR definition keyword census located BR-0539 and FIXED BR-0604 (provisioning deletion scope); no additional matching target-eligibility definition. Final full chunk reconciliation and canonical admission remain pending.

## Minimal proposed change and acceptance

Retain the recovery helper's emulator restriction. Exclude graphics recovery from physical suites at the existing physical eligibility filter with a specific skip reason. Preserve selected-serial forwarding, physical isolation, normal emulator extended rotation, explicit emulator invocation and both-game fault coverage. Avoid broad requirement renaming or catalog abstraction unless further evidence requires it.

Future maintained acceptance should exercise actual suite selection/dispatch using recorded children and stubbed target discovery: physical selection with explicit ExtendedGraphics, ordinary automatic graphics sample enabled, sample disabled, and explicit graphics filter; require no graphics recovery child and an accurate skip record. Default and non-default emulator targets with ExtendedGraphics must retain recovery and receive the selected serial. Retain direct physical helper rejection before output or fault mutation. Preserve HostOnly/ListTests no-device discovery and unrelated physical tests. Run relevant catalog/target fixtures only during an authorized implementation tranche.

## Formatter fixture limits

The complete 88-line test_formatter_process_cleanup.ps1 was read without running it. It uses the production matcher and stopper, synthetic invocation/identity controls and actual child-process fixtures. It asserts preview does not kill, selected checkout parent/descendant retirement and sibling preservation. Its finally block sequentially stops/disposes children, then disposes the producer lock and deletes the root; an earlier cleanup throw can bypass remaining cleanup. Existing BR-0543 cleanup ownership applies. The fixture does not prove abandoned-versus-live quality-session admission or atomic exclusive quality ownership; retain BR-0538/0603 acceptance. No new formatter root or runtime credit.

## Remaining publication gates

- [ ] Complete chunk consumer and current-owner reconciliation
- [ ] Decide canonical admission and directly applicable rating
- [ ] Publish complete chunk report/snapshot and independently audit before DONE
