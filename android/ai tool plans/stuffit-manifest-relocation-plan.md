## StuffIt Manifest Relocation Plan

### Goal
- move committed StuffIt regression manifests out of the extract scratch tree that cue/iso tests delete
- keep the manifests near the extract desktop tests in an already committed fixture area
- update manifest generation and consumption paths together so regeneration and CTest stay in sync

### Steps
- [x] confirm the destination under extract test data and collect the current manifest inputs
- [x] move the committed manifest JSON files to the new fixture directory
- [x] update generator and consumer paths in PowerShell, CMake, and desktop test code
- [x] run the relevant desktop StuffIt regression test
- [x] mark the plan complete with the validated path

### Result
- committed StuffIt manifests now live under `android/app/src/main/cpp/extract/test/data/stuffit_manifests/`
- `android/generate-stuffit-corpus-manifests.ps1` regenerates that committed fixture directory by default
- `test_stuffit_corpus.cpp` and the CMake compile definition now point at the same relocated path
- `ctest -C Release -R stuffit_corpus_tests --output-on-failure` passed after the move