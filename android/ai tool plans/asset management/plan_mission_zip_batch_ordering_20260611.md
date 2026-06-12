# Mission zip batch ordering

## Goal
- Reorder mission zip batch processing so zips without existing JSON output run first, then zips with JSON output run oldest output first

## Plan
- [x] Find the mission zip batch script and its current zip enumeration
- [x] Add ordering based on expected JSON output presence and modification time
- [x] Run a scoped validation of the script behavior or syntax
- [x] Mark this plan complete after verification
