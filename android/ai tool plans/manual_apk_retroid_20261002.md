# Manual APK Retroid validation

- Test the exact universal artifact from Actions run 37037829265 on Retroid Pocket 4 Pro JYPR42510121028
- Install the previously verified private-test-key signed copy as com.dxxredux.app.ci
- Preserve the existing Play/legacy installations and target every adb operation to the physical device
- Use ordinary release-app import and launch flows because external automation and run-as are disabled in the release APK
- Exercise both games, gameplay/menu transitions, suspend/resume and inspect crash/native-loader logs
- Capture device details, package/ABI evidence, logs and results under android/temp/
