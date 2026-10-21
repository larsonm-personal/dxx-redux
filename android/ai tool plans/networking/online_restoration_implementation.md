# Online restoration implementation

Base: cmake 092f4c7d247eaeb26160933540f3bdbf2f318d4a

Keep the online release browser disabled until qualification completes. Use an
isolated local server and synthetic identities for development. The detailed
security working report is private and is not part of this branch.

## Work sequence

1. Establish real loopback control/data-plane tests and negative controls
2. Make connection replacement and room departure atomic and idempotent; enforce
   room authorization, explicit production authentication and trusted proxy identity
3. Bind relay admission and cleanup to sender, pair and game lifetime; verify
   legitimate native producer traffic and three-player global slot numbers
4. Qualify Android cancellation/reconnect and register bounded recovery coverage
5. Prepare isolated deployment/readiness/rollback rehearsal with synthetic state
6. Publish a reviewable opt-in rollout plan; actual staging/live work needs the
   owned destination and applicable approval, and is not implied by local tests

## Policies

- Duplicate login replaces the previous connection, fencing all old work
- Host departure closes its room consistently; online host migration is deferred
- Dev identity is explicitly local/test-only, never a production fallback
- Keep protocol changes explicit and coordinate client/server rollout
- No production probes, credentials, infrastructure changes or merge

## Evidence

Record exact commands, results and remaining gaps in each implementation receipt.
Use existing CI evidence from its owner instead of duplicating active runs.

### Connection and room lifecycle slice

Local Rust 1.94.1 build uses the committed Cargo.lock. Three new socket-level
negative controls failed before implementation; the complete integration target
then passed 69 tests after implementation, including replacement close ordering,
room isolation, legitimate friendship, host departure, repeated authentication and
close-before-welcome cleanup. Commands use `cargo test --locked --manifest-path
server/Cargo.toml --target-dir android/temp/online-server-target --test integration`.
This is local server coverage, not Android or internet deployment qualification.

Short control-plane transitions use one mutex, never across network awaits, so
replacement, authorization checks and mutation cannot interleave. Host departure
closes the room; a rejected join preserves the old room. Session retirement also
releases its STUN admission reference. Slow outbound clients are retired instead
of silently dropping state transitions. Existing native/game behavior is unchanged.
