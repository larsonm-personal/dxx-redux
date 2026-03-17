# windows build dependencies
* see auto-download scripts in `android/get_deps/`
* create `dependency_base.txt` in the repo root with the path to your dependency directory (e.g. `C:\local`)
* android NDK (gets automatically found under `<dependency_base>/android-ndk.../`)
* android SDK command line tools (`<dependency_base>/android-sdk.../`)
* JDK (`<dependency_base>/jdk.../`)
* wsl (for bash)

# VS code extensions
* extension pack for java (ms)

# google play console setup
## basic, and programmatic access
* https://support.google.com/googleplay/android-developer/answer/6112435
* note on programmatic auth https://stackoverflow.com/questions/76541480/how-does-fully-create-an-internal-release-on-google-play-console-via-api

## gpgs
* https://developer.android.com/games/pgs/start
1. Create a Play Games project in Google Play Console
2. Configure OAuth consent screen
3. Generate OAuth 2.0 client IDs:
   - Android client ID (linked to app signing key SHA-1)
   - Web/server client ID (for server-side token exchange)
4. Add the Games project to the app's Play Console listing
