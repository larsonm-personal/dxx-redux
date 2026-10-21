# Corpus D1 route staging

- Reproduce the Training route's pre-execution failure with split verified retail inputs
- Use the existing hash-checked automatic combined D1/D2 staging instead of passing a D1-only directory to the D2 route runner
- Retain bounded typed early route failure diagnostics without publishing private stderr
- Verify adapter wiring, split-source staging and wrong-edition rejection; preserve route oracles and variant ambiguity blocks
- Publish an isolated draft fix and follow exact-head checks; do not merge or change trusted workflow gates
