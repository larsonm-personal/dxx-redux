# Large ZIP metadata resume rescan and nested error plan

## Goal
Fix two launcher metadata-viewer problems for oversized mission ZIPs that are durably extracted:

- returning to the app from minimized state should not visibly rescan the same metadata twice
- opening level metadata through a nested `.hog` or `.mn2` constituent link should use the durable extracted files and should not report `zip entry is too large`

## Current hypotheses
- The duplicate rescan is likely caused by a lifecycle refresh plus an active metadata dialog refresh both invalidating the same state on resume.
- The stale nested-entry error likely means top-level mission ZIP metadata resolves through `MissionZipExtractionStore`, while constituent metadata still builds a ZIP target from the original owner archive entry.

## Work phases
1. [ ] Trace metadata dialog state and app resume refresh paths.
2. [ ] Trace top-level versus constituent `LevelMetadataTarget` creation for extracted mission ZIPs.
3. [ ] Add a focused fix so resume coalesces metadata refreshes and constituent links resolve to extracted files when available.
4. [ ] Add or extend unit tests for the oversized extracted ZIP constituent path and any refresh policy helper.
5. [ ] Run scoped formatting and relevant unit tests.

