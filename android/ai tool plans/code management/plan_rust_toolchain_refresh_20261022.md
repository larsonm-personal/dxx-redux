# Rust toolchain and dependency refresh

Base: cmake 4be58740e29440a2c2b944b31bcfd2d72c0d325b
Branch: rust-toolchain-refresh

## Scope and plan

- Audit existing Rust upgrade, dependency, installation, formatting and CI helpers
- Query official Rust distribution manifests and crates.io metadata for stable selections
- Pin compiler, compatible components, rustup, addon tools and dated udeps nightly
- Keep all direct/dev Cargo dependencies exact; retain package 0.1.0 and edition 2021
- Make regular installs reproducible, preserve explicit upgrade workflow, remove floating nightly
- Build/test with --locked, run Clippy, pinned formatting, addon checks and script tests
- Check RustSec advisories and source/API compatibility; document base defects separately
- Publish this independent branch and a draft PR targeting cmake; monitor applicable CI

No online-restoration/PR113 history, deployment, service/configuration changes, or cmake push

## Discovery evidence

Official https://static.rust-lang.org/dist/channel-rust-stable.toml queried 2026-10-22:
Rust 1.99.1 (2026-10-15 release), Clippy 0.1.99, rustfmt 1.10.0
Official rustup release-stable.toml: 1.29.2
crates.io API: cargo-udeps 0.1.61, cargo-edit 0.13.14 (MSRV 1.97), bindgen-cli 0.73.2
Existing formatter pin: Rust 1.96.1; align with selected stable compiler

The base has 25 versioned direct/dev dependencies (24 normal, one dev), all exact
PR113's direct ring dependency is absent; do not silently import it or its fixes
No .agents/skills directory exists in this checkout or /workspace/.agents

## Exact dependency changes

| Crate             | Before         | After      |
| ----------------- | -------------- | ---------- |
| tokio             | `=1.51.1`      | `=1.54.1`  |
| tokio-tungstenite | `=0.29.0`      | `=0.30.0`  |
| rustls            | `=0.23.37`     | `=0.23.46` |
| tokio-rustls      | `=0.26.4`      | `=0.26.6`  |
| serde             | `=1.0.228`     | `=1.0.229` |
| serde_json        | `=1.0.149`     | `=1.0.154` |
| reqwest           | `=0.13.2`      | `=0.13.5`  |
| dashmap           | `=6.1.0`       | `=6.2.1`   |
| uuid              | `=1.23.0`      | `=1.28.0`  |
| tracing-appender  | `=0.2.4`       | `=0.2.5`   |
| futures-util      | `=0.3.32`      | `=0.3.34`  |
| axum              | `=0.8.8`       | `=0.8.9`   |
| rusqlite          | `=0.39.0`      | `=0.40.2`  |
| chrono            | `=0.4.44`      | `=0.4.45`  |
| ed25519-dalek     | `=3.0.0-pre.6` | `=3.0.0`   |
| sha2              | `=0.11.0`      | `=0.11.1`  |
| rand              | `=0.10.0`      | `=0.10.3`  |
| hyper             | `=1.9.0`       | `=1.12.0`  |
| hyper-util        | `=0.1.20`      | `=0.1.21`  |

Six direct pins remain unchanged; package version and edition are unchanged

## Upgrade and regular-install flow

1. Discover releases from official Rust distribution manifests and crates.io metadata
2. Edit `server/rust-toolchain.toml`, `server/rust_tools.conf` and the matching `RUSTFMT_TOOLCHAIN` pin
3. Run `bash server/rust_upgrade.sh` to install exact selections and record actual inventory
4. Preview dependencies with `bash server/rust_dependency_lint.sh --dry-run --incompatible`
5. Review release notes, then run the helper without `--dry-run`; inspect manifest and lockfile
6. Run locked build/tests, Clippy, formatting and advisory checks before committing

The dependency helper preserves exact requirements with cargo-upgrade `--pinned`,
checks all dependency tables before/after, resolves the lockfile, and runs locked
udeps with a dated nightly. It no longer deletes the registry or requests the
non-sparse git index. A dry run exits before lock/build operations

Regular get_rust/deploy installs consume declared pins, including a versioned
rustup installer checked against the official SHA256 file. They never discover
stable/nightly releases. Deploy builds use --locked; deployment was not executed

## Compatibility review

- All 272 resolved packages with declared MSRVs support the selected compiler
- ed25519-dalek moves from 3.0.0-pre.6 to stable 3.0.0; its release notes raise MSRV
  to 1.85 and update signature/rand_core dependencies. Existing key generation,
  signature roundtrip/rejection tests pass without changing verification code
- tokio-tungstenite/tungstenite 0.30.0 raises MSRV to 1.85 and rejects invalid
  Sec-WebSocket-Key values. Existing websocket integration tests pass
- rusqlite 0.40 changes virtual-table interfaces not used by this server and
  updates bundled SQLite; database/integration tests pass
- No source/API edits, crypto-provider selection changes, LAN-default changes,
  or imported PR113/online-restoration history

Sources: official distribution manifests, https://crates.io/api/v1/crates/{crate},
https://github.com/rusqlite/rusqlite/releases/tag/v0.40.0, and published
tungstenite 0.30.0 / ed25519-dalek 3.0.0 crate CHANGELOG.md files

## Validation checkpoint

