// Developer-only original-frame ownership check through the real queue flush
extern "C" {
#include "fireball.h"
}
unsigned xl_compat::fireball_queue_raster_controls(const PogBitmap &strip)
{
	if (!ConsoleObject || strip.frames != 5 || !strip.truecolor)
		throw std::runtime_error("Fireball queue proof requires original five-frame strip");
	struct Guard {
		vms_vector position = View_position, scale = Matrix_scale;
		vms_matrix matrix = View_matrix;
		Window window;
		std::vector<int> objects;
		~Guard()
		{
			for (int object : objects) obj_delete(object);
			View_position = position;
			Matrix_scale = scale;
			View_matrix = matrix;
			window.restore();
			xl_transparency_clear();
			discard_sprite_glow(false);
		}
	} guard;
	View_position = {};
	View_matrix = vmd_identity_matrix;
	Matrix_scale = { F1_0, F1_0, F1_0 };
	for (int x : { -26214, 26214 }) {
		vms_vector position = ConsoleObject->pos;
		auto *object = object_create_explosion(ConsoleObject->segnum, &position, F1_0 / 8, VCLIP_AFTERBURNER_BLOB);
		if (!object) throw std::runtime_error("Fireball queue proof could not allocate native objects");
		guard.objects.push_back(int(object - Objects));
		// Native admission uses the real mine; only raster fixture coordinates move
		object->pos = { x, 0, F1_0 / 2 };
	}
	const auto &clip = Vclip[VCLIP_AFTERBURNER_BLOB];
	std::array<int, 5> lifetime{};
	for (int remaining = 1; remaining <= clip.play_time; ++remaining) {
		const int frame = dxx_xl_vclip_lifetime_frame(clip.play_time, clip.frame_time, 5, remaining);
		if (frame >= 0 && frame < 5 && !lifetime[frame]) lifetime[frame] = remaining;
	}
	for (int remaining : lifetime)
		if (!remaining) throw std::runtime_error("Original fireball clock did not expose every frame");
	const auto clear = [] {
		glViewport(0, 0, 64, 64);
		glDisable(GL_SCISSOR_TEST);
		glDepthMask(GL_TRUE);
		glClearDepth(.75);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	};
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	const auto frame_bytes = strip.pixels.size() / strip.frames;
	for (int first = 0; first < 5; ++first) {
		struct Independent {
			grs_bitmap image{};
			std::vector<unsigned char> pixels;
			ParticleQuad quad;
			~Independent()
			{
				ogl_freebmtexture(&image);
			}
		};
		std::array<Independent, 2> reference;
		xl_transparency_begin(nullptr, 0);
		maximum_depth = 4 * F1_0;
		const std::array<int, 2> selected{ first, (first + 3) % 5 };
		for (unsigned i = 0; i < 2; ++i) {
			auto &object = Objects[guard.objects[i]];
			object.lifeleft = lifetime[selected[i]];
			if (!xl_transparency_object(guard.objects[i]) || items.size() != i + 1)
				throw std::runtime_error("Original fireball was not submitted to native queue");
			const auto *sprite = std::get_if<Sprite>(&items.back());
			if (!sprite || sprite->frame != selected[i] || sprite->clip != VCLIP_AFTERBURNER_BLOB)
				throw std::runtime_error("Native queue lost the original fireball frame identity");
			auto &expected = reference[i];
			expected.quad = sprite->quad;
			expected.image = GameBitmaps[sprite->bitmap];
			expected.image.gltexture = expected.image.gltexture_mask = nullptr;
			expected.image.bm_parent = nullptr;
			expected.pixels.assign(strip.pixels.begin() + frame_bytes * selected[i], strip.pixels.begin() + frame_bytes * (selected[i] + 1));
			expected.image.bm_data = expected.pixels.data();
		}
		if (transparency_draw_order(positions, eye, maximum_depth) != std::vector<std::size_t>{ 1, 0 })
			throw std::runtime_error("Original fireball equal-depth queue lost source reverse order");
		// Neither submitted frame may depend on the shared bitmap's last selection
		xl_pog_vclip_bitmap(VCLIP_AFTERBURNER_BLOB, (first + 1) % 5);
		clear();
		xl_transparency_flush();
		std::array<unsigned char, 64 * 64 * 4> actual{}, expected{};
		glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
		if (collecting || !items.empty()) throw std::runtime_error("Fireball queue flush left pending items");
		clear();
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
		for (unsigned i : { 1u, 0u }) draw_fireball_sprite(reference[i].image, reference[i].quad);
		finish_sprite_glow();
		glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, expected.data());
		unsigned visible = 0;
		for (unsigned i = 0; i < actual.size(); ++i) {
			if (i % 4 != 3) visible += expected[i] != 0;
			if (std::abs(int(actual[i]) - int(expected[i])) > 1)
				throw std::runtime_error("Original queued fireball raster differs from independent frames: pair=" + std::to_string(first) + " byte=" + std::to_string(i));
		}
		if (!visible) throw std::runtime_error("Original queued fireball proof rendered no visible pixels");
	}
	return 5;
}
