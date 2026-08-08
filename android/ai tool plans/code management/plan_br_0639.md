# BR-0639 implementation plan

1. [completed] Inspect the public XFing converter, its PNG emission paths, and focused tests
2. [completed] Replace the host graphics API with a cross-platform PNG encoder that preserves RGBA pixel semantics
3. [completed] Add focused decoder-backed coverage and exercise both D1 and D2 conversion paths on Windows and Linux
4. [completed] Run scoped quality checks and archive BR-0639 in the completed ledger
