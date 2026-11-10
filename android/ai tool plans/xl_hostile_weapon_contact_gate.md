# Ordinary hostile-robot weapon contact

Pinned D2X-XL rejects robot-owned ordinary weapon contact when the parent does
not attack robots. This gate comes after boss contact bookkeeping, before
persistent history, splash, projectile attrition and ordinary victim effects.
Unlike the common radial-damage gate, this contact gate has no guidebot exception.

Add the narrow XL singleplayer/cheat-disabled return in that source position.
Preserve classic/D1-in-D2, multiplayer and cheat-enabled behavior, and leave the
boss contact consumer before the return. Do not change the later common shield
rule or move the return into that damage consumer.

Extend the existing native projectile fixture with real robot/guidebot-owned
persistent and nonpersistent shots, repeated rejected contact, unchanged victim
and nearby actor physics, unchanged projectile/history/RNG, and a nearby splash
negative control. Include allowed player, classic, multiplayer-dispatch and
cheat controls. This is controlled dispatch, not full multiplayer qualification.

Source demonstrates the old path records persistent history or retires shots,
and can dispatch splash before the later shield gate. Local common CPU coverage
does not retain this full contact path; do not call it a native baseline pass or
failure. Use existing exact-head hosted native execution and disclose pending
results. No new linkage harness, environment expansion or merge is authorized.
