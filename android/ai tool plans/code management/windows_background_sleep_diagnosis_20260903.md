# Windows background task sleep diagnosis

## Goal

Determine why background tasks pause after roughly eight minutes despite the
configured display and system sleep settings.

## Plan

1. Record the active power scheme, supported sleep states, and all effective AC
   and DC sleep/display timers.
2. Inspect Windows sleep, resume, power-policy, and Modern Standby events around
   recent occurrences.
3. Check hidden unattended-sleep settings, battery saver, hibernation, and
   policy overrides that can bypass the visible Settings values.
4. Identify the most likely cause and provide a narrowly scoped correction and
   verification procedure.

## Status

- [x] Effective settings collected
- [x] Event history inspected
- [x] Cause identified
- [x] Correction and verification documented

## Findings

- The Lenovo 83JM supports S0 Low Power Idle (Modern Standby), not S3 sleep.
- The active Balanced plan has the expected AC values: display off after 2,700
  seconds and sleep after 18,000 seconds.
- Kernel-Power event 506 records repeated actual Modern Standby entries with
  reason `Idle Timeout`. On September 3, an AC-powered active session began at
  07:20:49 and entered standby at 07:33:02, only 12 minutes 13 seconds later.
- Lenovo Intelligent Sensing is active. The `SmartSense` service and
  `SmartSenseController` process are running, and its HPD configuration has
  presence-on-leave enabled (`PresentLeave=1`, `CameraDetect=1`). This can turn
  off the display when the user is judged absent, bypassing the ordinary
  45-minute display timer. On a Modern Standby machine, display-off begins the
  standby session and Windows pauses desktop applications.
- A hidden 120-second System unattended sleep timeout is also configured. It
  can shorten a session following an unattended wake, but it does not explain
  the initial presence-triggered display-off by itself.

## Recommended correction

Disable Lenovo's user-presence or zero-touch-lock feature in Lenovo Vantage
under Smart Assist or Intelligent Sensing. If the UI separates features,
disable Lock on Leave or Zero Touch Lock while retaining Approach/Login if
desired. Then verify that Kernel-Power event 506 no longer occurs before the
configured 45-minute display timeout.
