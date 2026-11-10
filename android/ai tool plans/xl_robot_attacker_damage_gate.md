# Ordinary XL hostile robot damage attribution

Pinned D2X-XL ApplyDamageToRobot rejects a non-guidebot robot attacker when the
victim does not attack robots. Ordinary radial explosions reach Redux's common
damage consumer after applying force, while camera splash already has a separate
prefilter. Add the missing common-consumer restriction only for XL singleplayer
with robotskillrobots disabled, before bookkeeping, shield changes and death or
object-trigger dispatch. Preserve existing classic, multiplayer and cheat-enabled
behavior. Do not claim implementing the reference cheat or reprogramming model.

Use the existing linked CPU target. Demonstrate failure on the unchanged parent,
then test surviving and lethal boss/non-boss victims, rejected side effects,
guidebot/player/nonrobot/no-killer attribution, cheat-enabled and classic and
multiplayer controls. Keep nonrobot attribution separate from hostile rejection.
Remove the restriction and the guidebot exemption independently to check test
sensitivity. Extend the existing native projectile fixture with actual ordinary
radial hostile/guidebot/player/cheat controls and force-before-shield-rejection.

Use existing exact-head hosted native asset execution for runtime qualification.
Do not introduce another contact harness. CPU tests alone do not qualify native
blast geometry, save behavior or a complete campaign. No merge is authorized.
