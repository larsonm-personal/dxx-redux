// Included after the existing bitmap, depth and glow owners
namespace
{
xl_compat::LightningGlowGpu sprite_glow;
}

void xl_compat::finish_sprite_glow()
{
	sprite_glow.end();
}
void xl_compat::discard_sprite_glow(bool abandoned)
{
	sprite_glow.discard(abandoned);
}

void xl_compat::draw_fireball_sprite(grs_bitmap &image, const ParticleQuad &quad)
{
	ParticleSoftRestore restore;
	std::array<float, 16> projection{};
#ifdef ANDROID
	std::copy_n(gles3_shim_get_projection(), 16, projection.begin());
#else
	glGetFloatv(GL_PROJECTION_MATRIX, projection.data());
#endif
	std::array<int, 4> viewport{};
	GLint target = 0;
	glGetIntegerv(GL_VIEWPORT, viewport.data());
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
	std::array<int, 4> extent{ 32767, 32767, -32767, -32767 };
	for (const auto &vertex : quad.vertices) {
		extend_glow_viewport(extent, { vertex.position[0], vertex.position[1], -vertex.position[2] }, projection, viewport[2], viewport[3]);
	}
	// Source optional glow/depth preparation may fail independently of the image
	try {
		prepare_particle_soft();
		verify_particle_soft_depth();
		if (!particle_soft.captured) {
			particle_soft.depth.capture();
			particle_soft.captured = true;
		}
		const GLenum attachment = particle_soft.depth.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
		sprite_glow.begin(GLuint(target), particle_soft.depth.texture, attachment, particle_soft.depth.width, particle_soft.depth.height, viewport, 2, false, .85f);
	} catch (const std::runtime_error &) {
		sprite_glow.discard(false);
		draw_bitmap_sprite(image, quad, 10);
		return;
	}
	// Source sprite SetViewport retains cumulative item extents across the batch
	sprite_glow.draw_sprite(extent, true, [&] { draw_bitmap_sprite(image, quad, 10); });
}
