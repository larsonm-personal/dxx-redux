# BR-0025 SOW header documentation

## Goal

Replace the obsolete libarchive description with an accurate statement of the
in-tree SOW and ARJ extraction implementation

## Plan

- [x] Create the remediation plan
- [x] Read repository instructions, the complete finding, and the live SOW interface
- [x] Trace related documentation and callers for conflicting implementation claims
- [x] Correct the smallest authoritative documentation surface
- [x] Run scoped code quality and relevant consistency or native checks
- [x] Finalize the finding disposition and validation record

## Validation

- Scoped code quality passed for `sow_extract.h`
- CMake rebuilt `test_sow_huffman` and `test_sow_integrity`
- Focused CTest execution passed all 3 SOW suites
- Repository and CMake searches found no live obsolete dependency claim or link
  dependency
