# Original XL fixture CI

Base: cmake 72dba642e97ef1e3b65079cec6b32b4d4ec545de. Private inputs:
dxx-redux-test-assets 8c9988a96a7e306f8aac04c85e2610492917b7ac,
missions/d2xxl/manifest.json. PR140's unmerged 23/40 implementation is excluded.

Keep the existing nightly controller, retail-ci environment, immutable checkout,
trusted cmake-only execution and sanitized receipts. Public synthetic XL contracts
remain asset-free. Add bounded original-resource owners to the existing runner,
with an explicit private campaign and normal nightly selection. Never grant a PR
access to private inputs. No secret, environment or repository setting changes.

Minimum owners:

1. Host native: validate/extract pinned compressed inputs, assert all nine original
   format pairs with the native inspector (15/33 and 17/36 inspected; seven other
   pairs rejected on this base), execute the existing required Counterstrike,
   water, Observatory, Konflict and Dragon Pit CTests. Verify exact test names and
   completion rather than accept omitted optional registrations.
2. Android lane JVM: run OriginalStaticTextureTest with the pinned ten-file adapter,
   original levelpack and freshly built lifecycle engine; reject skipped tests.
3. Serial Android: reuse water, Observatory and Konflict runtime owners and the
   required original ceil002 renderer witness after ordinary retail provisioning.
   Preserve full-package Dragon Pit lifecycle/import as separately provisioned
   follow-up coverage if a safe existing automatic import path is unavailable.

The source manifest is data, never commands. Validate its identity, archive and
member hashes, bounded sizes, safe paths, unique case-folded names and HOG bounds.
Generated levels, saves and images remain private temporary outputs, never public
artifacts. Emit only fixed test IDs, counts/status and provenance in public receipts.
Missing inputs or required test execution must fail rather than silently skip.

Validation: synthetic staging and runner negatives (missing/tampered/path/duplicate
inputs, missing or skipped CTest/JUnit), existing nightly boundary/catalog tests,
local pinned original host/JVM execution, scoped formatting and final-head public
CI. Trusted private hosted execution is unavailable on an unmerged branch by design;
do not bypass that gate to obtain a green private run.

## Separate unfinished campaign acceptance

The original native owner runs 40 established, pass-required regressions and one
experimental acceptance test, `xl_level_two_keys`. The latter is an unfinished
normal-input campaign controller goal, not a prerequisite for fixture CI. Its
existing engine test and assertions remain unchanged. Improving the controller
is a separate future goal; level-two completion is not qualified by this lane.

Both categories must register and execute exactly once. Missing, duplicate,
skipped or errored results fail the owner, as does any established regression
failure or an inconsistent CTest exit code. An experimental assertion failure is
reported explicitly as `failed`, never as a pass. The owner prints a fixed-field
status receipt and retains its JSON and raw JUnit locally; raw mission output
remains private. A passing experimental run still needs separate review before
claiming campaign qualification. No bot policy or gameplay change is included.

The follow-up is based on cmake b46efc833, which includes merged PR140. Its
23/40 original witness now requires successful inspection alongside 15/33 and
17/36; the six other witnessed pairs still require explicit rejection. This
updates the historical minimum above without admitting gameplay from inspection.

Experimental failure allowance is restricted to the known incomplete red-route
assertion after earned blue-key recovery. The native wrapper must finish normally
with that exact assertion and route evidence before emitting a fixed classification.
CTest disabled/not-run cases, timeouts, crashes, missing commands, unknown inner
failures and inconsistent statuses remain blocking. Raw logs stay private.
