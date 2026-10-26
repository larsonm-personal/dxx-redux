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
