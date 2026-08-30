# LAN lobby bidirectional liveness

## Symptoms

- Client-to-host chat arrives, but host-to-client chat does not arrive even immediately after joining.
- After roughly one minute, client ready changes no longer appear locally or on the host.
- The client can leave and rejoin.
- Both screens remain on, so screen-off behavior is out of scope for this investigation.

## Plan

1. [Completed] Trace UDP socket binding, learned peer endpoints, host broadcasts, client heartbeats, and transport job ownership.
2. [Completed] Add durable packet and job-lifecycle diagnostics at the points needed to distinguish routing failure from coroutine or socket failure.
3. [Completed] Fix deterministic acknowledgement and lifecycle defects found during the trace and add focused regression coverage.
4. [Completed] Run scoped quality checks, focused unit tests, the two-device integration entry point, and the Android debug build. The integration entry point skipped because no emulators were attached.
