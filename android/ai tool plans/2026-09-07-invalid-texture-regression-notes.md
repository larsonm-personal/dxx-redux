# Invalid texture regression notes

Record invalid texture indices in the engine before substitution, reset on every level load, and aggregate in numeric order. Serialize the notes into level metadata and route confirmation JSON; retain them in compact simulation regression records.

Extend the loader integration fixture to check the three affected levels' notes and absence of notes on the clean KCXF2 load. Build both engines, run the focused fixture, and regenerate only the affected simulation entries.

## Validation

- Both Windows engine builds passed without new compiler warnings
- All four loader fixtures passed, including exact occurrence notes in compact simulation JSON
- One metadata worker processed Comet, clean Abandoned, and Comet again: notes were respectively two occurrences, absent, and two occurrences, verifying per-load reset and metadata serialization
- Regenerated only the three affected simulation entries in the two CD simulation files; all three now report successful routes with texture notes
- Scoped code quality and diff whitespace checks passed
- Full corpus and Android device runs were not repeated
