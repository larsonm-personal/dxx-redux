# Metadata Tool Idea Review Plan

## Goal
- [x] Review proposed launcher metadata ideas against current Kotlin metadata support and local engine/source format readers, then identify sensible next additions.

## Work Items
- [x] Compare proposed HOG, PIG, mission descriptor, HAM, HXM, POG, sound bank, level, and BIN/CUE metadata against current launcher support.
- [x] Mark which ideas are already implemented, low-risk additions, native-helper candidates, or not worth doing now.
- [x] Capture a recommended implementation order.

## Review Findings
- Already strong: HOG entry listing, mission descriptor title/type/author/editor/levels, DXA listing, and safe PIG header summaries.
- Good low-risk additions: mission descriptor secret levels and extra keys, HOG total embedded bytes and known-size edition hints, PIG animated bitmap groups, POG header/count/override summaries, and BIN/CUE track summaries using existing native CUE parsing.
- Native-helper candidates: HAM table counts, HXM extra robot/weapon counts, S11/S22 sound table counts, and level object/segment summaries.
- Defer or avoid: exposing detailed HAM physics constants, full level texture summaries, or detailed robot/weapon stat diffs in Kotlin.
