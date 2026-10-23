# Rust refresh checkpoint

Branch: rust-toolchain-refresh; draft PR: https://github.com/larsonm-personal/dxx-redux/pull/115
Base: cmake 4be58740e29440a2c2b944b31bcfd2d72c0d325b

## Scope

Refresh compiler, components, addons and exact server dependencies independently
of online-restoration/PR113. No deployment, merge or cmake push

## Selections and workflow

- Rust/Cargo 1.99.1, Clippy 0.1.99, rustfmt 1.10.0 from the same compiler release
- rustup 1.29.2, cargo-udeps 0.1.61, cargo-edit 0.13.14, bindgen 0.73.2
- udeps uses nightly-2026-10-22
- Versions verified against official Rust distribution manifests and crates.io
- Compiler/components: root rust-toolchain.toml; addons: server/rust_tools.conf
- rust_upgrade.sh installs exact selections and records actual version output
- rust_dependency_lint.sh uses cargo upgrade --pinned; --dry-run previews without
  changing the lockfile, while --incompatible explicitly permits major updates
- All 25 direct/dev dependency versions remain exact; 19 updated
- Package 0.1.0 and edition 2021 remain unchanged
- Normal installation uses declared versions; deploy build uses --locked
- Rust bootstrap uses platform.sh's portable create_temp_dir helper

## CI

The server check always reports on PRs and pushes to cmake. A short inline Git
comparison runs Rust only for server/ changes: PR aggregate merge-base-to-head,
push before-to-after, new branch versus empty tree, deleted ref skipped
Deletions and moves into/out of server/ count. Unrelated Android, native, root
compiler declaration and workflow-only changes do not run Rust. No branch
protection changes. Bespoke Python guards/filter/tests were removed on request

## Validation and known limits

Locked build and 112 Rust tests, Clippy, formatting, dated udeps and bindgen smoke
checks passed. The portability regression passed after adopting create_temp_dir
The full hosted CI suite passed at 6ba7a6bd9efdf7117aa4932104ef4789ae2a1c9a
Current-head CI and simplification validation are recorded in the PR body

RustSec database 4b587f00996ebe5c05a73eaef105ef241e876414: old lockfile 20
vulnerability matches, refreshed lockfile zero. Retained informational warning
RUSTSEC-2025-0134: rustls-pemfile 2.2.0 is unmaintained; a separate change can
migrate PEM parsing to rustls-pki-types. No advisory ignores or TLS changes
Local SDK/master catalog execution is limited by missing executor /proc child
supervision support; tests were not weakened. No live deployment was tested
