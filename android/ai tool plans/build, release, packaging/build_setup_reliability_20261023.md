# Build setup reliability

Base: cmake 27272be48cb0d309a272f44458518797499dd99b

1. Preserve verified cache locking, exact pins, TLS and atomic publication
2. Bound transient transport retries and apt acquisition stalls
3. Preflight SDK archives and document prerequisites and recovery
4. Exercise partial transfers, permanent failures, checksum rejection and cache reuse
5. Run scoped quality, affected suites and final-head CI; publish a draft PR

Platform startup failures with zero jobs are outside repository execution

Persist the verified recovery entry points on the same PR:

- Add a check-first tool wrapper with explicit bounded install and child-only environment
- Reuse the native runner with an external test build root; add a narrow Android SDL12 smoke helper
- Keep the image-specific Debian compiler/runtime workaround in documentation only
- Validate failure propagation with existing fixtures and real native/Android builds
