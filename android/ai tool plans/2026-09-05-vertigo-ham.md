# Vertigo HAM lookup

- [x] Verify source HOG contents and trace metadata/mission loading
- [x] Mount the Vertigo archive before metadata's HAM preflight
- [x] Reproduce and verify native analysis with real Vertigo data, including missing HAM handling
- [x] Run scoped quality checks and distinguish the phone symptom from the confirmed host bug

- [x] Fix Vertigo HOG classification so the imported descriptor and archive share one managed mission
- [x] Verify launcher reconciliation/projection tests

Native D1/D2 builds and scoped quality checks passed. Real Vertigo CD scan passed (2 missions, 24 levels); reused-worker valid/missing-HAM tests passed. Existing split imports should be reimported with D2 + Vertigo into a fresh file set after updating the APK. Phone launch has not been retested on the user's device.

Both launcher suites passed (20 tests). No on-phone launch verification or APK installation was performed.
