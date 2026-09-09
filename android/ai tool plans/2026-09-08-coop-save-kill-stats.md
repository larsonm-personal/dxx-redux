Persist co-op robot kill statistics

- Save the per-slot robot kills, earned robot score, and level score total in the shared co-op metadata
- Restore statistics using the same player identity mapping as inventory, preserving unmatched contributions in unused slots
- Bump the disposable Android metadata version, leaving upstream save layouts unchanged
- Run scoped code quality, both game builds, and relevant save-format tests

Completed implementation and scoped formatting
Validation: D1/D2 Windows builds and D1/D2 Android x86_64 Debug libraries pass
Co-op save-format, player-session, and recovery tests pass in the D2 host test build
D1 host build has no registered tests
Live two-player save/load verification remains unperformed
Metadata is now version 9, per the repository's disposable Android format policy
