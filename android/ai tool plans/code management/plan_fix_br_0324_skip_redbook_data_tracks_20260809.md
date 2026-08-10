# Fix BR-0324 combined Redbook data-track playback

## Plan

- [x] Re-read project instructions and trace combined playlist numbering, automatic D1/D2 selection, range playback, next/previous controls, names, disc identity, and maintained tests
- [x] Centralize audio-track iteration while preserving physical combined track numbers
- [x] Reject audio-empty ranges and skip data records during range advancement
- [x] Cover leading and interleaved data tracks in the maintained SAF Redbook integration test
- [x] Run scoped quality, focused tests, Android builds and integration, Windows builds, and diff validation
- [x] Move BR-0324 to the done ledger with resolution evidence
- [x] Mark this plan complete
