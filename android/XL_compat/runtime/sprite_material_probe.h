#include "particle_renderer.h"
#include "../particle_render.h"
#include "original_fireball_raster_probe.h"
#include "sprite_glow_probe.h"
namespace sprite_material_control
{
#include "sprite_bitmap_gpu_probe.h"
}
static int xl_sprite_material_probe(const char *path)
{
	nlohmann::ordered_json proof = { { "schema", "xl-sprite-material-v1" }, { "complete", bool{ false } } };
	try {
		if (!gl_initialized || !Game_wind) throw std::runtime_error("Sprite material controls require a rendered game");
		proof["renderer"] = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
		struct Target {
			GLint framebuffer = 0, renderbuffer = 0, matrix_mode = 0;
			GLuint target = 0, color = 0, depth = 0;
			Target()
			{
				glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
				glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
				glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
				glPushAttrib(GL_ALL_ATTRIB_BITS);
				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glGenFramebuffers(1, &target);
				glGenTextures(1, &color);
				glGenRenderbuffers(1, &depth);
				glBindFramebuffer(GL_FRAMEBUFFER, target);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, color);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
				glBindRenderbuffer(GL_RENDERBUFFER, depth);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
			}
			~Target()
			{
				glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
				glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
				glDeleteFramebuffers(1, &target);
				glDeleteRenderbuffers(1, &depth);
				glDeleteTextures(1, &color);
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glMatrixMode(matrix_mode);
				glPopAttrib();
			}
		};
		{
			Target target;
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Sprite material target is incomplete");
			glDisable(GL_DITHER);
			proof["cases"] = sprite_material_control::sprite_bitmap_device_controls();
			if (proof["cases"] != 216) throw std::runtime_error("Sprite material controls are incomplete");
			proof["soft_depth_cases"] = sprite_material_control::sprite_bitmap_soft_controls();
			if (proof["soft_depth_cases"] != 86) throw std::runtime_error("Sprite soft-depth controls are incomplete");
			proof["original_fireball"] = original_fireball_raster_controls();
			proof["glow_profile_cases"] = sprite_glow_control::controls();
			if (proof["glow_profile_cases"] != 12) throw std::runtime_error("Sprite glow profile controls are incomplete");
			proof["glowing_fireballs"] = sprite_glow_control::runtime_controls();
			proof["glow_bounds_cases"] = sprite_glow_control::bounds_controls();
		}
		if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Sprite material controls left a GPU error");
		proof["complete"] = bool{ true };
	} catch (const std::exception &error) {
		proof["error"] = error.what();
	}
	std::ofstream output(path);
	output << proof.dump(2) << '\n';
	output.flush();
	return output.good() && proof["complete"].get<bool>() ? 0 : 1;
}
