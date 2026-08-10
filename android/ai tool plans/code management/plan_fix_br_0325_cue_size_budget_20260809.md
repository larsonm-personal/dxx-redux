# Fix BR-0325 CUE size budget

## Plan

- [x] Confirm the existing import limit and trace every CUE staging and native materialization path
- [x] Define one shared CUE byte limit for Kotlin and native consumers
- [x] Reject over-limit and conversion-unsafe CUE files before copy, allocation, or publication
- [x] Add focused boundary and short-read regression coverage
- [x] Run scoped quality, focused tests, Windows builds, and Android integration validation
- [x] Move BR-0325 to the done ledger with resolution evidence
- [x] Mark this plan complete
