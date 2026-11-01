// Actual pinned Create/Update bodies share the existing source adapter
void particle_bubble_creation_controls()
{
	using namespace particle_geometry_reference;
	const xl_compat::ParticleFace face{ { { { -655360, -655360, 13 }, { 655360, -655360, -19 }, { 655360, 655360, 27 }, { -655360, 655360, -35 } } }, 4 };
	unsigned count = 0, updates = 0;
	for (int value : { 1, 2, 5, 10 })
		for (bool on_face : { false, true })
			for (bool default_color : { false, true })
				for (bool turned : { false, true })
					for (unsigned seed = 0; seed < 32; ++seed) {
						xl_compat::Object authored{};
						authored.render = 11;
						authored.render_data = { value, value, value, value, 2, 9, default_color ? 0 : 255, default_color ? 0 : 128, 0, 100, on_face ? 1 : 0, 2, 1 };
						const xl_compat::ParticleOrientation basis = turned ? xl_compat::ParticleOrientation{ { { 0, 0, -65536 }, { 0, 65536, 0 }, { 65536, 0, 0 } } } : xl_compat::ParticleOrientation{ { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } } };
						const auto definition = xl_compat::particle_emitter_definition(xl_compat::decode_particle_effect(authored, 27), basis[2], true);
						RandomInput random;
						random.generated = true;
						random.state = seed;
						source_random = random;
						const xl_compat::ParticlePoint position{ 123456, -345678, 987654 };
						const auto actual = xl_compat::create_bubble_particle(definition, position, basis, on_face ? &face : nullptr, 0, false, 123, random);
						CParticle source;
						auto source_position = point(position), direction = point(definition.direction);
						CFixMatrix orientation;
						orientation.m.dir.r = point(basis[0]);
						orientation.m.dir.u = point(basis[1]);
						orientation.m.dir.f = point(basis[2]);
						CFixVector corners[4];
						for (unsigned i = 0; i < 4; ++i) corners[i] = point(face.corners[i]);
						CFloatVector color;
						for (unsigned i = 0; i < 4; ++i) color.v.vec[i] = definition.color[i];
						particle_segment.function = 0;
						if (!source.Create(&source_position, &direction, &orientation, 0, definition.life, definition.speed, 2, 0, definition.scale, &color, 123, definition.blow_up, -1, 0.9f, on_face ? corners : nullptr)) throw std::runtime_error("Source rejected authored bubbles");
						const auto &m = actual.motion;
						if (m.position != point(source.m_vPos)) std::cerr << "Bubble position seed " << seed << " actual " << m.position[0] << ',' << m.position[1] << ',' << m.position[2] << " source " << source.m_vPos.v.coord.x << ',' << source.m_vPos.v.coord.y << ',' << source.m_vPos.v.coord.z << '\n';
						if (m.position != point(source.m_vPos) || m.drift != point(source.m_vDrift) || m.direction != point(source.m_vDir) || m.life != source.m_nLife || m.ttl != source.m_nTTL || m.radius != source.m_nRad || m.width != source.m_nWidth || m.height != source.m_nHeight || m.frame != source.m_iFrame || m.decay != source.m_decay || m.bounce != source.m_nBounce || random.state != source_random.state || random.trace != source_random.trace) throw std::runtime_error("Bubble creation differs at seed " + std::to_string(seed));
						for (unsigned i = 0; i < 4; ++i)
							if (actual.color[i] != source.m_color[0].v.vec[i] || actual.target[i] != source.m_color[1].v.vec[i] || actual.color[i] != source.m_renderColor.v.vec[i]) throw std::runtime_error("Bubble creation color differs");
						if (source.m_nFrames != 4 || !source.m_bAnimate || source.m_bRotate || source.m_nRotFrame || source.m_nOrient) throw std::runtime_error("Bubble atlas or orientation differs");
						++count;
						if (seed >= 4) continue;
						for (int scenario = 0; scenario < 6; ++scenario) {
							auto moving = actual;
							auto reference = source;
							source_random = random;
							WorldInput world;
							world.wet = particle_segment.water = scenario != 1;
							world.located = scenario == 2 ? -1 : 0;
							if (scenario == 3) world.normal = xl_compat::ParticlePoint{ 65536, 0, 0 };
							if (scenario == 5) world.portal = 1;
							source_world = world;
							gameOpts->render.particles.bCollisions = scenario != 4;
							for (int offset : { 0, 1, 16, 249, 250, 251, 500, 501, 750, 1000, 6000, 300000 }) {
								const bool result = xl_compat::advance_bubble_particle(moving, 123 + offset, world, scenario != 4);
								const bool expected = reference.Update(123 + offset, 0.9f, 0) != 0;
								const auto &motion = moving.motion;
								if (result != expected || motion.position != point(reference.m_vPos) || motion.life != reference.m_nLife || motion.segment != reference.m_nSegment || motion.updated != reference.m_nUpdated || motion.moved != reference.m_nMoved || motion.bounce != reference.m_nBounce || motion.locate_delay != reference.m_nDelayPosUpdate || world.lookups != source_world.lookups || world.collisions != source_world.collisions || random.state != source_random.state || random.trace != source_random.trace) throw std::runtime_error("Bubble update differs at scenario " + std::to_string(scenario) + " offset " + std::to_string(offset));
								++updates;
							}
						}
					}
	particle_segment.water = false;
	std::cout << "Pinned bubbles: " << count << " creation and " << updates << " water/collision/lifetime updates passed\n";
}
