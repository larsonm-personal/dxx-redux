# Retire the superseded hosted Android SDK

Run 41891082083 stopped before LAN gameplay with 12,705,344 KiB free at the
16 GiB two-emulator admission gate. Initial image-tool cleanup recovered about
8 GiB, but a 12 GiB hosted SDK remained alongside the separate pinned toolchain.

1. Add explicit hosted-SDK retirement after pinned toolchain provisioning
2. Preserve runner identity checks, configured paths, caches, artifacts and budgets
3. Exercise retirement using disposable fixtures and a non-destructive sudo mock
4. Run scoped quality and relevant tests, then publish a draft PR without merging

PR #349 affects developer workspace outputs, not this hosted-runner helper.
Real recovery and gameplay must be confirmed in Actions after controller adoption.
