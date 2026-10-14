// Existing operation matrices shared by retail and the audited public profile
static nlohmann::json exercise_player_resource_damage(bool native)
{
	using nlohmann::json;
	json pickups = json::array(), damage = json::array();
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		Difficulty_level = difficulty;
		for (const int id : { POW_ENERGY, POW_SHIELD_BOOST })
			for (const fix start : { 100 * F1_0, MAX_ENERGY - F1_0 / 2, MAX_ENERGY }) {
				init_test_corridor();
				Players[0].objnum = 0;
				Players[0].energy = Players[0].shields = start;
				Players[0].score = 0;
				Players[0].lives = 3;
				Players[0].flags = 0;
				object pickup = {};
				pickup.type = OBJ_POWERUP;
				pickup.id = id;
				const int used = do_powerup(&pickup);
				const fix result = id == POW_ENERGY ? Players[0].energy : Players[0].shields;
				const fix amount = (difficulty == 0 && !native ? 27 : 18 - 3 * difficulty) * F1_0;
				require(result == (std::min) (MAX_ENERGY, start + amount) && used == (start < MAX_ENERGY), "real pickup uses native or D2 difficulty amount and retains saturation/consumption");
				pickups.push_back({ difficulty, id, start, used, result, +Players[0].score });
			}
		for (const int invulnerable : { 0, 1 })
			for (int kind = 0; kind < 3; ++kind) {
				init_test_corridor();
				Players[0].objnum = 0;
				Players[0].flags = invulnerable ? PLAYER_FLAGS_INVULNERABLE : 0;
				Players[0].shields = ConsoleObject->shields = 100 * F1_0;
				ConsoleObject->mtype.phys_info.mass = F1_0;
				vms_vector origin = {};
				const int other = obj_create(OBJ_CLUTTER, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
				require(other > 0, "create contact source");
				d_srand(13);
				if (kind == 0) apply_force_damage(ConsoleObject, 64 * F1_0 + 7, &Objects[other]);
				else if (kind == 1) {
					TmapInfo[1].damage = 20 * F1_0 + 7;
					TmapInfo[1].flags = 0;
					vms_vector point = { 0, 0, -10 * F1_0 };
					scrape_player_on_wall(ConsoleObject, 0, 5, &point);
				} else {
					vms_vector point = { 0, 0, 2 * F1_0 };
					require(object_create_badass_explosion(nullptr, 0, &point, F1_0, VCLIP_SMALL_EXPLOSION, 20 * F1_0, 10 * F1_0, 0, 0) != nullptr, "create actual area-damage explosion");
				}
				const fix delta = 100 * F1_0 - Players[0].shields;
				require(invulnerable ? delta == 0 : delta > 0, "real contact/lava/blast honors invulnerability");
				const auto &physics = ConsoleObject->mtype.phys_info;
				// Equal RNG counts do not prove equal order: lava shove and spin share the stream
				damage.push_back({ difficulty, invulnerable, kind, delta, d_rand_get_call_count(), { physics.velocity.x, physics.velocity.y, physics.velocity.z }, { physics.rotvel.x, physics.rotvel.y, physics.rotvel.z } });
			}
	}
	return { { "pickups", pickups }, { "damage", damage } };
}

static nlohmann::json exercise_replacement_drops(bool native)
{
	auto drops = nlohmann::json::array();
	for (const int id : { POW_CLOAK, POW_VULCAN_WEAPON, POW_SPREADFIRE_WEAPON, POW_PLASMA_WEAPON, POW_FUSION_WEAPON, POW_QUAD_FIRE, POW_VULCAN_AMMO, POW_ENERGY, POW_EXTRA_LIFE })
		for (int flags = 0; flags < 8; ++flags)
			for (int nearby = -1; nearby < 4; ++nearby)
				for (const unsigned seed : { 1u, 7u, 1000u }) {
					init_test_corridor(4);
					Players[0].objnum = 0;
					Players[0].primary_weapon_flags = flags & 1 ? 0x1f : 1;
					Players[0].flags = flags & 1 ? PLAYER_FLAGS_QUAD_LASERS : 0;
					Players[0].primary_ammo[VULCAN_INDEX] = flags & 2 ? VULCAN_AMMO_MAX : 0;
					Game_mode = flags & 4 ? GM_MULTI : 0;
					if (nearby >= 0) {
						vms_vector point = { 0, 0, nearby * 20 * F1_0 };
						require(obj_create(OBJ_POWERUP, id, nearby, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE) > 0, "create actual nearby pickup");
					}
					for (const int gated : { 0, 1 }) {
						object container = {};
						container.type = OBJ_ROBOT;
						container.segnum = 0;
						container.matcen_creator = gated ? BOSS_GATE_MATCEN_NUM : 0;
						container.contains_type = OBJ_POWERUP;
						container.contains_id = id;
						container.contains_count = 3;
						d_srand(seed);
#ifdef DXX_BUILD_DESCENT_II
						if (!native) {
							const object original = container;
							const auto draws = d_rand_get_call_count();
							require(!d1_in_d2_replace_powerup(&container) && std::memcmp(&original, &container, sizeof(container)) == 0 && d_rand_get_call_count() == draws,
							        "inactive native drop operation leaves D2 object bytes and RNG intact");
						}
#endif
						maybe_replace_powerup_with_energy(&container);
						if (!native && (flags & 1) && !gated &&
						    (id == POW_SPREADFIRE_WEAPON || id == POW_PLASMA_WEAPON || id == POW_FUSION_WEAPON || id == POW_QUAD_FIRE))
							require(container.contains_count == 3 && (container.contains_id == POW_ENERGY || container.contains_id == POW_SHIELD_BOOST),
							        "ordinary D2 duplicate drops retain their count and energy/shield choice");
						drops.push_back({ id, flags, nearby, seed, gated, container.contains_type, container.contains_id, container.contains_count, d_rand_get_call_count() });
					}
				}
	Game_mode = 0;
	return drops;
}