- Base and upgraded locked builds pass on Rust 1.99.1
- Base and upgraded locked tests: 37 unit + 62 integration + 13 NAT = 112 passed
- cargo clippy --locked --all-targets: clean, no warnings
- cargo fmt --check using 1.99.1: pass, no Rust formatting changes
- Six isolated helper tests, exact-pin guard, Bash syntax, ShellCheck 0.11.0,
  shfmt 3.14.1 and actionlint 1.7.12: pass
- Scoped project mixed-language quality invocation: all checks passed
- Automation catalog validation: 95 standalone JSON tests, 430 support scripts,
  262 standalone PowerShell tests, pass
- Master catalog execution blocked by executor: Linux child supervisor cannot
  read /proc/self/task/<pid>/children (exit 125). Tests remain unchanged
- No C/C++ changes; native gameplay builds and live deployment are outside this validation
- Addon and advisory results below; final hosted CI status is maintained in draft PR #115

## Completed addon and advisory verification

- `bash server/rust_upgrade.sh`: pass; rust_version.txt is actual installed output
- `bash server/rust_dependency_lint.sh --incompatible`: pass; no further manifest/lock
  changes, dated nightly all-targets udeps reports all dependencies used
- bindgen 0.73.2 + host libclang 19 generated a C-struct binding whose compile-time
  size/alignment/offset assertions compile with Rust 1.99.1
- Cargo.lock SHA256: `b8492d61fe7f27abd64a2f67e6f9c58f80f50c5639bd60f3b929d851e5d2f0ac`
- cargo-audit 0.22.2 against RustSec database commit
  `4b587f00996ebe5c05a73eaef105ef241e876414` (1348 advisories):
  old lockfile 20 vulnerability findings; refreshed lockfile zero (audit exit 0)
- Old lockfile also had three unsound warnings and one yanked-package warning;
  those no longer appear in the refreshed lockfile
- Remaining informational warning: [RUSTSEC-2025-0134](https://rustsec.org/advisories/RUSTSEC-2025-0134.html),
  rustls-pemfile 2.2.0 is unmaintained. It is already a thin wrapper around
  rustls-pki-types PEM parsing. Retained exact existing version to avoid changing
  TLS parsing in this bounded refresh; follow-up can use `PemObject` directly
- These are lockfile advisory matches, not a claim that every old finding was
  exploitable through this server's enabled features or call paths

### Old lockfile vulnerability matches eliminated

- [RUSTSEC-2026-0044](https://rustsec.org/advisories/RUSTSEC-2026-0044.html): aws-lc-sys 0.38.0
- [RUSTSEC-2026-0048](https://rustsec.org/advisories/RUSTSEC-2026-0048.html): aws-lc-sys 0.38.0
- [RUSTSEC-2026-0258](https://rustsec.org/advisories/RUSTSEC-2026-0258.html): h2 0.4.13
- [RUSTSEC-2026-0338](https://rustsec.org/advisories/RUSTSEC-2026-0338.html): quinn-proto 0.11.14
- [RUSTSEC-2026-0342](https://rustsec.org/advisories/RUSTSEC-2026-0342.html): quinn-proto 0.11.14
- [RUSTSEC-2026-0340](https://rustsec.org/advisories/RUSTSEC-2026-0340.html): quinn-proto 0.11.14
- [RUSTSEC-2026-0339](https://rustsec.org/advisories/RUSTSEC-2026-0339.html): quinn-proto 0.11.14
- [RUSTSEC-2026-0185](https://rustsec.org/advisories/RUSTSEC-2026-0185.html): quinn-proto 0.11.14
- [RUSTSEC-2026-0348](https://rustsec.org/advisories/RUSTSEC-2026-0348.html): rustls 0.23.37
- [RUSTSEC-2026-0346](https://rustsec.org/advisories/RUSTSEC-2026-0346.html): rustls 0.23.37
- [RUSTSEC-2026-0349](https://rustsec.org/advisories/RUSTSEC-2026-0349.html): rustls 0.23.37
- [RUSTSEC-2026-0285](https://rustsec.org/advisories/RUSTSEC-2026-0285.html): rustls 0.23.37
- [RUSTSEC-2026-0104](https://rustsec.org/advisories/RUSTSEC-2026-0104.html): rustls-webpki 0.103.9
- [RUSTSEC-2026-0382](https://rustsec.org/advisories/RUSTSEC-2026-0382.html): rustls-webpki 0.103.9
- [RUSTSEC-2026-0099](https://rustsec.org/advisories/RUSTSEC-2026-0099.html): rustls-webpki 0.103.9
- [RUSTSEC-2026-0098](https://rustsec.org/advisories/RUSTSEC-2026-0098.html): rustls-webpki 0.103.9
- [RUSTSEC-2026-0049](https://rustsec.org/advisories/RUSTSEC-2026-0049.html): rustls-webpki 0.103.9
- [RUSTSEC-2026-0009](https://rustsec.org/advisories/RUSTSEC-2026-0009.html): time 0.3.44
- [RUSTSEC-2026-0383](https://rustsec.org/advisories/RUSTSEC-2026-0383.html): webpki-root-certs 1.0.6
- [RUSTSEC-2026-0381](https://rustsec.org/advisories/RUSTSEC-2026-0381.html): webpki-roots 1.0.6

## Publication checkpoint

Draft PR: https://github.com/larsonm-personal/dxx-redux/pull/115
Target: cmake; branch: rust-toolchain-refresh
Identity: larsonm-personal for both Git commits and GitHub operations
Hosted server, tooling, engine, public save-integrity and LAN smoke checks run
on the branch; current/final results are recorded in the PR body
