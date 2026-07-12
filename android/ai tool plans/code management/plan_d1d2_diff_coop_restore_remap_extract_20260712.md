# Coop restore client remap extraction plan

## Goal

Move the identical Android client-ID/callsign restore remap from both inherited
state files into the existing shared coop-save layer.

## Boundary

- Pass only the open rewind file and the saved player/object snapshots.
- Mutate live `Players` and `Objects` in the same slot order and return the
  mapped-player count.
- Preserve client-ID-first matching, callsign fallback, fresh-spawn behavior,
  object field copy order, reset, segment update, HUD text, and logs.

## Validation

- Build D1/D2 desktop and Android targets.
- Run coop save/restore coverage for same players, remapped slots, and a new or
  absent joiner.
