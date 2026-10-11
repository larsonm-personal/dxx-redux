# Chunk0339 Kotlin JSONC lexical boundary continuation, 2026-10-10

Diagnosis and plan only. EXTENDS existing GQF-0237/GQR-0222 OPEN/TODO; canonical extension publication waits for final chunk0339. No new root, rerating, product/fixture edit or executed reproduction. Preserve completed PowerShell repair and existing Rust continuation.

## Current source evidence

Jsonc.kt block-comment loop copies original newlines but emits no separating character for a block without newlines. Source-derived examples: 1/* gap */2 becomes12, 1/* gap */e2 becomes1e2, and 1/* gap */.2 becomes1.2 before downstream JSON parsing. This matches the existing GQR-0222 comment-token joining mechanism. It does not undo the completed quoted-string/escape/comment-marker/trailing-comma repair or imply every maintained asset is affected.

Actual Jsonc consumers include known-version, physical-disc, fingerprint/album/configuration and robot-name asset readers. KnownVersions closes its reader in the current delta; preserve that GQR-0206 repair. JsoncTest covers trailing commas, nested closers, line shape and quoted escape/comment literals, not adjacent scalar-token boundaries. ConfigImportLoader parses its bounded raw text directly through JSONObject rather than Jsonc; keep its strict grammar/admission requirements under GQR-0210 distinct.

LauncherScriptExecutor also has a separate stripLauncherJsonc implementation, used by actual load and referenced in LauncherFileCopyTest. It handles quoted strings, line comments and trailing commas but not block comments. This is a possible simplification boundary, not a separately admitted defect or automatic instruction to expand the accepted grammar. Evaluate shared use only after preserving intentional launcher-script grammar, errors and string/line semantics.

## Implementation boundary and future acceptance

- Preserve lexical separation at each recognized comment boundary while retaining CR/LF, quoted URLs/comment literals, escapes and structural trailing-comma behavior
- Use one shared Kotlin comment/trailing-comma authority where actual caller grammars agree; remove duplicate lexical loops only when acceptance shows no accidental grammar expansion or changed diagnostics
- Test split integer/fraction/exponent and structural-token boundaries through actual Jsonc and actual consumer parsing, plus ordinary commented assets and exact string/escape/newline controls
- Require invalid split numeric source to remain invalid; whitespace insertion must not be used to claim Android org.json enforces the full strict JSON subset. Coordinate the existing GQR-0210 strict grammar boundary separately
- Preserve bounded config import, disposable schemas and all known-version/disc/fingerprint/robot identity policies; no compatibility readers or native/inherited-engine changes

No parser, malformed/security/resource/fault probe, build, device or fixture execution in this tranche. Final actual-reader ownership, inherited minimization and canonical chunk audit remain pending.
