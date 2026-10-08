// Actual FBO compositor compared with independently bound/clamped sources
static std::vector<unsigned char> merged_wrap_pixels(ogl_texture &texture, int width, int height)
{
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.handle, 0);
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "composite readback framebuffer");
	std::vector<unsigned char> pixels(width * height * 4);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	require(glGetError() == GL_NO_ERROR, "composite readback");
	return pixels;
}
static void merged_wrap_bind(const android_ogl_texture_runtime_state *state, int unit, ogl_texture &texture, int wrap)
{
	android_ogl_active_texture(state ? &state->bind_state : nullptr, GL_TEXTURE0 + unit);
	android_ogl_bind_texture_2d(state ? &state->bind_state : nullptr, texture.handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	texture.wrapstate = wrap;
}
static void test_merged_wrap(nlohmann::json &results, int generation, bool baseline)
{
	const int sizes[][4] = { { 64, 64, 64, 64 }, { 512, 512, 512, 512 }, { 64, 64, 512, 512 }, { 512, 512, 64, 64 }, { 45, 63, 127, 95 } };
	for (int cached = 0; cached < 2; ++cached)
		for (int size = 0; size < 5; ++size)
			for (int filter = 0; filter < 3; ++filter)
				for (int orient = 0; orient < 4; ++orient) {
					int binds = 0, reuse = 0, enabled = -1;
					android_ogl_texture_runtime_state runtime = { { &binds, &reuse }, &enabled };
					const auto *state = cached ? &runtime : nullptr;
					ogl_texture textures[3]{};
					grs_bitmap bitmaps[2]{};
					for (int source = 0; source < 2; ++source) {
						auto &texture = textures[source];
						texture.w = sizes[size][source * 2];
						texture.h = sizes[size][source * 2 + 1];
						texture.tw = texture.w;
						texture.th = texture.h;
						texture.u = texture.v = 1;
						texture.numrend = 1;
						glGenTextures(1, &texture.handle);
						merged_wrap_bind(state, source, texture, GL_REPEAT);
						std::vector<unsigned char> pixels(texture.w * texture.h * 4);
						for (int y = 0; y < texture.h; ++y)
							for (int x = 0; x < texture.w; ++x) {
								int i = (y * texture.w + x) * 4;
								pixels[i] = x == 0 ? 250 : x == texture.w - 1 ? 5
								                                              : 90;
								pixels[i + 1] = y == 0 ? 230 : y == texture.h - 1 ? 10
								                                                  : 70;
								pixels[i + 2] = source ? 160 : 40;
								pixels[i + 3] = source ? ((x + y) % 3 ? 128 : 0) : 255;
							}
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.w, texture.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter == 2 ? GL_LINEAR_MIPMAP_LINEAR : filter ? GL_LINEAR
						                                                                                                     : GL_NEAREST);
						if (filter == 2) glGenerateMipmap(GL_TEXTURE_2D);
						bitmaps[source].gltexture = &texture;
					}
					const int width = (std::max) (textures[0].w, textures[1].w), height = (std::max) (textures[0].h, textures[1].h);
					textures[2].format = textures[2].internalformat = GL_RGBA;
					android_ogl_active_texture(state ? &state->bind_state : nullptr, GL_TEXTURE0);
					android_merged_wall_cached_texmerge_setup_output_texture(&textures[2], width, height, OGL_FLAG_ALPHA, filter, state);
					std::vector<unsigned char> actual, reference;
					bool restored = true;
					for (int pass = 0; pass < 2; ++pass) {
						// The reference applies real GL clamp to each intended object before composition
						merged_wrap_bind(state, 0, textures[0], pass ? GL_CLAMP_TO_EDGE : GL_REPEAT);
						merged_wrap_bind(state, 1, textures[1], pass ? GL_CLAMP_TO_EDGE : GL_REPEAT);
						android_ogl_active_texture(state ? &state->bind_state : nullptr, GL_TEXTURE0);
						glViewport(3, 5, 307, 223);
						glEnable(GL_BLEND);
						glEnable(GL_DEPTH_TEST);
						glEnable(GL_CULL_FACE);
						glDepthMask(GL_TRUE);
						glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
						require(android_merged_wall_cached_texmerge_render_to_texture(&textures[2], &bitmaps[0], &bitmaps[1], orient, width, height, filter, 0, 1, state, nullptr), "actual cached compositor");
						GLint viewport[4], unit, framebuffer;
						GLboolean depth, colors[4];
						glGetIntegerv(GL_VIEWPORT, viewport);
						glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
						glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
						glGetBooleanv(GL_DEPTH_WRITEMASK, &depth);
						glGetBooleanv(GL_COLOR_WRITEMASK, colors);
						require(viewport[0] == 3 && viewport[1] == 5 && viewport[2] == 307 && viewport[3] == 223 && unit == GL_TEXTURE0 && framebuffer == 0, "compositor viewport/unit/framebuffer restoration");
						require(depth && colors[0] && !colors[1] && colors[2] && !colors[3] && glIsEnabled(GL_BLEND) && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE), "compositor mask/enable restoration");
						auto pixels = merged_wrap_pixels(textures[2], width, height);
						if (pass) reference = std::move(pixels);
						else actual = std::move(pixels);
						for (int source = 0; source < 2; ++source) {
							android_ogl_active_texture(state ? &state->bind_state : nullptr, GL_TEXTURE0 + source);
							android_ogl_bind_texture_2d(state ? &state->bind_state : nullptr, textures[source].handle);
							GLint s, t;
							glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &s);
							glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &t);
							restored = restored && s == GL_REPEAT && t == GL_REPEAT && textures[source].wrapstate == GL_REPEAT;
						}
					}
					size_t different = 0, border = 0;
					for (int y = 0; y < height; ++y)
						for (int x = 0; x < width; ++x) {
							int i = (y * width + x) * 4;
							if (std::memcmp(actual.data() + i, reference.data() + i, 4)) {
								++different;
								if (x == 0 || y == 0 || x == width - 1 || y == height - 1) ++border;
							}
						}
					if (!baseline) {
						require(different == 0, "composite equals independently clamped reference");
						require(restored, "source metadata and live wraps restored");
					}
					if (!filter) require(different == 0, "nearest sampling control");
					std::string a, r, error;
					require(input_demo_sha256_hex(actual.data(), actual.size(), &a, &error) && input_demo_sha256_hex(reference.data(), reference.size(), &r, &error), "hash composite RGBA");
					results.push_back({ { "context", generation }, { "cached_binding", cached }, { "size", size }, { "filter", filter }, { "orient", orient }, { "different_pixels", different }, { "different_border_pixels", border }, { "wrap_restored", restored }, { "actual_sha256", a }, { "reference_sha256", r } });
					for (auto &texture : textures) glDeleteTextures(1, &texture.handle);
					android_ogl_active_texture(nullptr, GL_TEXTURE0);
					glViewport(0, 0, 320, 240);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					glDisable(GL_BLEND);
					glDisable(GL_DEPTH_TEST);
					glDisable(GL_CULL_FACE);
					require(glGetError() == GL_NO_ERROR, "merged wrap case cleanup");
				}
}
