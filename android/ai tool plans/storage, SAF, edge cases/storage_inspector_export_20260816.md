# Storage inspector export

## Goal

Add an exportable diagnostic snapshot of the storage inspector file list and SAF link list so accumulated or orphaned files on a long-lived device can be analyzed.

## Plan

- [x] Trace storage inspector enumeration, SAF link persistence, and existing diagnostic export patterns
- [x] Define a bounded, readable export format with useful file and link provenance
- [x] Implement the export UI and serialization while preserving current storage behavior
- [x] Add or extend automated tests for the diagnostic output
- [x] Run scoped formatting, focused Android unit tests, Android debug assembly, and D1/D2 Windows builds

## Device artifact analysis

- [x] Validate the exported artifacts without loading the large JSON directly into conversation context
- [x] Summarize storage files by location, purpose, directory, extension, age, and mission ZIP ownership
- [x] Correlate suspicious file groups with SAF links and route metadata activity
- [x] Identify retained route checkpoints as the file-count cause and document the cleanup boundary

## Default set provenance investigation

- [x] Trace file-set creation, import assignment, and selector display behavior
- [x] Determine why Vertigo files are present under `sets/default` without being visible in the selector
- [x] Distinguish retained default-set content from an orphaned named set and identify the UI/provenance boundary

## Implementation

- [x] Add a tested mission/expansion inventory model for loose active-set HOG and descriptor files
- [x] Expose the inventory, detected product/version, source set, file details, and safe removal in the file-set UI
- [x] Label route-cache records and visibility checkpoints accurately in Storage Inspector
- [x] Delete completed visibility checkpoints after final cache publication and prune obsolete generations safely
- [x] Add a user-facing route-metadata cache clear action coordinated with the background worker
- [x] Add focused Kotlin/native tests and expose inventory through launcher introspection for integration checks
- [x] Run scoped formatting, focused tests, Android assembly, and D1/D2 host builds
