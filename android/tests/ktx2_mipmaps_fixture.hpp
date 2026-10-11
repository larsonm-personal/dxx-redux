// Actual ETC2 upload, LOD sampling, filtering and device-limit fallback
#pragma push_macro("glDrawArrays")
#undef glDrawArrays
static GLuint ktx_shader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	require(ok, "compile explicit LOD shader");
	return shader;
}

static std::vector<unsigned char> ktx_sample(GLuint program, GLuint texture, int lod)
{
	glViewport(0, 0, 320, 240);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glUseProgram(program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(glGetUniformLocation(program, "tex"), 0);
	glUniform1f(glGetUniformLocation(program, "lod"), (float) lod);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	std::vector<unsigned char> pixel(4);
	glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
	require(glGetError() == GL_NO_ERROR, "sample authored LOD without GL errors");
	return pixel;
}

static void test_ktx2_mipmaps(nlohmann::json &results, int generation)
{
	// Exercise the actual decoder with the oversized entry that blocked D2 128
	require(PHYSFS_mount("d2-hires-128-textures-ktx2.dxa", nullptr, 0), "mount real release candidate");
	clear_texture_lookup_cache();
	etc2_file_data real{};
	require(read_ktx2_file("textures/d2/cockpitbx2.ktx2", &real), "decode real 4096px pack entry");
	require(real.width == 4096 && real.mip_count > 1, "retain source dimensions and authored levels");
	ogl_texture real_texture{};
	require(android_ogl_upload_ktx2(&real_texture, &real, 2, 2048, "cockpitbx2"), "adapt actual release texture to 2048px device");
	require(real_texture.w == 2048 && real_texture.mip_base_level == 1 && real_texture.authored_mip_count == real.mip_count - 1, "real pack mip selection");
	glDeleteTextures(1, &real_texture.handle);
	std::free(real.filedata);
	require(PHYSFS_unmount("d2-hires-128-textures-ktx2.dxa"), "unmount release candidate");
	// The uncompressed path resizes before entering the legacy CPU buffer
	std::vector<unsigned char> tga(18 + 4096 * 32 * 3, 180);
	std::fill(tga.begin(), tga.begin() + 18, 0);
	tga[2] = 2;
	tga[13] = 16;
	tga[14] = 32;
	tga[16] = 24;
	tga[17] = 32;
	FILE *image = std::fopen("large.tga", "wb");
	require(image && std::fwrite(tga.data(), 1, tga.size(), image) == tga.size(), "write large decoded fixture");
	std::fclose(image);
	clear_texture_lookup_cache();
	const int previous_limit = ogl_max_texture_size;
	ogl_max_texture_size = 2048;
	png_data decoded{};
	require(read_png("large.tga", &decoded) && decoded.width == 2048 && decoded.height == 16 && decoded.data[0] == 180, "bounded decoded image resize preserves pixels");
	std::free(decoded.data);
	ogl_max_texture_size = previous_limit;
	std::remove("large.tga");
	clear_texture_lookup_cache();
	GLuint vertex = ktx_shader(GL_VERTEX_SHADER, "#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}");
	GLuint fragment = ktx_shader(GL_FRAGMENT_SHADER, "#version 300 es\nprecision highp float; uniform sampler2D tex; uniform float lod; out vec4 color; void main(){color=textureLod(tex,vec2(0.5),lod);}");
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	require(linked, "link explicit LOD shader");
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	for (int alpha = 0; alpha < 2; ++alpha)
		for (int count : { 1, 3 })
			for (int cap : { 0, 8 })
				for (int filter = 0; filter < 3; ++filter) {
					std::vector<unsigned char> bytes;
					std::vector<std::vector<unsigned char>> raw;
					for (int level = 0, size = 16; level < count; ++level, size /= 2) {
						std::vector<unsigned char> block(alpha ? 16 : 8, 0);
						if (alpha) block[0] = 255;
						const int color = alpha ? 8 : 0;
						block[color] = (unsigned char) (0x22 + 0x44 * level);
						block[color + 1] = 0x44;
						block[color + 2] = 0x66;
						std::vector<unsigned char> image;
						for (int n = 0; n < ((size + 3) / 4) * ((size + 3) / 4); ++n) image.insert(image.end(), block.begin(), block.end());
						for (int b = 0; b < 4; ++b) bytes.push_back((unsigned char) (image.size() >> (8 * b)));
						bytes.insert(bytes.end(), image.begin(), image.end());
						raw.push_back(image);
					}
					etc2_file_data data{};
					data.width = data.height = data.orig_width = data.orig_height = 16;
					data.format = (unsigned char) alpha;
					data.mip_count = (unsigned char) count;
					data.filedata = bytes.data();
					data.filedata_size = (unsigned int) bytes.size();
					ogl_texture texture{};
					const bool expected = count > 1 || cap == 0;
					require((android_ogl_upload_ktx2(&texture, &data, filter, cap, "fixture") != 0) == expected, "device-limit admission uses supplied mip levels");
					if (!expected) {
						require(!texture.handle, "unsupported single level allocates no texture");
						results.push_back({ { "context", generation }, { "alpha", alpha }, { "levels", count }, { "cap", cap }, { "filter", filter }, { "fallback", true } });
						continue;
					}
					const int first = cap ? 1 : 0;
					require(texture.authored_mip_count == count - first && texture.mip_base_level == first && texture.w == (16 >> first), "resident mip metadata");
					GLint maximum, minimum;
					glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &maximum);
					glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minimum);
					require(maximum == count - first - 1, "authored chain maximum level");
					require(minimum == (!filter ? GL_NEAREST : count - first == 1 ? GL_LINEAR
					                                       : filter == 1          ? GL_LINEAR_MIPMAP_NEAREST
					                                                              : GL_LINEAR_MIPMAP_LINEAR),
					        "initial filter follows actual mip availability");
					volatile int pending = 1;
					int requested = 2, applied = 0;
					android_ogl_texture_texfilt_state state{ { &texture, 1 }, &pending, &requested, &applied };
					android_ogl_apply_texfilt_all(&state);
					require(glGetError() == GL_NO_ERROR, "live filtering does not regenerate compressed mipmaps");
					std::vector<unsigned char> first_pixel;
					for (int level = first; level < count; ++level) {
						const auto actual = ktx_sample(program, texture.handle, level - first);
						GLuint reference;
						glGenTextures(1, &reference);
						glBindTexture(GL_TEXTURE_2D, reference);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
						glCompressedTexImage2D(GL_TEXTURE_2D, 0, alpha ? GL_COMPRESSED_RGBA8_ETC2_EAC : GL_COMPRESSED_RGB8_ETC2, 16 >> level, 16 >> level, 0, (GLsizei) raw[level].size(), raw[level].data());
						require(actual == ktx_sample(program, reference, 0), "sample matches independently uploaded authored level");
						if (level == first) first_pixel = actual;
						else require(first_pixel != actual, "authored mip differs from base image");
						glDeleteTextures(1, &reference);
					}
					glDeleteTextures(1, &texture.handle);
					texture = {};
					--data.filedata_size;
					require(!android_ogl_upload_ktx2(&texture, &data, filter, cap, "truncated") && !texture.handle, "truncated chain rejected before upload");
					results.push_back({ { "context", generation }, { "alpha", alpha }, { "levels", count }, { "cap", cap }, { "filter", filter }, { "correct", true } });
				}
	glUseProgram(0);
	glDeleteProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vao);
}
#pragma pop_macro("glDrawArrays")
