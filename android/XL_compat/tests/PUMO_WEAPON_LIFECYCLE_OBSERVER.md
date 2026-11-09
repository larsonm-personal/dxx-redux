# Bounded developer weapon lifecycle source receipt

`DXX_PUMO_CONTACT_DEVELOPER_PROBE` can opt into `weapon_lifecycle_plan`
for ordinary-player source diagnostics. The field is absent by default. It
requires `level`, `player_object`, `player_signature`, `max_frames` (1 to
10000), and `max_events` (1 to 256) matching the post-load source player.

The receipt records all occupied and empty object slots up to the highest
source and POST slot once each. Saved active weapons seed their slot and
signature without an invented birth. Within each frame, fixed-size storage
records raw clocks before weapon input, after weapon input, after game-time
advance, and after world update. Event records include phase, clock, load
generation, slot and signature. A cap hit, duplicate birth, orphan retirement,
identity mismatch, unbracketed event, load, or level change invalidates the
receipt.

Events cover generic weapon allocation and retirement, standard final parent
promotion, Omega's direct blob parent, buddy ownership rewrite, and reflected
clone provenance. A birth's zero-initialized parent fields are provisional.
A reflected clone retains its native unresolved parent fields; the observer
does not assign an owner. A marked-but-live weapon has no retirement event
until `obj_delete` runs.

The receipt is deliberately not a no-contact or no-hit result. It does not
observe weapon control, movement legs, bounces, teleports, direct or indirect
contact, or damage. Position records are endpoints, not swept paths. All such
completeness flags are false; uncertain paths must be checked separately.
This source-only packet supplies no native gameplay qualification.
