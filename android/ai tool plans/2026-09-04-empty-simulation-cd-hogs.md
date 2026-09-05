# Empty simulation records and CD HOG isolation

1. Permit empty simulation level arrays, retain metadata failure details, and test schema validation
2. Reproduce CD HOG mounting failures and isolate descriptor loading from unrelated archives
3. Run focused schema and host CD regeneration tests without replacing checked-in data

## Results

- Empty simulation records now serialize as failed with the metadata problem, and both aggregate helpers accept empty arrays
- Tests exercise the real finalization writer for object and collection roots
- CD scans no longer pre-mount every HOG on the disc; the engine resolves the selected descriptor's archive
- Malformed deepfrst.hog and hd.hog were contaminating unrelated mission scans
- Both CD collections passed twice: 123 missions and 130 levels recovered in temporary outputs
- Native failed CD responses now fail the source stage instead of being silently counted as passed
- Schema, runner discovery/sampling, archive-source tests, and scoped quality checks passed
- Checked-in metadata and simulation JSON were not overwritten
