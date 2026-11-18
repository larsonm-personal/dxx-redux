# Bounded readiness staging follow-up

Runtime #581 passes all 64 frozen cases locally in 382.579 seconds with
pre-provisioned inputs. The parent CI measurement stopped incomplete after setup
left 309 seconds for probes. Profile immutable input work before changing any
measurement rule. staging-profile-01 found 7.56-8.09 seconds per selected
levelpack case, of which 6.14-6.63 seconds was repeated archive extraction and
about one second copied the 1,158,986,029-byte package. Dinter extraction was
1.60 seconds. This supports eliminating repeated extraction, not changing native
simulation, case selection or watchdogs.

Implement an optional per-run archive extraction cache in the trusted readiness
runner. Verify original archive size/hash on every use; publish a completed
extraction only after hashing its files and recording its identity. Before
reuse verify every cached path and byte, rejecting missing, added, changed or
symlinked entries. Keep each case's full independent writable package copy,
user directory, process, seed and ordinary developer-OFF engine unchanged.
Never pass the cache directory to the engine as an asset mount. Fresh worker
reports and existing timeout/failure classification remain authoritative.

The expanded supervisor owns the cache in a temporary sibling of its output,
outside uploaded evidence. Remove it on normal, failed and budget-cancelled
completion. Retain only bounded diagnostics, never cached private assets in
results artifacts. Do not share across runs or relax the 45-second start guard,
per-case timeout or ten-minute total CI cap. The original eight runner remains
uncached unless explicitly requested by its caller.

Validate real fixture extraction, full package byte identity, fresh writable
case isolation, changed source/cache rejection and no stale partial reuse.
Exercise supervisor process cancellation and cache cleanup with its existing
integration tests. Compare a separately labeled local frozen-64 run against the
retained uncached result; keep both receipts. Public CI can qualify the code;
the trusted private controller only uses it after user adoption. Do not bypass
that controller or merge the stack.

## Qualification

cached-expanded-01 passes all 64 frozen cases in 229.797 seconds against runtime
#581, using the same 600-second pre-provisioned probe budget as the retained
uncached single-strip-expanded-01 (382.579 seconds). Every case identity,
outcome, frame count, level and XL-active value agrees. The cache was removed
and the retained evidence contains only the existing bounded logs/JSON files,
not decoded assets. The harness source/binary hashes are separately recorded in
cached-expanded-01-harness.json; this does not replace the uncached trusted CI
64/64 receipt. These are descriptive host timings, not a controlled throughput
guarantee. A final edit removed punctuation from a comment only.

Nine expanded-runner integration tests pass, with the actual symlink control
skipped on this Windows host because link creation is unavailable. Linux CI must
exercise that control. The 56 related private-asset/receipt controls pass after
adding the installed CMake/CTest directory to PATH; the first missing-tool
failure remains in staging-private-regression-01.log. Both automation catalogs
and scoped mixed formatting pass. All original measurement rules, process
isolation, current outcome classification and controller review requirements
remain unchanged.

The new cache is temporary and sequential. It revalidates source and cached
bytes, preserves directories and full independent package copies, rejects stale
partial extraction instead of silently retrying, and never uploads cached
assets. Public exact-head CI and trusted-controller adoption remain separate
acceptance steps. The original eight command remains uncached by default.
