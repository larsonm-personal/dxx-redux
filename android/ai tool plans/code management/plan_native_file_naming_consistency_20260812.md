# Native file naming consistency plan - 2026-08-12

## Objective

Correct the Redbook interface filename so its implementation and public header
share the `rbaudio_bin` stem, then audit every branch-added C, C++, and header
path for comparable ownership-name mismatches. Preserve names that communicate a
real architectural distinction, such as entry points, backend implementations,
header-only policy or type owners, compatibility shims, and aggregate stubs.

## Work plan

- [x] Freeze the merge base and enumerate all branch-added `.c`, `.cpp`, and `.h`
  paths
- [x] Rename `rbaudio_android.h` to `rbaudio_bin.h`, including its guard, paired
  inherited includes, focused test ownership, and current campaign records
- [x] Review every mechanically unmatched source and header using declarations,
  consumers, and build ownership rather than filename similarity alone
- [x] Correct any additional unambiguous mismatches and update include sites,
  build files, tests, and current documentation
- [x] Publish the inventory, exception rubric, decisions, and exact metrics in a
  tracked survey report
- [x] Run focused contracts, scoped quality, D1/D2 host builds, Android ABI
  builds, and final path/guard/reference audits

## Naming rule

A source and header use the same stem when the header is the unique public or
private interface implemented by that source. Different stems are retained when
they describe different ownership: entry points and JNI bridges, a backend that
implements an inherited API, header-only policy/types/limits/compatibility,
multiple implementations of one interface, or one aggregate implementation of
several interfaces. Include guards normally derive from the complete header
filename; upstream-compatible replacement headers may retain the upstream guard
when that identity is required.

## Completion

The 442-file inventory found the Redbook source/interface mismatch and one
guard-only omission in `net_udp_android_autonet_shared.h`. Both are corrected;
all other cross-stem cases were classified as intentional ownership boundaries
in the tracked survey.
The focused three-test contract passed, the complete Windows D1/D2 build passed,
and Android external native builds passed for arm64-v8a, armeabi-v7a, and x86_64.
Scoped formatting, path/guard/reference audits, and `git diff --check` passed.

## Corrective 1:1 ownership pass

The initial survey over-relied on filename categories. This follow-up treats
exported declaration ownership as primary and requires evidence for every
cross-stem exception.

- [x] Extract declarations from every branch-added unmatched header and map each
  to its defining translation unit
- [x] Identify headers whose externally defined interface is owned wholly or
  overwhelmingly by one differently named source
- [x] Rename every clear 1:1 source/header pair, including guards, includes,
  build registrations, tests, and current documentation
- [x] Replace the initial survey conclusions with the declaration-level mapping
  and retain only demonstrated split, inherited, header-only, or entry-point
  exceptions
- [x] Rerun focused contracts, scoped quality, D1/D2 host builds, Android ABI
  builds, and final stale-name/path audits

Completion: the declaration-level pass corrected four additional 1:1 source
names: `android_music_control.c`, `net_udp_reconnect_jni.c`, `secretarea.c`, and
`pngfile.c`. A maintained naming contract covers all five corrected families and
rejects their obsolete paths. Fourteen focused tests, scoped quality, complete
Windows D1/D2 builds, and all three Android ABIs passed.
