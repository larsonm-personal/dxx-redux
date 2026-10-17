# Bad test inputs

- `pcx-truncated-header.pcx`: 127 zero bytes, one byte short of a PCX header
- `pcx-obfuscated-overlong-rle.xxx`: D1 obfuscated PCX whose decoded RLE run exceeds its row
- `png-truncated-signature.png`: PNG signature with no chunks or image data
- `mission-missing-equals.msn`: recognized directive without the required `=` separator
- `mission-overlong-briefing.msn`: nine-character stem cannot fit a DOS name plus `.tex`
- `mission-negative-level-count.msn`: negative declared level count must not reach allocation
- `mission-truncated-level-list.msn`: declares two level records but provides only one

Keep additions minimal and document the parser invariant each sample violates.
