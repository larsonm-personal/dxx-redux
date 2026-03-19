# D1 D2 Compilation Fixes Summary

## Overview
Fixed compilation warnings and errors in D1 and D2 `net_udp.c` files related to uninitialized Android logging code.

## Root Cause
The Android multiplayer sync logging added several `logbuf` local variables that were only used inside `#ifdef __android__` conditional blocks. When building without Android platform, these variables were declared but never used, causing "unused variable" warnings.

## Fixes Applied

### D1 Changes (d1/main/net_udp.c)

1. **Line 3708**: Fixed duplicate `int int` declaration
   - Before: `int int\nnet_udp_sync_poll(...)`
   - After: `int\nnet_udp_sync_poll(...)`

2. **net_udp_sync_poll() (lines 3714-3715)**
   - Wrapped logbuf declaration in `#ifdef __android__`
   - Added `(void)poll_count;` in `#else` block to suppress unused warning (poll_count is incremented outside ifdef)

3. **net_udp_wait_for_sync() (lines 5462-5467)**
   - Changed unconditional logbuf declaration to conditional
   - Wrapped `char logbuf[256];` in `#ifdef __android__` block
   - Added suppression in `#else` block for logbuf (since it doesn't exist in non-Android build)

4. **net_udp_level_sync() (lines 5621-5624)**
   - Wrapped logbuf declaration in `#ifdef __android__`

### D2 Changes (d2/main/net_udp.c)

1. **net_udp_request_poll() (lines 5633-5638)**
   - Wrapped logbuf declaration in `#ifdef __android__`

2. **net_udp_wait_for_requests() (lines 5677-5682)**
   - Wrapped logbuf declaration in `#ifdef __android__`
   - Note: logbuf used in line 5705 which is inside larger `#ifdef __ANDROID__` block

3. **net_udp_wait_for_sync() (lines 5564-5569)**
   - Wrapped logbuf declaration in `#ifdef __android__`

4. **net_udp_send_sync() (lines 4971-4975)**
   - Wrapped logbuf declaration in `#ifdef __android__`

5. **net_udp_level_sync() (lines 5750-5755)**
   - Wrapped logbuf declaration in `#ifdef __android__`

6. **net_udp_sync_poll() (lines 3769-3774)**
   - Wrapped logbuf declaration in `#ifdef __android__`

## Pattern Applied
All `logbuf` variables used only for Android logging now follow this pattern:

```c
#ifdef __android__
char logbuf[256];
#endif
```

Where the variable is declared in the conditional block where it's actually used.

## Variables Affected
- `logbuf`: Used for snprintf() calls that format Android-specific diagnostic messages
- `poll_count`: Static counter incremented outside ifdef but used inside ifdef (suppressed with `(void)poll_count;` in non-Android build)

## Result
These changes ensure:
1. No unused variable warnings when building without Android
2. Conditional compilation properly protects variable declarations
3. Full Android-specific diagnostic logging preserved when building with Android
4. No duplicate declarations or malformed preprocessor blocks

## Testing
Build the Android APK with:
```bash
cd android
./gradlew.bat assembleRelease
```

The build should now complete without unused variable warnings in net_udp.c for both D1 and D2.
