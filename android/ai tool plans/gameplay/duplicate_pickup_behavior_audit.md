# Duplicate pickup behavior audit

- [x] Enumerate D1 and D2 pickups with single-player-only duplicate handling
- [x] Compare each pickup with the co-op all-players-owned feature
- [x] Verify ammo rack behavior and identify any remaining omissions
- [x] Report findings and recommended scope

## Findings

- D2 Full Map, Converter, Ammo Rack, Afterburner, and Headlight duplicates
  convert to energy in single-player but remain blocked in cooperative games
- Ammo Rack duplicates grant energy, not ammunition
- Duplicate Vulcan and Gauss weapons have separate ammo-transfer behavior and
  are intentionally governed by multiplayer ammo-duplication rules
- Cloak and Invulnerability duplicates remain unavailable while their effects
  are active in both single-player and multiplayer
- Full Map, Converter, and Ammo Rack acquisition do not currently schedule a
  co-op ship-status update, so extending their duplicate rule also requires
  synchronizing their ownership flags

## Implementation

- [x] Add a shared D2 all-connected-players flag predicate for duplicate pickups
- [x] Apply it to Full Map, Converter, Ammo Rack, Afterburner, and Headlight
- [x] Broadcast newly acquired equipment ownership
- [x] Run scoped formatting, Windows builds, Android native builds, and tests

## Implementation result

- All five D2 equipment duplicates now convert to energy when every connected
  cooperative player owns the item
- Equipment and quad ownership flags are broadcast immediately so every peer
  can evaluate the rule from current state
- Competitive multiplayer behavior is unchanged
- Scoped code-quality checks, D1 and D2 Windows builds, Android debug builds for
  all configured ABIs, and Android debug unit tests passed

## Vulcan and Gauss follow-up

- [x] Trace D1 and D2 duplicate Vulcan/Gauss ammo behavior and multiplayer modes
- [x] Apply the all-connected-players-owned rule without changing competitive modes
- [x] Verify partial-ammo and full-ammo pickup behavior
- [x] Run scoped formatting, Windows builds, Android native builds, and tests

### Vulcan and Gauss result

- Duplicate D1 Vulcan pickups grant the normal single-player Vulcan ammo-box
  amount once every connected cooperative player owns the Vulcan cannon
- Duplicate D2 Vulcan and Gauss pickups transfer their normal single-player
  stored ammo once every connected cooperative player owns that specific weapon
- Competitive multiplayer and cooperative games where a player still lacks the
  weapon continue to use the configured Gauss/Vulcan ammo-style rules
- Scoped code-quality checks, D1 and D2 Windows builds, Android debug builds for
  all configured ABIs, and Android debug unit tests passed
