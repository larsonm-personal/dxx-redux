# Developer-only player FVI observer

The `DXX_PUMO_CONTACT_DEVELOPER_PROBE` target can opt into a bounded player
physics FVI receipt with a `player_fvi_plan` object. This is separate from
the legacy `observe_player_fvi` first/last sample diagnostic. Neither mode is
active by default, and they cannot be enabled together.

The object must contain `level`, `player_object`, `player_signature`,
`allowed_start_segments`, `max_frames`, and `max_samples`. The level, object
slot, and signature must identify the player after initial setup or checkpoint
restore. Supply 1 to 16 distinct source-level segment indices, a frame budget
from 1 to 10000, and a sample budget from 1 to 256. An example with abstract
identities is:

    "player_fvi_plan": {
      "level": 1,
      "player_object": 0,
      "player_signature": 1,
      "allowed_start_segments": [0, 1],
      "max_frames": 10,
      "max_samples": 40
    }

Each retained sample records its frame, enclosing input-through-world-update phase, load generation,
and the existing native FVI values, including game time and hit result. The
frame bracket starts before player weapon/input work and ends after world
update. Only FVI callbacks within that bracket and bound to the selected
player, level, and allowed start segments are retained. Exceeding either budget,
missing an in-order frame or its player FVI, observing the bound player in an
unexpected start segment, or changing level/loading while bound fails the probe rather
than reporting a complete observation. This packet does not observe weapon
births, retirement, or damage, and it is not a native gameplay qualification.
