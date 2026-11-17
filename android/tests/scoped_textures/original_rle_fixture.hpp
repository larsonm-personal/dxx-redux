#ifdef DXX_BUILD_DESCENT_II
#include "../../XL_compat/scoped_texture_image.h"
#include "../../XL_compat/runtime/pog_renderer.h"

static nlohmann::json test_original_rle(GLuint program, GLuint vao)
{
	const auto bytes = original_animation_bytes("rock194.tga");
	const auto input_hash = picosha2::hash256_hex_string(bytes);
	require(input_hash == "ffbc219965339e0f079838b1919836adc2e1037ef67ab679d717d97adf01f064", "original RLE input identity");
	const auto image = xl_compat::read_scoped_texture_image(bytes.data(), bytes.size());
	const auto decoded_hash = picosha2::hash256_hex_string(image.pixels);
	// Independently decoded with Pillow; top-left RGBA, including opaque alpha
	require(image.width == 512 && image.height == 512 && image.rgb_opaque &&
	            decoded_hash == "370bf54a6cda458661dd8439c1ff749d38931b6dd005dcd0d69e6512b1a7b3e5",
	        "original RLE independent pixel identity");
	const auto sim = d_rand_get_stream_call_count(D_RNG_SIM), fx = d_rand_get_stream_call_count(D_RNG_FX);
	auto pixels = image.pixels;
	grs_bitmap bitmap{};
	bitmap.bm_type = BM_RGBA;
	bitmap.bm_w = image.width;
	bitmap.bm_h = image.height;
	bitmap.bm_rowsize = image.width * 4;
	bitmap.bm_data = pixels.data();
	require(!ogl_xl_rgba_load(&bitmap, 0, "rock194"), "original RLE native GPU upload");
	const auto *texture = bitmap.gltexture;
	glViewport(0, 0, 64, 64);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	gles3_shim_bind_program(program);
	glBindVertexArray(vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glUniform1i(glGetUniformLocation(program, "source"), 0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	std::vector<unsigned char> captured(64 * 64 * 4);
	glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, captured.data());
	require(glGetError() == GL_NO_ERROR, "original RLE GPU readback state");
	for (unsigned y = 0; y < 64; ++y)
		for (unsigned x = 0; x < 64; ++x)
			for (unsigned channel = 0; channel < 4; ++channel) {
				if (captured[(y * 64 + x) * 4 + channel] != image.pixels[((y * 8 + 4) * 512 + x * 8 + 4) * 4 + channel])
					std::fprintf(stderr, "RLE GPU mismatch x=%u y=%u channel=%u actual=%u expected=%u texture=%dx%d allocated=%dx%d\n", x, y, channel,
					             captured[(y * 64 + x) * 4 + channel], image.pixels[((y * 8 + 4) * 512 + x * 8 + 4) * 4 + channel], texture->w, texture->h, texture->tw, texture->th);
				require(captured[(y * 64 + x) * 4 + channel] == image.pixels[((y * 8 + 4) * 512 + x * 8 + 4) * 4 + channel],
				        "original RLE sampled surface pixels");
			}
	require(sim == d_rand_get_stream_call_count(D_RNG_SIM) && fx == d_rand_get_stream_call_count(D_RNG_FX), "original RLE rendering preserves RNG");
	ogl_freebmtexture(&bitmap);
	return { { "input_sha256", input_hash }, { "rgba_sha256", decoded_hash }, { "gpu_sha256", picosha2::hash256_hex_string(captured) }, { "sampled_pixels", 4096 }, { "correct", true } };
}
#endif
