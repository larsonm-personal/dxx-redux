// Included in the existing particle source owner after the shared adapter
void particle_smoke_creation_controls()
{
	using namespace particle_geometry_reference;
	const xl_compat::ParticleFace face{ { { { -655360, -655360, 13 }, { 655360, -655360, -19 }, { 655360, 655360, 27 }, { -655360, 655360, -35 } } }, 4 };
	unsigned count = 0, updates = 0;
	for (int value : { 1, 5, 10 })
		for (bool on_face : { false, true })
			for (bool blow_up : { false, true })
				for (bool default_color : { false, true })
					for (float brightness : { 0.0f, 0.5f, 1.0f })
						for (unsigned seed = 0; seed < 32; ++seed) {
							xl_compat::Object authored{};
							authored.render = 11;
							authored.render_data = { value, value, value, value, 1, 10, default_color ? 0 : 255, default_color ? 0 : 128, 0, 128, on_face ? 1 : 0, 0, 1 };
							auto definition = xl_compat::particle_emitter_definition(xl_compat::decode_particle_effect(authored, 27), { -17031, 10000, 60000 }, true);
							definition.blow_up = blow_up;
							RandomInput random;
							random.generated = true;
							random.state = seed;
							source_random = random;
							const xl_compat::ParticlePoint position{ 112233, -445566, 778899 };
							const auto actual = xl_compat::create_smoke_particle(definition, position, on_face ? &face : nullptr, 0, false, 123, brightness, random);
							CParticle source;
							auto source_position = point(position), direction = point(definition.direction);
							CFixVector corners[4];
							for (unsigned i = 0; i < 4; ++i) corners[i] = point(face.corners[i]);
							CFixMatrix orientation;
							CFloatVector color;
							for (unsigned i = 0; i < 4; ++i) color.v.vec[i] = definition.color[i];
							particle_segment.function = 0;
							if (!source.Create(&source_position, &direction, &orientation, 0, definition.life, definition.speed, 1, 0, definition.scale, &color, 123, blow_up, 1, brightness, on_face ? corners : nullptr)) throw std::runtime_error("Source rejected authored smoke");
							const auto &motion = actual.motion;
							const auto &appearance = actual.appearance;
							if (motion.position != point(source.m_vPos) || motion.drift != point(source.m_vDrift) || motion.direction != point(source.m_vDir) || motion.life != source.m_nLife || motion.ttl != source.m_nTTL || motion.radius != source.m_nRad || motion.width != source.m_nWidth || motion.height != source.m_nHeight || motion.frame != source.m_iFrame || motion.rotation != source.m_nRotFrame || motion.orientation != source.m_nOrient || motion.decay != source.m_decay || appearance.bright != bool(source.m_bBright) || random.state != source_random.state || random.trace != source_random.trace)
								throw std::runtime_error("Authored smoke creation differs at seed " + std::to_string(seed) + " value " + std::to_string(value));
							for (unsigned i = 0; i < 4; ++i)
								if (appearance.color[i] != source.m_color[0].v.vec[i] || appearance.target[i] != source.m_color[1].v.vec[i] || appearance.rendered[i] != source.m_renderColor.v.vec[i]) {
									std::cerr << "Smoke color field " << i << " seed " << seed << " default " << default_color << " blowup " << blow_up << " brightness " << brightness << " actual " << appearance.color[i] << ',' << appearance.target[i] << ',' << appearance.rendered[i] << " source " << source.m_color[0].v.vec[i] << ',' << source.m_color[1].v.vec[i] << ',' << source.m_renderColor.v.vec[i] << '\n';
									throw std::runtime_error("Authored smoke creation color differs");
								}
							if (source.m_nFrames != 8 || !source.m_bAnimate || source.m_bEmissive || source.m_texCoord.v.u != float(motion.frame % 8) / 8 || source.m_texCoord.v.v != float(motion.frame / 8) / 8) throw std::runtime_error("Authored smoke atlas differs");
							if (seed < 4)
								for (int scenario = 0; scenario < 6; ++scenario) {
									auto moving = actual;
									auto reference = source;
									auto moving_random = random;
									source_random = random;
									WorldInput world;
									world.located = scenario == 1 ? -1 : 0;
									if (scenario == 2 || scenario == 3) {
										world.normal = xl_compat::ParticlePoint{ scenario == 2 ? 65536 : -65536, 0, 0 };
										moving.motion.drift = { 655360, 0, 0 };
										moving.motion.direction = { 0, 0, 0 };
										reference.m_vDrift = point(moving.motion.drift);
										reference.m_vDir = point(moving.motion.direction);
									}
									if (scenario == 5) world.portal = 1;
									source_world = world;
									gameOpts->render.particles.bCollisions = scenario != 4;
									for (int offset : { 0, 1, 16, 249, 250, 251, 500, 501, 750, 1000, 2000, 6000, 300000 }) {
										const bool result = xl_compat::advance_smoke_particle(moving, 123 + offset, brightness, moving_random, world, scenario != 4);
										const bool expected = reference.Update(123 + offset, brightness, 0) != 0;
										const auto &m = moving.motion;
										if (result != expected || m.position != point(reference.m_vPos) || m.drift != point(reference.m_vDrift) || m.direction != point(reference.m_vDir) || m.life != reference.m_nLife || m.decay != reference.m_decay || m.radius != reference.m_nRad || m.width != reference.m_nWidth || m.height != reference.m_nHeight || m.updated != reference.m_nUpdated || m.moved != reference.m_nMoved || m.segment != reference.m_nSegment || m.bounce != reference.m_nBounce || m.locate_delay != reference.m_nDelayPosUpdate || moving_random.state != source_random.state || moving_random.trace != source_random.trace || world.lookups != source_world.lookups || world.collisions != source_world.collisions)
											throw std::runtime_error("Authored smoke update differs at scenario " + std::to_string(scenario) + " offset " + std::to_string(offset));
										for (unsigned i = 0; i < 4; ++i)
											if (moving.appearance.color[i] != reference.m_color[0].v.vec[i] || moving.appearance.rendered[i] != reference.m_renderColor.v.vec[i]) throw std::runtime_error("Authored smoke update color differs");
										++updates;
									}
								}
							++count;
						}
	std::cout << "Pinned authored smoke: " << count << " creation comparisons with color, motion, lifetime and atlas passed\n";
	std::cout << "Pinned authored smoke: " << updates << " updates with collision, lookup, drag, growth and retirement passed\n";
}
