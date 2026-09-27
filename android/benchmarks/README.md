# Level metadata benchmark

Run from the repository root with PowerShell 7 on Windows or Linux:

```powershell
./android/tests/test_level_metadata_benchmark.ps1 -RequireAssets -NoHistoryUpdate
```

The runner uses the shared host build entry point, hash-verified retail data
discovery, and supervised processes with per-level timeouts. Use `-SkipBuild`
with existing native build trees. The full suite requires both pinned mission
archives in `level_metadata_analysis_manifest.jsonc`; absent assets produce
exit 2 (SKIP), or failure with `-RequireAssets`.

To select exact manifest level IDs:

```powershell
./android/tests/test_level_metadata_benchmark.ps1 -SkipBuild -RequireAssets `
    -LevelId d1-first-strike-01,d2-counterstrike-02 -NoHistoryUpdate
```

Selected runs check the same expected metadata hashes and repeat determinism as
the full suite. They always leave full-suite history unchanged and reject
`-AcceptBaseline`. Their current summary is
`android/temp/level_metadata_analysis_filtered_current.json`, with selected
IDs and `complete_suite: false`; the full suite uses its separate current summary.

Metadata digests use UTF-8 with canonical CRLF line endings to preserve the
existing Windows baseline convention on both hosts. JSON fields, numbers, and
spacing are not normalized. Generated files themselves are left unchanged.

`-SkipDigestValidation` is for diagnostic measurements. It still checks repeated
output hashes against each other, but does not prove compatibility with the
expected manifest hashes. Summaries record whether expected digest validation
was enabled. Use `-NoHistoryUpdate` for diagnostic runs.

Per-run JSON and logs live under `android/temp/level_metadata_benchmark/run_*`.
Retention keeps three prior generations, producer locks protect active runs,
and extracted mission archives are removed on success or failure.
