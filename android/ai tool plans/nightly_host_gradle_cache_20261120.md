# Trusted host metadata CLI dependency cache

On November 20 Linux shard 001 was blocked before tests by failure to resolve the Kotlin 2.4.20 plugin BOM. Five other shards got through provisioning with the same pin, so do not change pins or classify the error as a test regression. The Android cache does not cover the host-only Gradle invocation.

Use a versioned, exact GitHub Actions cache for Gradle's dependency modules, populated only by the protected controller with public inputs before any private checkout or target code execution. Require an exact match on the host JVM build declarations before using that cache for a target. Try the asset-free CLI build offline after a hit, then preserve the online fallback. Never cache test assets, Gradle home configuration, target output, or the unverified Gradle distribution. Bound warming and fallback without changing the nightly run's sample deadline. Exercise the workflow syntax and forced hit/miss/failure branches.
