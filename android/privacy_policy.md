Privacy Policy for DXX-Revival (com.dxxrevival.app)

## On-device

- The app stores these locally:
  - settings
  - game files
  - saves
  - multiplayer IDs
  - recent server addresses
  - logs and crash dumps
- There are no ads or advertising trackers
- Crash and debug logs are not automatically sent anywhere. To create a helpful bug report you can export them through the Advanced tab; they may include device details, file paths, callsigns, and IP addresses
- Android can back up or transfer app data according to your device settings. The installation ID is excluded
- Clearing app storage or uninstalling removes local app data, but not exported files, backups, or records held by servers or other people

## Online features

- Netplay:
  - callsigns, IP addresses, player IDs, chat, lobby details, and game state. Mission sharing transfers mission files. Servers can store player records, friend/block lists, connection events, and match results; each operator controls their records
- Google Play Games sign-in provides your game-specific player ID to the matchmaking server. Its SDK also collects analytics and diagnostics and can start when the launcher opens. The app also checks Google Play for updates. See [Google's privacy policy](https://policies.google.com/privacy) and [Play Games disclosures](https://developer.android.com/games/pgs/data-collection)
- Download sites receive your IP address and the file requested
- AcoustID music lookup (default: off) sends a fingerprint and track duration, not the recording itself. AcoustID also sees your IP address
  - song lookups are done locally by default, using a built-in chromaprint database

## Exlanation of Permissions

- Network and Nearby Wi-Fi access:
  - netplay and downloads. No location permission is requested
- Foreground-service and wake-lock:
  - keeps the app and multiplayer connections running
- file picker gives access to files or folders you select
- Camera permission is requested when you choose "Read QR code". Images are processed locally, not saved or uploaded. The scanned host address may be saved in your recent server list

## Source and contact

The app is built from the source in this repository. I've shared everything on github other than the google play secrets I use to publish the app

- contact: larsonm.pdx@gmail.com
