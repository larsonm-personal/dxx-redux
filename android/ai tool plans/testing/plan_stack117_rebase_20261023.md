# Stack 117 rebase onto merged build and Rust fixes

- Preserve remote PR113 head 7db3ff6ef514d61a06e22e14297fc714d4b574d7 and PR116 head 9d1f11e78b41a8087ca0a818a06b206547b158b4 as rollback points
- Rebase locally onto cmake 1cadb4d3e5a39bd7fa08926df2e53725b1851b64, including merged PR118 and PR115
- Preserve three-player LAN scheduling, online opt-in defaults, strict receipt checks, incoming build timeouts, exact Rust pins and the current installer entry point
- Retain the refreshed lock graph and resolve the online JWT dependency without upgrading existing package versions
- Adapt the deprecated atomic fetch_update name to try_update for Rust 1.99.1 without changing admission ordering or limits
- Verify PR116's four commits remain patch-equivalent after rebasing from their actual fork point 5e888904f042d9e9612a8378df64cfbab21ece36
- Run Rust tests, strict Clippy, planner and catalog controls, installer fixtures and host JVM transport/service checks
- Publish only the two approved stack refs with explicit old-head lease guards after fresh remote checks; never push cmake or merge a PR
