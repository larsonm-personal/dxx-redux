# Production import coverage

## Objective

Keep the existing extraction architecture lightweight while sharing decisions
that can drift and testing the original sources through the launcher paths.
Do not rewrite working codecs or build a second test-only importer.

## Work and acceptance criteria

- [ ] Move split SOW assembly into a shared native batch API using ARJ continuation
  flags and offsets, used by the desktop CD tool, Android CD import, and nested
  demo extraction; eliminate per-package append decisions
- [ ] Cover complete, reordered, missing, conflicting and damaged volumes with
  native tests, including real demo/CD output hashes
- [ ] Extract only the launcher operations necessary to call real archive import
  and publication from integration tests, with complete known-package validation
- [ ] Add a reusable original-demo corpus test invoking production launcher
  extraction, checking installed output paths, sizes, hashes and readiness
- [ ] Strengthen CD regression assertions to validate actual expected contents,
  distinguish extraction from launch coverage, and exercise the split-volume CDs
- [ ] Run the host corpus, Android integration, appropriate builds, scoped quality,
  and automation catalog checks; audit the final coverage against this plan

## Constraints

Preserve unrelated concurrent worktree changes and user device data. Run Android
tests serially on a test emulator. Existing native format code remains shared.
Independent expected hashes remain reviewable data, not regenerated from a test
run's output. Required corpus fixtures must fail clearly when absent.

## Evidence

The raw D1 and D2 SOW entries have ARJ flag 0x04 for a file that continues and
0x08 plus a 32-bit offset at header byte 30 for its continuation. This matches
the ARJ reference format (`https://github.com/joncampbell123/arj`, `defines.h`
and header readers). The previous blanket append option ignored these fields.
Host and launcher CD loops both used overwrite mode, while the existing native
real-media test supplied append mode explicitly. General CD regressions checked
filenames but did not compare expected output hashes.
