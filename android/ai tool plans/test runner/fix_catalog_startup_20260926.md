# Repair unattended catalog startup

1. Correct the imported D1 metadata scenario owner to the existing top-level wrapper
2. Register missing tests in coverage families, keeping tests requiring caller-supplied fixtures or two preconfigured emulators explicit and host-only tests in the no-infrastructure tier
3. Run scoped code quality and the catalog regression through run_all_tests.ps1
