# Auxiliary native build cleanup, November 4 2026

Inventory found 3.02 GiB in android/tools/etc2tool build trees and about 1.48 GiB
under android/tests, outside existing discovery. Extend independent cleanup PR349,
preserving gameplay WIP, original assets, APK bundles, profiles and AVDs.

1. Discover known auxiliary build locations only when CMakeCache identifies an
   existing in-checkout CMakeLists source; use existing generation and safety policy
2. Exercise preview, default family retention, zero retention, exclusions and
   unknown/external-source rejection in the synthetic integration test
3. Run scoped quality and cleanup tests; preview actual candidates with exclusions
4. Apply only verified generated outputs toward 50 GiB actual free space and record
   preservation, before/after space and remaining limits
