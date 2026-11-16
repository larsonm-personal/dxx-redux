#include "scoped_texture_playback.h"
#include "mission_prepare.h"
#include "transparency_scene.h"
#include "../fireball_material.h"
extern "C" {
#include "bm.h"
#include "piggy.h"
}

static nlohmann::ordered_json original_fireball_raster_controls()
{
	using namespace xl_compat;
	if (std::string(Current_mission_filename) != "wshock20" || Current_level_num != 6)
		throw std::runtime_error("Original fireball raster proof requires Warp Shock level 6");
	auto *target = mission_prepare_by_name("wshock20");
	if (!target) throw std::runtime_error("Cannot retain original fireball mission descriptor");
	struct Release {
		mission_preparation **target;
		~Release()
		{
			mission_discard_prepared(target);
		}
	} release{ &target };
	auto retained = XlPreparedResources::prepare_level_inputs(target, 6);
	if (!retained->scoped_textures() || !retained->scoped_texture_playback())
		throw std::runtime_error("Original fireball proof lacks retained scoped textures");
	const auto &clips = retained->scoped_texture_playback()->clips;
	const auto clip = std::find_if(clips.begin(), clips.end(), [](const auto &entry) { return entry.clip == VCLIP_AFTERBURNER_BLOB; });
	if (clip == clips.end()) throw std::runtime_error("Original afterburner strip is absent");
	const auto &image = retained->scoped_textures()->images[clip->image];
	const auto &pixels = image.bitmap;
	if (!pixels.truecolor || pixels.frames != 5 || xl_pog_vclip_frames(clip->clip) != int(pixels.frames))
		throw std::runtime_error("Original fireball strip has unexpected storage or frame count");
	const unsigned width = pixels.width, height = pixels.frame_height();
	const std::size_t frame_bytes = pixels.pixels.size() / pixels.frames;
	glViewport(0, 0, 64, 64);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_SCISSOR_TEST);
	unsigned samples = 0;
	int maximum_error = 0;
	const auto depth = [](double eye) { return (5001. - 10000. / eye) / 4999. * .5 + .5; };
	for (int frame : { 0, 4, 1, 3, 2 }) {
		// Disturb the shared native strip first, then select the requested frame
		xl_pog_vclip_bitmap(clip->clip, (frame + 1) % int(pixels.frames));
		const int bitmap = xl_pog_vclip_bitmap(clip->clip, frame);
		if (bitmap != clip->bitmaps.front()) throw std::runtime_error("Original fireball bitmap binding changed");
		auto &native = GameBitmaps[bitmap];
		if (native.bm_type != BM_RGBA || native.bm_w != int(width) || native.bm_h != int(height) ||
		    std::memcmp(native.bm_data, pixels.pixels.data() + frame_bytes * frame, frame_bytes))
			throw std::runtime_error("Original fireball live frame differs from independent retained pixels");
		const auto material = fireball_material(frame, int(pixels.frames));
		for (float radius : { 0.f, 10.f })
			for (unsigned row = 0; row < 4; ++row)
				for (unsigned column = 0; column < 4; ++column) {
					const unsigned x = (2 * column + 1) * width / 8, y = (2 * row + 1) * height / 8;
					const auto *texel = pixels.pixels.data() + frame_bytes * frame + (y * width + x) * 4;
					ParticleQuad quad{};
					quad.blend = ParticleBlend::additive;
					const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
					for (unsigned i = 0; i < 4; ++i) {
						quad.vertices[i].position = { corners[i][0], corners[i][1], float(2 * depth(radius ? 15 : 8) - 1) };
						quad.vertices[i].uv = { (x + .5f) / width, (y + .5f) / height };
						quad.vertices[i].color = material.color;
					}
					glDepthMask(GL_TRUE);
					glClearDepth(depth(10));
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					struct Scene {
						Scene()
						{
							xl_particle_soft_begin_scene();
						}
						~Scene()
						{
							xl_particle_soft_end_scene();
						}
					} scene;
					draw_bitmap_sprite(native, quad, radius);
					std::array<unsigned char, 4> actual{};
					glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
					for (unsigned c = 0; c < 4; ++c) {
						const double expected = c == 3 ? radius ? 255. : texel[c] * material.color[c] : texel[c] * .5 * (radius ? .5 : 1.);
						const int error = std::abs(int(actual[c]) - int(std::lround(expected)));
						maximum_error = (std::max) (maximum_error, error);
						if (error > 1) throw std::runtime_error("Original fireball raster differs: frame=" + std::to_string(frame) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + " channel=" + std::to_string(c) + " actual=" + std::to_string(actual[c]) + " expected=" + std::to_string(expected));
					}
					++samples;
				}
	}
	xl_pog_vclip_bitmap(clip->clip, 0);
	const auto queued_pairs = fireball_queue_raster_controls(pixels);
	return { { "image", image.path }, { "image_sha256", image.hash }, { "frames", pixels.frames }, { "samples", samples }, { "maximum_channel_error", maximum_error }, { "queued_pairs", queued_pairs }, { "limitation", "Controlled native queue and authored frame raster proof; not campaign or device qualification" } };
}
