# Inno Reader Capabilities

This file is the maintained capability statement for `inno_reader.c` and
`inno_reader.h`. The reader is a focused implementation derived in part from
innoextract's format handling. It retains the upstream zlib-style license but
is not a complete innoextract replacement and does not depend on Boost.

## Version matrix

| Range | Status | Notes |
| --- | --- | --- |
| Inno Setup 5.3.0 through 5.6.99, Unicode | Intended parser range | Faithful transition fixtures cover the 5.3.8 MD5 and 5.3.9 SHA-1 layouts; registered real installers cover 5.5.7 and 5.6.2 Unicode metadata, listings, and extraction |
| Inno Setup before 5.3.0 or after 5.6.99 | Rejected | `inno_open` returns an error before parsing entry tables |
| Non-Unicode installers | Unverified | The parser has legacy string branches, but no registered fixture establishes support |

The version gate is broader than the verified fixture matrix. Do not describe
the complete accepted range as tested or supported until transition fixtures
cover its version-dependent layouts.

## Compression and feature matrix

| Capability | Status | Notes |
| --- | --- | --- |
| Setup header block stream | Implemented | CRC-chunked LZMA1 header decompression |
| Stored data chunks | Implemented | Buffered and streaming extraction paths |
| zlib data chunks | Implemented | Buffered and streaming extraction paths |
| LZMA1 data chunks | Implemented | Buffered and streaming extraction paths |
| LZMA2 data chunks | Implemented | Buffered and streaming extraction paths |
| BZip2 data chunks | Unsupported and rejected | No BZip2 decoder is linked |
| GOG Galaxy inner zlib stream | Implemented | Used only for entries identified by the current Galaxy heuristic |
| File integrity checksums | Implemented | MD5 before 5.3.9 and SHA-1 from 5.3.9 onward; checked before output publication |
| Executable call-instruction filter | Unsupported and rejected | `inno_extract_file` rejects `call_instruction_optimized` entries before output |
| Encrypted chunks | Unsupported and rejected | Encryption metadata is preserved for analysis, and `inno_extract_file` rejects encrypted entries before payload access or output creation |

## API semantics

- `inno_open` and `inno_open_fd` parse metadata and return the file count or
  `-1`. A successful handle must be released with `inno_close`.
- `inno_open_fd` duplicates the source descriptor. The caller retains ownership
  of the descriptor passed to it.
- `inno_extract_file` extracts one selected entry and returns `0` on success or
  `-1` on failure.
- The progress callback reports compressed-input progress. Its integer return
  value is ignored, so it is informational and cannot cancel extraction.
- Resource ceilings are shared through `extract_limits.h`; integrity and
  compatibility gaps remain tracked by their separate branch-review findings.

## Maintenance checklist

When a version layout, compression method, filter, encryption path, callback
contract, or real fixture changes:

1. Update this matrix and the public header comments in the same change.
2. Update `test_inno_capability_docs.py` if the intended capability changes.
3. Add or update a parser or extraction fixture for the changed capability.
4. Run the scoped code-quality check and `android/tests/test_cue_iso.ps1`.
