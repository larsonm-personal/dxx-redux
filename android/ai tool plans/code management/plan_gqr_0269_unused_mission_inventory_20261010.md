# GQR-0269: remove retired mission inventory wrapper

Diagnosis and implementation plan only. Proposed GQF-0283 is a NEW maintainability finding. No product or test edits or execution in this tranche.

Problem: FileSetMissionInventory and FileSetMissionEntry are branch additions whose only current consumers are FileSetMissionInventoryTest. The broad source/native/build/manifest/reflection-text census finds ten matches, all within those two files. Production setup inventory/enable/delete and launch projection use FileSetContentManager and FileSetContentCatalog. The unused wrapper retains a second scan/deletion API, including unmanaged per-file deletion and manifest updates, while its fixture verifies an API the application does not call. This is an obsolete branch maintenance surface, not a demonstrated production deletion failure.

- [x] Whole helper and fixture, actual production owners and broad caller census
- [ ] Remove retired helper/data class and its dedicated fixture
- [ ] Confirm unique valuable ownership/deletion assertions remain on production manager/catalog fixtures
- [ ] Verify no references, focused maintained manager/catalog contracts and Android Kotlin compilation

Allowed product removal: android/app/src/main/java/com/dxxredux/app/FileSetMissionInventory.kt. Allowed fixture removal: android/app/src/test/java/com/dxxredux/app/FileSetMissionInventoryTest.kt. If a unique ownership assertion is missing, adapt it to the actual FileSetContentManagerTest.kt or FileSetContentCatalogTest.kt; do not preserve the wrapper solely for tests. Avoid adding replacement facades, migrations, new registries or native hooks. Keep base/player exclusions, shared dependencies, managed enabled state, retained grants and actual deletion outcomes unchanged.

Acceptance: no source/build/manifest/reflection references remain; actual manager removal still removes an owned mission while retaining base/player data and unrelated entries. Existing catalog shared-dependency, malformed-descriptor, movie/CUE/demo/CD exclusions and manager projection/state controls continue to exercise production owners. Future runtime verification should use existing setup content deletion/launch flows rather than the retired helper. Deletion transaction/failure repair remains BR-0472, not part of this cleanup.

Priority: 34 LOW (H/M/B/C/R=12/0/2/10/10). Maintenance harm, zero original-file merge payoff, current test-only exposure, exact source confidence and bounded removal with maintained production fixtures. Seventy branch-added source lines and64 isolated fixture lines are candidates for removal; zero inherited savings and no code removal claimed yet.
