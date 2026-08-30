# Mission import copy integrity

## Goal

Preserve the existing final mission SHA-256 verification while avoiding a redundant hash after a successful move and verifying any fallback copy before registration.

## Plan

- [x] Track whether direct-source import moved or copied the verified archive
- [x] Recompute and compare the destination identity after a fallback copy
- [x] Confirm stored hash, size, chunk hashes, and mtime remain part of normal mission registration
- [x] Add a focused regression test and run scoped quality, tests, and Android build

## Constraints

- Do not add another full hash to the normal successful rename path
- Never register a copied archive unless its full identity matches the accepted source identity
- Preserve existing import cleanup and metadata behavior

## Validation

- Scoped code quality passed for the implementation, test, and plan
- Copy-integrity and managed-archive mtime tests passed
- Android `:app:assembleDebug` passed for all configured ABIs

Status: complete
