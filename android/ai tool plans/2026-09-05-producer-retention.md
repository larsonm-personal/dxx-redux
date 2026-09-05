# Producer startup retention

- [x] Default manual generation cleanup to one newest, with no age exemption
- [x] Support planned output paths in the shared retention helper and retain three prior outputs
- [x] Move existing producer hooks before output creation and cover missing run/native build/package producers
- [x] Test fresh-generation retention and producer hook ordering, update documentation, and run scoped quality checks

Validation: cleanup fixture suites passed, scoped code quality passed, and Windows Gradle assembleDebug dry run passed with startup retention and caller-chain handling.
