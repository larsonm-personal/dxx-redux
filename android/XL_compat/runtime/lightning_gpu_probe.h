#include "sprite_bitmap_gpu_probe.h"
#include "additive_wall_gpu_probe.h"
#include "shield_gpu_probe.h"
#include "camera_target_gpu_probe.h"
#include "camera_native_pass_probe.h"
#include "camera_scene_probe.h"
#include "camera_monitor_probe.h"
#include "shield_original_gpu_probe.h"
// Android renderer control: retains an owned corona without admitting a level
unsigned lightning_line_device_controls()
{
	using namespace xl_compat;
	LightningDrawState restore;
	struct Primitive {
		LightningGpu renderer;
		~Primitive()
		{
			renderer.discard(false);
		}
	} primitive;
	const std::array<float, 16> identity{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	const std::vector<LightningPoint> points{ { -32256, 512, 0 }, { 33280, 512, 0 }, { 40000, 512, 0 } };
	const auto core = lightning_core_geometry(points, [](const LightningVertex &p) { return p[2]; });
	const LightningGlow glow{};
	const std::array<float, 4> color{ 0, .25f, .5f, .75f };
	int subpixel_bits = 0;
	glGetIntegerv(GL_SUBPIXEL_BITS, &subpixel_bits);
	const double grid = std::ldexp(1.0, -subpixel_bits);
	require(subpixel_bits >= 4 && grid > 0 && std::isfinite(grid), "Invalid native raster subpixel precision");
	unsigned cases = 0;
	for (float width : { 1.5f, 2.53125f, 3.f, 5.0625f, 10.f, 20.f, 40.f })
		for (bool occluded : { false, true })
			for (bool scissored : { false, true }) {
				glViewport(0, 0, 64, 64);
				glDisable(GL_SCISSOR_TEST);
				glDepthMask(GL_TRUE);
				glClearDepthf(occluded ? .25f : 1.f);
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glScissor(9, 12, 35, 30);
				if (scissored) glEnable(GL_SCISSOR_TEST);
				primitive.renderer.draw(core, glow, 1, color, width, identity, 0, false);
				std::array<int, 4> view{}, box{};
				glGetIntegerv(GL_VIEWPORT, view.data());
				glGetIntegerv(GL_SCISSOR_BOX, box.data());
				require(view == std::array<int, 4>{ 0, 0, 64, 64 } && box == std::array<int, 4>{ 9, 12, 35, 30 } && bool(glIsEnabled(GL_SCISSOR_TEST)) == scissored, "Wide lightning device control lost viewport/scissor state");
				std::array<unsigned char, 64 * 64 * 4> pixels{};
				glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
				for (unsigned y = 0; y < 64; ++y) {
					// X bounds are exact on every ES grid; Y edges can round to adjacent grid positions
					const double half = double(width) * .5 + .5, py = double(y) + .5;
					const double bottom = 32.25 - half, top = 32.25 + half;
					double minimum = 1, maximum = 0;
					bool possible = false, certain = true;
					for (double lower : { std::floor(bottom / grid) * grid, std::ceil(bottom / grid) * grid })
						for (double upper : { std::floor(top / grid) * grid, std::ceil(top / grid) * grid }) {
							const bool inside = py >= lower && py <= upper;
							possible |= inside;
							certain &= py > lower && py < upper;
							const double distance = -half + (py - lower) * (2 * half) / (upper - lower);
							const double coverage = inside ? (std::clamp) (half - std::abs(distance), 0.0, 1.0) : 0;
							minimum = (std::min) (minimum, coverage);
							maximum = (std::max) (maximum, coverage);
						}
					for (unsigned x = 0; x < 64; ++x) {
						const float px = float(x) + .5f;
						const float along = (std::min) (px - 15.75f, 48.75f - px);
						const bool visible = !occluded && along > 0 && possible && (!scissored || (x >= 9 && x < 44 && y >= 12 && y < 42));
						const auto *pixel = pixels.data() + (y * 64 + x) * 4;
						bool colored = true, blank = true;
						for (unsigned c = 0; c < 3; ++c) {
							colored &= std::abs(int(pixel[c]) - int(std::lround(255.f * color[c]))) <= 1;
							blank &= pixel[c] <= 1;
						}
						const double alpha = 255.0 * color[3] * (std::clamp) (double(along), 0.0, 1.0);
						const int lower_alpha = visible && colored ? int(std::floor(alpha * minimum)) : 0;
						const int upper_alpha = visible && colored ? int(std::ceil(alpha * maximum)) : 0;
						if (!(visible ? colored || (blank && !certain) : blank) || int(pixel[3]) < lower_alpha - 1 || int(pixel[3]) > upper_alpha + 1)
							throw std::runtime_error("Wide lightning device pixels differ: width=" + std::to_string(width) + " occluded=" + std::to_string(occluded) + " scissored=" + std::to_string(scissored) + " x=" + std::to_string(x) + " y=" + std::to_string(y) + " alpha_range=" + std::to_string(lower_alpha) + "," + std::to_string(upper_alpha) + " actual_alpha=" + std::to_string(pixel[3]) + " subpixel_bits=" + std::to_string(subpixel_bits));
					}
				}
				require(glGetError() == GL_NO_ERROR, "Wide lightning device control raised a GPU error");
				++cases;
			}
	return cases;
}
std::shared_ptr<const xl_compat::ParticleImageResource> lightning_probe_image;
std::shared_ptr<const xl_compat::ParticleImageResource> lightning_probe_glare;
std::array<std::shared_ptr<const xl_compat::ShieldImageResource>, 3> lightning_probe_shields;
std::array<std::unique_ptr<xl_compat::ShieldAddon>, 3> lightning_probe_shield_addons;
uint32_t lightning_probe_shield_ticks = 10000;
std::vector<unsigned char> lightning_probe_pixels;
void lightning_gpu_probe(const char *operation)
{
	using namespace xl_compat;
	require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI) && game_is_time_paused(), "Lightning GPU control requires paused singleplayer");
	if (!std::strcmp(operation, "capture")) {
		Inventory request;
		request.segments.resize(1);
		Object source{};
		source.type = 20;
		source.id = source.reader_id = 1;
		source.render = source.reader_render = 12;
		source.render_data = { 300, 50, 18, 5, 1, -1, 3, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, 1 };
		request.objects.push_back(source);
		lightning_probe_image = prepare_lightning_image(request);
		lightning_probe_glare = prepare_powerup_glare_image();
		for (unsigned i = 0; i < lightning_probe_shields.size(); ++i) {
			lightning_probe_shields[i] = prepare_shield_image(i);
			lightning_probe_shield_addons[i] = std::make_unique<ShieldAddon>(i);
			lightning_probe_shield_addons[i]->replace_image(lightning_probe_shields[i]);
		}
		lightning_probe_shield_ticks = 10000;
		require(lightning_probe_glare && lightning_probe_glare->texture, "Lightning control did not prepare its owned powerup glare");
		require(lightning_probe_image && lightning_probe_image->texture, "Lightning control did not prepare its owned corona");
		lightning_probe_pixels.clear();
		return;
	}
	if (!std::strcmp(operation, "release")) {
		discard_lightning_context(false);
		discard_shield_context(false);
		lightning_probe_image.reset();
		lightning_probe_glare.reset();
		for (auto &addon : lightning_probe_shield_addons) addon.reset();
		for (auto &image : lightning_probe_shields) image.reset();
		lightning_probe_pixels.clear();
		return;
	}
	require(!std::strcmp(operation, "gpu") && lightning_probe_image && lightning_probe_glare && gl_initialized, "Lightning GPU control lacks its original corona/glare");
	require(glGetError() == GL_NO_ERROR, "Lightning GPU control requires a clean context");
	const auto started = std::chrono::steady_clock::now();
	const auto progress = [&](const char *stage) {
		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
		debug_log_force(DLOG_PROFILING, "XL GPU control %s after %lld ms", stage, static_cast<long long>(milliseconds));
	};
	progress("start");
	const unsigned camera_scene_cases = camera_scene_device_controls();
	progress("camera scene");
	const unsigned camera_monitor_cases = camera_monitor_device_controls();
	progress("camera monitor");
	std::vector<unsigned char> pixels(64 * 64 * 4);
	unsigned changed = 0;
	unsigned line_cases = 0, bitmap_cases = 0, outline_cases = 0, wall_cases = 0, shield_cases = 0, original_shield_cases = 0, camera_target_cases = 0, camera_native_pass_cases = 0;
	std::vector<unsigned char> shield_pixels;
	unsigned shield_overlap_rounding = 0;
	GLfloat line_range[2];
	glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, line_range);
	{
		LightningGlowState restore;
		Readback readback;
		ParticleProbeTarget target;
		glBindRenderbuffer(GL_RENDERBUFFER, target.color);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 64, 64);
		struct NativeState {
			vms_vector eye = View_position;
			vms_matrix view = View_matrix;
			grs_screen *screen = grd_curscreen;
			grs_screen probe = *screen;
			object *viewer = Viewer;
			object probe_viewer{};
			grs_canvas *canvas = grd_curcanv;
			grs_canvas probe_canvas = *canvas;
			GLuint depth = 0;
			unsigned random_state = 0, random_calls = d_rand_get_stream_call_count(D_RNG_PRESENTATION);
			NativeState()
			{
				require(d_rand_get_stream_state(D_RNG_PRESENTATION, &random_state) != 0, "Lightning control cannot preserve effects RNG");
				probe.sc_w = probe.sc_h = 64;
				grd_curscreen = &probe;
				probe_viewer.size = 65536;
				Viewer = &probe_viewer;
				probe_canvas.cv_bitmap.bm_w = 640;
				grd_curcanv = &probe_canvas;
				View_position = {};
				View_matrix = { { 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } };
				glGenRenderbuffers(1, &depth);
				glBindRenderbuffer(GL_RENDERBUFFER, depth);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
				d_srand_stream(D_RNG_PRESENTATION, 12345);
			}
			~NativeState()
			{
				View_position = eye;
				View_matrix = view;
				grd_curscreen = screen;
				Viewer = viewer;
				grd_curcanv = canvas;
				d_rand_set_stream_state(D_RNG_PRESENTATION, random_state);
				d_rand_set_stream_call_count(D_RNG_PRESENTATION, random_calls);
				glDeleteRenderbuffers(1, &depth);
			}
		} native;
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Lightning GPU control target is incomplete");
		line_cases = lightning_line_device_controls();
		progress("lines");
		bitmap_cases = sprite_bitmap_device_controls();
		progress("bitmaps");
		wall_cases = additive_wall_device_controls();
		progress("walls");
		camera_target_cases = camera_target_device_controls(shield_pixels);
		progress("camera target");
		camera_native_pass_cases = camera_native_pass_controls(shield_pixels);
		progress("camera native pass");
		shield_cases = shield_device_controls(shield_pixels, &shield_overlap_rounding);
		progress("analytical shields");
		original_shield_cases = shield_original_device_controls(lightning_probe_shields, lightning_probe_shield_addons, lightning_probe_shield_ticks, shield_pixels);
		progress("original shields");
		lightning_probe_shield_ticks += 1000;
		glViewport(3, 5, 48, 43);
		const GLfloat projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -5001.f / 4999.f, -1, 0, 0, -10000.f / 4999.f, 0 };
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(projection);
		glMatrixMode(GL_MODELVIEW);
		glDepthMask(GL_TRUE);
		glClearDepthf(1);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		const std::vector<LightningPoint> nodes{ { -32768, -16384, 196608 }, { 0, 19661, 196608 }, { 32768, -6554, 196608 }, { 40000, 10000, 196608 } };
		LightningSceneItem item{};
		item.frame.core = lightning_core_geometry(nodes, [](const LightningVertex &p) { return p[2]; });
		item.frame.glow = lightning_glow_geometry(nodes, 1, .7f, true, { 0, 0, 0 }, 3, 4999);
		item.random_nodes = 1;
		item.width = .7f;
		item.color = { 0, 52.f / 255.f, 149.f / 255.f, 128.f / 255.f };
		const auto calls = d_rand_get_stream_call_count(D_RNG_PRESENTATION);
		draw_lightning_scene_item(item, lightning_probe_image->texture.get());
		ParticleQuad quad{};
		quad.blend = ParticleBlend::additive;
		const std::array<std::array<float, 2>, 4> corners{ { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } } };
		for (unsigned i = 0; i < 4; ++i) {
			quad.vertices[i].position = { corners[i][0] * .6f, corners[i][1] * .4f, -2.5f };
			quad.vertices[i].uv = { (corners[i][0] + 1) * .5f, (1 - corners[i][1]) * .5f };
			quad.vertices[i].color = { .2f, .15f, .3f, .5f };
		}
		xl_particle_soft_begin_scene();
		lightning_probe_glare->texture->draw_soft_sprite(quad);
		xl_particle_soft_end_scene();
		std::vector<unsigned char> sprite_scene(pixels.size()), pending(pixels.size());
		glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, sprite_scene.data());
		require(std::any_of(sprite_scene.begin(), sprite_scene.end(), [](unsigned char value) { return value != 0; }), "Original glare did not draw into the scene");
		draw_lightning_scene_item(item, lightning_probe_image->texture.get());
		glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pending.data());
		require(sprite_scene == pending, "Original glare prematurely flushed pending lightning");
		finish_lightning_batch();
		require(d_rand_get_stream_call_count(D_RNG_PRESENTATION) == calls + 6, "Mixed lightning/sprite draw used the wrong effects RNG count");
		GLint framebuffer, view[4];
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
		glGetIntegerv(GL_VIEWPORT, view);
		require(GLuint(framebuffer) == readback.framebuffer && view[0] == 3 && view[1] == 5 && view[2] == 48 && view[3] == 43, "Lightning draw lost caller target or viewport");
		glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		require(pixels != sprite_scene, "Pending lightning did not composite over original glare");
		require(glGetError() == GL_NO_ERROR, "Lightning draw/readback raised a GPU error");
		for (unsigned p = 0; p < 64 * 64; ++p) changed += pixels[p * 4 + 2] != 0;
		require(changed > 0 && changed < 64 * 64, "Lightning GPU control produced no bounded visible glow");
		item.frame.glow.vertices.clear();
		item.frame.glow.uv.clear();
		std::vector<unsigned char> narrow_outline;
		for (int canvas_width : { 640, 4096 }) {
			std::vector<unsigned char> scaled_outline;
			for (float scale : { 0.f, 1.f }) {
				native.probe_canvas.cv_bitmap.bm_w = canvas_width;
				item.frame.glow.distance_scale = scale;
				glDepthMask(GL_TRUE);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				d_srand_stream(D_RNG_PRESENTATION, 12345);
				const auto outline_calls = d_rand_get_stream_call_count(D_RNG_PRESENTATION);
				draw_lightning_scene_item(item, nullptr);
				std::vector<unsigned char> immediate(64 * 64 * 4), finished(immediate.size());
				glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, immediate.data());
				finish_lightning_batch();
				glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, finished.data());
				require(immediate == finished, "Non-glowing lightning deferred its direct device draw");
				require(d_rand_get_stream_call_count(D_RNG_PRESENTATION) == outline_calls + 3, "Non-glowing lightning consumed the wrong effects RNG count");
				unsigned visible = 0;
				for (unsigned p = 0; p < 64 * 64; ++p) visible += finished[p * 4 + 2] != 0;
				require(visible > 0 && visible < 64 * 64, "Non-glowing lightning produced no bounded device outline");
				if (!scaled_outline.empty()) require(scaled_outline == finished, "Non-glowing device width depended on glow distance scale");
				scaled_outline = finished;
				pixels.insert(pixels.end(), finished.begin(), finished.end());
				++outline_cases;
			}
			if (!narrow_outline.empty()) require(narrow_outline != scaled_outline, "Non-glowing device width ignored canvas size");
			narrow_outline = std::move(scaled_outline);
		}
	}
	require(glGetError() == GL_NO_ERROR, "Lightning GPU control restoration raised a GPU error");
	pixels.insert(pixels.end(), shield_pixels.begin(), shield_pixels.end());
	if (!lightning_probe_pixels.empty()) require(pixels == lightning_probe_pixels, "Lightning/shield pixels changed across graphics context recreation");
	lightning_probe_pixels = std::move(pixels);
	progress("complete");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "shield_cases", shield_cases }, { "original_shield_cases", original_shield_cases }, { "camera_target_cases", camera_target_cases }, { "camera_native_pass_cases", camera_native_pass_cases }, { "camera_scene_cases", camera_scene_cases }, { "camera_monitor_cases", camera_monitor_cases }, { "shield_overlap_rounding", shield_overlap_rounding }, { "wall_cases", wall_cases }, { "outline_cases", outline_cases }, { "line_cases", line_cases }, { "sprite_cases", 1 }, { "bitmap_cases", bitmap_cases }, { "glare_width", lightning_probe_glare->bitmap.width }, { "lightning_changed", changed }, { "line_width_min", line_range[0] }, { "line_width_max", line_range[1] }, { "corona_width", lightning_probe_image->bitmap.width } }.dump();
}
