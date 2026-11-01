#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "shield_renderer.h"
#include "particle_resources.h"
#include "../camera_hit_render.h"
#include "../shield_sphere.h"
#include "../pog.h"
extern "C" {
#include "3d.h"
#include "ogl_init.h"
#include "internal.h"
}
#undef min
#undef max
#if !defined(OGLES) || defined(ANDROID)
#include "shield_gpu.h"
#include "particle_border_texture.h"
namespace
{
xl_compat::ShieldGpu shield_gpu;
}
#endif
namespace xl_compat
{
struct ShieldTexture::State {
	static State *head;
	State *previous = nullptr, *next = nullptr;
	const PogBitmap &pixels;
	GLuint border = 0;
	unsigned uploaded_frame = 0;
	explicit State(const PogBitmap &image) : pixels(image)
	{
		if (!image.truecolor || !image.width || (image.width & (image.width - 1)) || image.width > 4096 ||
		    !image.frames || image.frames > 128 || image.height != image.width * image.frames ||
		    image.pixels.size() != size_t(image.width) * image.height * 4)
			throw std::runtime_error("XL shield requires a bounded RGBA strip of square power-of-two frames");
		next = head;
		if (next) next->previous = this;
		head = this;
	}
	void release(bool abandoned)
	{
		if (border && !abandoned) glDeleteTextures(1, &border);
		border = 0;
	}
	~State()
	{
		release(false);
		if (previous) previous->next = next;
		else head = next;
		if (next) next->previous = previous;
	}
};
ShieldTexture::State *ShieldTexture::State::head = nullptr;
ShieldTexture::ShieldTexture(const PogBitmap &pixels) : state(std::make_unique<State>(pixels)) {}
ShieldTexture::~ShieldTexture() = default;
void ShieldTexture::discard_context(bool abandoned)
{
	for (auto *texture = State::head; texture; texture = texture->next) texture->release(abandoned);
}
unsigned ShieldTexture::handle(unsigned frame, ShieldFilter filter)
{
#if defined(OGLES) && !defined(ANDROID)
	throw std::runtime_error("XL shield textures require OpenGL 3 or OpenGL ES 3");
#else
	if (frame >= state->pixels.frames) throw std::runtime_error("XL shield frame is out of range");
	if (filter != ShieldFilter::nearest && filter != ShieldFilter::linear && filter != ShieldFilter::mip_nearest && filter != ShieldFilter::trilinear)
		throw std::runtime_error("Unsupported XL shield texture filter");
	struct Restore {
		GLint active, binding, unpack, alignment, row, rows, pixels;
		Restore()
		{
			glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
			glActiveTexture(GL_TEXTURE0);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
			glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
			glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
			glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
			glGetIntegerv(GL_UNPACK_SKIP_ROWS, &rows);
			glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &pixels);
		}
		~Restore()
		{
			glBindTexture(GL_TEXTURE_2D, binding);
			glActiveTexture(active);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
			glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
			glPixelStorei(GL_UNPACK_SKIP_ROWS, rows);
			glPixelStorei(GL_UNPACK_SKIP_PIXELS, pixels);
		}
	} restore;
	const GLint min_filter = filter == ShieldFilter::nearest ? GL_NEAREST : filter == ShieldFilter::linear    ? GL_LINEAR
	                                                                    : filter == ShieldFilter::mip_nearest ? GL_LINEAR_MIPMAP_NEAREST
	                                                                                                          : GL_LINEAR_MIPMAP_LINEAR;
	const GLint mag_filter = filter == ShieldFilter::nearest ? GL_NEAREST : GL_LINEAR;
	if (!state->border || state->uploaded_frame != frame) {
		GLint maximum;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
		const unsigned side = state->pixels.width;
		if (maximum < 4 || side > unsigned(maximum / 4)) throw std::runtime_error("XL shield frame exceeds GPU border storage");
		GLuint source = 0, border = 0;
		try {
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
			glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
			glGenTextures(1, &source);
			glBindTexture(GL_TEXTURE_2D, source);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			             state->pixels.pixels.data() + size_t(frame) * side * side * 4);
			glGenerateMipmap(GL_TEXTURE_2D);
			border = copy_particle_border_texture(source, side, GL_RGBA32F);
			glDeleteTextures(1, &source);
		} catch (...) {
			if (source) glDeleteTextures(1, &source);
			if (border) glDeleteTextures(1, &border);
			throw;
		}
		state->release(false);
		state->border = border;
		state->uploaded_frame = frame;
	}
	glBindTexture(GL_TEXTURE_2D, state->border);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	return state->border;
#endif
}
ShieldAddon::ShieldAddon(unsigned addon) : index(addon)
{
	if (addon > 2) throw std::runtime_error("Unsupported XL shield addon");
}
void ShieldAddon::replace_image(std::shared_ptr<const ShieldImageResource> replacement)
{
	if (image == replacement) return;
	image = std::move(replacement);
	frame = 0;
}
void ShieldAddon::draw(uint32_t ticks, int quality, const CameraHitHardware &hit, const std::array<float, 3> &color,
                       const std::array<float, 16> &matrix, ShieldFilter filter)
{
	// The qualified hit branch returns before InitSurface when NeedEffect is false
	if (!hit.envelope.active) return;
	if (!image || !image->texture) throw std::runtime_error("XL hit shield lacks its owned frame resource");
	// CSphere::Animate recognizes only the first two addon parents
	if (index < 2 && clock.advance(ticks, true, true, 10)) frame = addon_next_frame(frame, image->bitmap.frames);
	const auto texture = image->texture->handle(frame, filter);
	draw_hit_shield(quality, hit, { color[0], color[1], color[2], 1 }, matrix, texture, int(image->bitmap.width), filter == ShieldFilter::nearest);
}
} // namespace xl_compat
void xl_compat::discard_shield_context(bool abandoned)
{
	ShieldTexture::discard_context(abandoned);
#if !defined(OGLES) || defined(ANDROID)
	shield_gpu.discard(abandoned);
#endif
}
void xl_compat::draw_hit_shield(int quality, const CameraHitHardware &hit, const std::array<float, 4> &color,
                                const std::array<float, 16> &matrix, unsigned texture, int border_side, bool nearest)
{
#if defined(OGLES) && !defined(ANDROID)
	throw std::runtime_error("XL shields require OpenGL 3 or OpenGL ES 3");
#else
	shield_gpu.draw(quality, hit, color, matrix, texture, border_side, nearest);
#endif
}
