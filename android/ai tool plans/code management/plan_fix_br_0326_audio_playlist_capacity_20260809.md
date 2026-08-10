# Fix BR-0326 audio playlist capacity

## Plan

- [x] Re-read project instructions and trace Kotlin production, native admission, fallback, and track-list consumers
- [x] Define and enforce matching source and aggregate-track semantics at the producer and consumer boundaries
- [x] Make native playlist admission atomic, including complete-document and one-track handling
- [x] Add focused boundary and end-to-end playlist regression coverage
- [x] Run scoped quality, focused tests, Windows builds, and Android integration validation
- [x] Move BR-0326 to the done ledger with resolution evidence
- [x] Mark this plan complete
