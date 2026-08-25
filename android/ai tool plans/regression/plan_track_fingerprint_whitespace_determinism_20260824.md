# Track fingerprint whitespace determinism

1. [x] Inventory the changed regression JSON files and characterize the exact byte-level differences
2. [x] Trace the files to every writer, formatter, and relevant line-ending configuration
3. [x] Reproduce generation or serialization in a temporary location and test repeated output for stability
4. [x] Record the cause, determinism risk, and any narrowly scoped correction or recommendation

## Outcome

- All ten changed manifests were semantically identical to `HEAD`; only indentation and spacing after colons differed
- Windows PowerShell 5.1 and PowerShell 7 serialize the same objects with different `ConvertTo-Json` whitespace
- The August 23 PowerShell 5.1 compatibility change exposed the host-dependent writer by running the workflow under the current PowerShell host
- `fingerprint_disc_tracks.ps1` now normalizes JSON as part of atomic publication and preserves the corpus's existing no-final-newline convention
- The shared normalizer now accepts redirected-stdin BOM forms produced by Windows PowerShell 5.1
- The publication test asserts exact canonical bytes and passes under PowerShell 5.1.26100.9168 and PowerShell 7.6.5
