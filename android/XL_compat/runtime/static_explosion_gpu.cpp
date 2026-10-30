// Android diagnostic: actual object dispatch, bitmap upload and sprite shader readback
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "vclip.h"
#include "piggy.h"
#include "playsave.h"
#include "ogl_init.h"
#include "../../../d2/3d/globvars.h"
}
namespace
{
void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
void enable(GLenum cap, GLboolean active)
{
	if (active) glEnable(cap);
	else glDisable(cap);
}
struct State {
	GLint read, draw, renderbuffer, viewport[4], program, vao, buffer, pack, alignment, row, skip_rows, skip_pixels, active, texture;
	GLint src, dst, src_alpha, dst_alpha;
	GLfloat clear[4];
	GLboolean depth, blend, cull, scissor, mask[4];
	GLuint fbo = 0, color = 0;
	int external = gles3_shim_external_active();
	int fade = grd_curcanv->cv_fade_level, blend_mode = grd_curcanv->cv_blend_func;
	int effects = PlayerCfg.AlphaEffects;
	object saved_object = Objects[49];
	vclip saved_clip = Vclip[0];
	vms_vector position = View_position, scale = Matrix_scale;
	vms_matrix matrix = View_matrix;
	std::array<int, 2> slots{ Num_bitmap_files - 2, Num_bitmap_files - 1 };
	std::array<grs_bitmap, 2> bitmaps{ GameBitmaps[slots[0]], GameBitmaps[slots[1]] };
	State()
	{
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_CURRENT_PROGRAM, &program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
		glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
		glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
		glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
		glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
		glGetIntegerv(GL_BLEND_SRC_RGB, &src);
		glGetIntegerv(GL_BLEND_DST_RGB, &dst);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		glGetBooleanv(GL_COLOR_WRITEMASK, mask);
		depth = glIsEnabled(GL_DEPTH_TEST);
		blend = glIsEnabled(GL_BLEND);
		cull = glIsEnabled(GL_CULL_FACE);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrthof(-1, 1, -1, 1, 0, 4);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		for (int slot : slots) GameBitmaps[slot] = {};
	}
	~State()
	{
		for (unsigned i = 0; i < slots.size(); ++i) {
			ogl_freebmtexture(&GameBitmaps[slots[i]]);
			GameBitmaps[slots[i]] = bitmaps[i];
		}
		Objects[49] = saved_object;
		Vclip[0] = saved_clip;
		View_position = position;
		View_matrix = matrix;
		Matrix_scale = scale;
		PlayerCfg.AlphaEffects = effects;
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glDeleteFramebuffers(1, &fbo);
		glDeleteRenderbuffers(1, &color);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
		glPixelStorei(GL_PACK_ROW_LENGTH, row);
		glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
		glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glActiveTexture(active);
		gr_settransblend(fade, static_cast<ubyte>(blend_mode));
		glBlendFuncSeparate(src, dst, src_alpha, dst_alpha);
		glClearColor(clear[0], clear[1], clear[2], clear[3]);
		glColorMask(mask[0], mask[1], mask[2], mask[3]);
		enable(GL_DEPTH_TEST, depth);
		enable(GL_BLEND, blend);
		enable(GL_CULL_FACE, cull);
		enable(GL_SCISSOR_TEST, scissor);
		gles3_shim_use_external(external ? GLuint(program) : 0);
		gles3_shim_bind_program(GLuint(program));
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}
};
} // namespace
unsigned xl_static_explosion_gpu_check()
{
	check(gl_initialized && grd_curcanv && Num_bitmap_files > 2 && Objects[49].type == 19 && glGetError() == GL_NO_ERROR,
	      "Static sprite GPU control requires a loaded fixture and clean current context");
	const std::array<std::array<unsigned char, 4>, 2> colors{ { { 50, 80, 100, 128 }, { 110, 40, 70, 255 } } };
	unsigned cases = 0;
	{
		State state;
		std::array<std::vector<unsigned char>, 2> data;
		for (unsigned frame = 0; frame < 2; ++frame) {
			auto &bitmap = GameBitmaps[state.slots[frame]];
			bitmap.bm_w = frame ? 2 : 4;
			bitmap.bm_h = frame ? 4 : 2;
			bitmap.bm_rowsize = bitmap.bm_w * 4;
			bitmap.bm_type = BM_RGBA;
			data[frame].resize(32);
			for (unsigned pixel = 0; pixel < 8; ++pixel) std::copy(colors[frame].begin(), colors[frame].end(), data[frame].begin() + pixel * 4);
			bitmap.bm_data = data[frame].data();
			Vclip[0].frames[frame].index = static_cast<ushort>(state.slots[frame]);
		}
		Vclip[0].num_frames = 2;
		glGenFramebuffers(1, &state.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, state.fbo);
		glGenRenderbuffers(1, &state.color);
		glBindRenderbuffer(GL_RENDERBUFFER, state.color);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 32, 32);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, state.color);
		check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Static sprite framebuffer incomplete");
		glViewport(0, 0, 32, 32);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_SCISSOR_TEST);
		glEnable(GL_BLEND);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
		View_position = {};
		View_matrix = vmd_identity_matrix;
		Matrix_scale = { F1_0, F1_0, F1_0 };
		auto &object = Objects[49];
		object.pos = { 0, 0, F1_0 };
		object.size = F1_0 / 2;
		object.rtype.vclip_info.vclip_num = 0;
		for (int effects : { 0, 1 })
			for (int frame : { 0, 1 }) {
				PlayerCfg.AlphaEffects = effects;
				object.rtype.vclip_info.framenum = static_cast<sbyte>(frame);
				glClearColor(17.f / 255, 29.f / 255, 43.f / 255, 1);
				glClear(GL_COLOR_BUFFER_BIT);
				render_object(&object);
				std::array<unsigned char, 32 * 32 * 4> pixels{};
				glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				check(glGetError() == GL_NO_ERROR, "Static sprite draw/readback raised GL errors");
				for (int y = 0; y < 32; ++y)
					for (int x = 0; x < 32; ++x) {
						const bool inside = frame ? (x >= 12 && x < 20 && y >= 8 && y < 24) : (x >= 8 && x < 24 && y >= 12 && y < 20);
						for (int channel = 0; channel < 3; ++channel) {
							const int background = channel == 0 ? 17 : channel == 1 ? 29
							                                                        : 43;
							const float alpha = colors[frame][3] / 255.f;
							const float expected = !inside ? float(background) : effects ? float(background + colors[frame][channel])
							                                                             : colors[frame][channel] * alpha + background * (1 - alpha);
							const int actual = pixels[(y * 32 + x) * 4 + channel];
							if (std::abs(actual - expected) > 1.1f)
								throw std::runtime_error("Static sprite pixels differ: profile=" + std::to_string(effects) + " frame=" + std::to_string(frame) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + " channel=" + std::to_string(channel) + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
						}
					}
				++cases;
			}
	}
	check(glGetError() == GL_NO_ERROR, "Static sprite restoration raised GL errors");
	return cases;
}
