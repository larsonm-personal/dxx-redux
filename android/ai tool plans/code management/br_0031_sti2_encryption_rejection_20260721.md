# BR-0031 STi2 Encryption Rejection

## Goal

Preserve STi2 encryption metadata and reject encrypted matching entries with a
documented unsupported result before allocating data or creating output

## Plan

- [x] Add public data- and resource-fork encryption metadata and an unsupported
  encryption result code
- [x] Preserve encryption flags while listing and reject encrypted data forks
  before allocation or filesystem mutation
- [x] Add valid-header fixtures covering stored and compressed encrypted data
  with compressed sizes below and above the legacy key size
- [x] Run scoped code quality, build the extraction targets, and run the native
  extraction suite
- [x] Finalize BR-0031 and move it to the done ledger with validation evidence
