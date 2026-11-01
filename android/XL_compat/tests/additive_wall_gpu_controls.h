#ifdef XL_POG_SHADER_REFERENCE
#include "../inventory.h"
namespace additive_wall_source
{
#include "xl_reference_wall_glow_flags.h"
constexpr bool face_glow = (GLOW_FLAGS & GLOW_FACES) != 0;
#undef GLOW_LIGHTNING
#undef GLOW_SHIELDS
#undef GLOW_SPRITES
#undef GLOW_THRUSTERS
#undef GLOW_LIGHTTRAILS
#undef GLOW_POLYS
#undef GLOW_FACES
#undef GLOW_OBJECTS
#undef GLOW_HEADLIGHT
#undef BLUR_OUTLINE
#undef BLUR_SHADOW
#undef GLOW_FLAGS
int selection(int m_nWallType, unsigned flags, const char *pszName, int sparks)
{
	struct {
		struct {
			int bAdditive = 0, bSparks = 0;
		} m_info;
	} face;
	auto *m_pFace = &face;
	face.m_info.bSparks = sparks;
	struct {
		unsigned flags;
	} wall{ flags };
#define WALL(index) (&wall)
#include "xl_reference_wall_additive.h"
#undef WALL
#undef WALL_RENDER_ADDITIVE
	return face.m_info.bAdditive;
}
} // namespace additive_wall_source
static void additive_wall_source_controls()
{
	check(!additive_wall_source::face_glow, "Pinned source unexpectedly enables face glow");
	const auto previous = grd_curcanv->cv_blend_func;
	unsigned cases = 0;
	for (unsigned flags = 0; flags < 256; ++flags) {
		if (!(flags & xl_compat::wall_render_additive)) continue;
		for (const char *name : { "ordinary", "lava01", "force01", "misc17" })
			for (int sparks : { 0, 1, 2 }) {
				check(additive_wall_source::selection(0, flags, name, sparks) == 0 && additive_wall_source::selection(1, flags, name, sparks) == 0,
				      "Pinned source applies additive wall flags to non-wall faces");
				ParticleBlendSource reference;
				reference.SetBlendMode(additive_wall_source::selection(2, flags, name, sparks));
				for (auto stock : { GR_BLEND_NORMAL, GR_BLEND_ADDITIVE_A, GR_BLEND_ADDITIVE_C }) {
					grd_curcanv->cv_blend_func = stock;
					additive_wall_control = true;
					ogl_set_blending();
					GLint source, destination;
					glGetIntegerv(GL_BLEND_SRC_RGB, &source);
					glGetIntegerv(GL_BLEND_DST_RGB, &destination);
					check(std::array<GLenum, 2>{ GLenum(source), GLenum(destination) } == reference.factors, "Authored wall blend differs from source classification and GL dispatch");
					++cases;
				}
			}
	}
	additive_wall_control = false;
	grd_curcanv->cv_blend_func = previous;
	ogl_set_blending();
	std::cout << "Additive walls: " << cases << " pinned flag/texture-name/stock-option blend controls passed; source face glow disabled\n";
}
#endif
