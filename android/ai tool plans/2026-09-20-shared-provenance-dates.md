# Shared provenance date input

- Delete the PowerShell archive date reader
- Share ZIP/7z date reading and serialization between Android adapters and the Kotlin host CLI
- Update host orchestration and integration fixtures without adding inference to scripts
- Verify shared Kotlin builds/tests and host/native provenance integration

Completed: helper removed; host CLI and Android share archive date conversion in
`ArchiveEntryDates`, with native provenance inference unchanged

Validation: shared ZIP/7z test and three Android provenance tests passed; all six
native integration cases passed; host generation for ROGUE.zip and KCXF2RMv11.7z
preserved the previous survey's provenance exactly; scoped formatting passed
