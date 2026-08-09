# File encoding repair plan

1. [x] Scan changed files for BOMs, invalid UTF-8, and mixed or unintended line endings
2. [x] Identify the encoding regression without rewriting unrelated files
3. [x] Normalize the affected files to the repository format
4. [x] Run encoding lint and diff verification
