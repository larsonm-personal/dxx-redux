# SDL mixer diagnostics extraction plan

## Goal

Move duplicated Android mixer declarations, chunk timing, init telemetry, and
SFX latency/start logging out of the upstream-original D1 and D2 mixer files.

## Boundary

- Keep each game's sample conversion, source-rate policy, channel allocation,
  and local `Mix_OpenAudio` error tag unchanged.
- Put the Android driver diagnostic declarations behind one public header.
- Pass chunk bytes and length to the shared start logger without exposing game
  sound tables or conversion state.
- Preserve probe polling before conversion and start notification after channel
  distance setup.

## Validation

- Build desktop D1/D2 with the Android source excluded.
- Link all Android ABIs and run focused primary/secondary SFX automation.
- Confirm latency and start probe counters remain nonzero and ordered.
