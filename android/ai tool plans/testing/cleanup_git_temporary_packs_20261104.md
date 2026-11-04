# Verified duplicate Git temporary packs

The November 4 space inventory finds 4.6 GiB of abandoned Git pack work files,
including October 27 `.tmp-<pid>-pack-*` and `tmp_pack_*` files. Preserve all
refs, stashes, worktree changes, original assets, APKs and profile evidence.

1. Compare temporary files with normal pack files using size and SHA-256
2. Add opt-in GitTemporaryPacksOnly cleanup, restricted to this checkout's
   ordinary .git/objects/pack directory, with a seven-day minimum age by default
3. Require an identical canonical file, no active Git/build/test processes,
   safe contained paths and fresh metadata/content checks before deletion
4. Extend synthetic cleanup tests for duplicates, unmatched/recent files,
   links, preview, exclusions, active Git and changed canonical bytes
5. Run scoped quality and cleanup tests, then preview/apply the verified
   duplicates toward 50 GiB free; record hashes and preservation checks

Do not prune objects, repack, remove refs, or remove unmatched temporary packs.
No APK builds or physical-device operations. Let the current protected Pumo
test finish and verify restoration before any real deletion.
