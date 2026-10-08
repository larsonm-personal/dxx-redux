// Binding ownership controls and actual enhanced/native draw interleaving
#include "xdescent.h"
extern "C" {
#include "xmodel.h"
}
struct binding_fixture_state {
	int binds = 0, reuse = 0;
#ifndef GLES3_SHIM_TEXTURE_UNIT_COUNT
	GLuint cached[3]{};
	int unit = 0;
	android_ogl_bind_texture_state state = { cached, 3, &unit, &binds, &reuse };
#else
	android_ogl_bind_texture_state state = { &binds, &reuse };
#endif
};
static void binding_image(GLuint texture, int source)
{
	glBindTexture(GL_TEXTURE_2D, texture);
	const unsigned char pixel[4] = { static_cast<unsigned char>(source ? 20 : 210), static_cast<unsigned char>(source ? 220 : 30), 70, 255 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
static void binding_assets(int variant = 0)
{
	gameFolders.game.szModels = const_cast<char *>("");
	const int width = variant % 2 ? 15 : 8, height = variant % 2 ? 13 : 8, channels = variant >= 2 ? 4 : 3;
	for (int team = 0; team < 2; ++team) {
		std::vector<unsigned char> tga(18 + width * height * channels);
		tga[2] = 2;
		tga[12] = width;
		tga[14] = height;
		tga[16] = channels * 8;
		tga[17] = channels == 4 ? 8 : 0;
		for (int i = 0; i < width * height; ++i) {
			tga[18 + i * channels] = team ? 50 : 190;
			tga[19 + i * channels] = team ? 230 : 25;
			tga[20 + i * channels] = team ? 15 : 210;
			if (channels == 4) tga[21 + i * channels] = 255;
		}
		FILE *file = std::fopen(team ? "color1.tga" : "color0.tga", "wb");
		require(file && std::fwrite(tga.data(), 1, tga.size(), file) == tga.size(), "valid model TGA");
		std::fclose(file);
	}

	const char *ase = R"ASE(*3DSMAX_ASCIIEXPORT 200
*MATERIAL_LIST {
 *MATERIAL_COUNT 2
 *MATERIAL 0 {
  *MAP_DIFFUSE {
   *BITMAP "color0.tga"
  }
 }
 *MATERIAL 1 {
  *MAP_DIFFUSE {
   *BITMAP "color1.tga"
  }
 }
}
*GEOMOBJECT {
 *NODE_NAME "body"
 *NODE_TM {
  *TM_POS 0 0 0
 }
 *MATERIAL_REF 0
 *MESH {
  *MESH_NUMVERTEX 3
  *MESH_NUMTVERTEX 3
  *MESH_NUMFACES 1
  *MESH_VERTEX_LIST {
   *MESH_VERTEX 0 -0.75 0 0.75
   *MESH_VERTEX 1 0.75 0 0.75
   *MESH_VERTEX 2 0 0 -0.75
  }
  *MESH_FACE_LIST {
   *MESH_FACE 0: A: 0 B: 1 C: 2
  }
  *MESH_TVERTLIST {
   *MESH_TVERT 0 0 0 0
   *MESH_TVERT 1 1 0 0
   *MESH_TVERT 2 0.5 1 0
  }
  *MESH_TFACELIST {
   *MESH_TFACE 0 0 1 2
  }
 }
}
)ASE";
	FILE *file = std::fopen("binding.ase", "wb");
	require(file && std::fwrite(ase, 1, std::strlen(ase), file) == std::strlen(ase), "valid model ASE");
	std::fclose(file);
}
static void binding_font_mode()
{
	glViewport(0, 0, 320, 240);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrthof(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0.03f, 0.07f, 0.11f, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gr_set_fontcolor(12, -1);
}
static std::string binding_font()
{
	binding_font_mode();
	gr_ustring(10, 10, "ABCD");
	return readback_hash();
}
#ifdef GLES3_SHIM_TEXTURE_UNIT_COUNT
static void test_binding_owner_controls(nlohmann::json &results, int generation)
{
	for (int scenario = 0; scenario < 5; ++scenario) {
		binding_fixture_state first, second;
		GLuint images[2];
		glGenTextures(2, images);
		glActiveTexture(GL_TEXTURE2);
		binding_image(images[0], 0);
		binding_image(images[1], 1);
		android_ogl_reset_texture_bindings(&first.state);
		int expected_unit = 2;
		GLuint wanted = images[0];
		if (scenario == 0) {
			android_ogl_bind_texture_2d(&first.state, wanted);
			android_ogl_bind_texture_2d(&first.state, wanted);
			require(first.binds == 1 && first.reuse == 1, "reset preserves actual nonzero unit and reuse");
		}
		if (scenario == 1) {
			android_ogl_bind_texture_2d(&first.state, wanted);
			android_ogl_bind_texture_2d(&second.state, wanted);
			require(first.binds == 1 && second.binds == 0 && second.reuse == 1, "adapters share binding owner");
		}
		if (scenario == 2) {
			for (int unit = 0; unit < 3; ++unit) {
				glActiveTexture(GL_TEXTURE0 + unit);
				glBindTexture(GL_TEXTURE_2D, wanted);
			}
			glDeleteTextures(1, &images[0]);
			images[0] = wanted = 0;
			for (int unit = 0; unit < 3; ++unit) {
				glActiveTexture(GL_TEXTURE0 + unit);
				android_ogl_bind_texture_2d(&first.state, 0);
				GLint binding;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
				require(binding == 0, "deleted object unbound on every tracked unit");
			}
			require(first.binds == 3 && first.reuse == 0, "deletion invalidates every matching unit");
		}
		if (scenario == 3) {
			expected_unit = 3;
			glActiveTexture(GL_TEXTURE3);
			android_ogl_bind_texture_2d(&first.state, wanted);
			android_ogl_bind_texture_2d(&first.state, wanted);
			require(first.binds == 2 && first.reuse == 0, "untracked valid unit always binds");
			glBindTexture(GL_TEXTURE_2D, images[1]);
			android_ogl_bind_texture_2d(&first.state, wanted);
		}
		if (scenario == 4) {
			android_ogl_bind_texture_2d(&first.state, wanted);
			GLuint cube;
			glGenTextures(1, &cube);
			glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
			android_ogl_bind_texture_2d(&first.state, wanted);
			require(first.binds == 1 && first.reuse == 1, "other texture targets preserve 2D binding");
			glDeleteTextures(1, &cube);
		}
		GLint actual, active;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &actual);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		require(static_cast<GLuint>(actual) == wanted && active == GL_TEXTURE0 + expected_unit, "binding owner control matches GL");
		require(glGetError() == GL_NO_ERROR, "binding owner control GL status");
		results.push_back({ { "context", generation }, { "kind", "owner" }, { "scenario", scenario }, { "correct", true } });
		glDeleteTextures(2, images);
	}
	glActiveTexture(GL_TEXTURE0);
}
#endif
static void test_texture_bindings(nlohmann::json &results, int generation, bool baseline)
{
	for (int scenario = 0; scenario < 10; ++scenario)
		for (int unit = 0; unit < 3; ++unit) {
			binding_fixture_state first, second;
			GLuint images[2];
			glGenTextures(2, images);
			glActiveTexture(GL_TEXTURE0 + unit);
			binding_image(images[0], 0);
			binding_image(images[1], 1);
			android_ogl_reset_texture_bindings(&first.state);
			android_ogl_active_texture(&first.state, GL_TEXTURE0 + unit);
			android_ogl_bind_texture_2d(&first.state, images[0]);
			GLuint wanted = images[0];
			int expected_unit = unit;
			if (scenario == 0) android_ogl_bind_texture_2d(&first.state, wanted);
			if (scenario == 1) {
				glBindTexture(GL_TEXTURE_2D, images[1]);
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			if (scenario == 2) {
				expected_unit = (unit + 1) % 3;
				glActiveTexture(GL_TEXTURE0 + expected_unit);
				glBindTexture(GL_TEXTURE_2D, images[1]);
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			if (scenario == 3) {
				android_ogl_bind_texture_2d(nullptr, images[1]);
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			if (scenario == 4) {
				glDeleteTextures(1, &images[0]);
				images[0] = 0;
				wanted = 0;
				android_ogl_bind_texture_2d(&first.state, 0);
				glGenTextures(1, &images[0]);
				binding_image(images[0], 0);
				wanted = images[0];
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			if (scenario == 5) {
				android_ogl_reset_texture_bindings(&second.state);
				android_ogl_active_texture(&second.state, GL_TEXTURE0 + unit);
				android_ogl_bind_texture_2d(&second.state, images[1]);
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			if (scenario >= 6 && scenario <= 8) {
				ogl_texture textures[2]{};
				textures[0].handle = images[0];
				textures[1].handle = images[1];
				int pending = 1, requested = scenario - 6, applied = 0;
#ifndef GLES3_SHIM_TEXTURE_UNIT_COUNT
				GLuint legacy = images[0];
				android_ogl_texture_texfilt_state filters = { { textures, 2 }, &legacy, &pending, &requested, &applied };
#else
				android_ogl_texture_texfilt_state filters = { { textures, 2 }, &pending, &requested, &applied };
#endif
				android_ogl_apply_texfilt_all(&filters);
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			if (scenario == 9) {
				android_ogl_reset_transient_blit_texture(&first.state, 1);
				android_ogl_active_texture(&first.state, GL_TEXTURE0 + unit);
				android_ogl_bind_texture_2d(&first.state, wanted);
			}
			GLint actual, active;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &actual);
			glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
			bool correct = static_cast<GLuint>(actual) == wanted && active == GL_TEXTURE0 + expected_unit;
			if (!baseline || scenario == 0 || scenario == 4 || scenario == 9) require(correct, "binding owner matches actual texture/unit");
			if (scenario == 0) require(first.binds == 1 && first.reuse == 1, "valid same-unit reuse counters");
			require(glGetError() == GL_NO_ERROR, "valid texture binding control GL status");
			results.push_back({ { "context", generation }, { "kind", "helper" }, { "scenario", scenario }, { "unit", unit }, { "correct", correct } });
			glDeleteTextures(2, images);
		}
	glActiveTexture(GL_TEXTURE0);
	g3_start_frame();
	g3_end_frame();
	std::fprintf(stderr, "binding: assets begin\n");
	binding_assets();
	std::fprintf(stderr, "binding: assets ready\n");
	void *model = xmodel_load("binding.ase");
	std::fprintf(stderr, "binding: model loaded %d\n", model != nullptr);
	require(model, "load valid enhanced model");
	binding_font_mode();
	const std::string empty_font = readback_hash();
	const std::string reference = binding_font();
	require(reference != empty_font, "native font reference changes pixels");
	std::fprintf(stderr, "binding: reference ready\n");
	for (int scenario = 0; scenario < 4; ++scenario) {
		std::fprintf(stderr, "binding: model scenario %d\n", scenario);
		// Explicit native reset establishes the font reference before each mutation
		g3_start_frame();
		g3_end_frame();
		require(binding_font() == reference, "stable native font reference");
		if (scenario == 0) {
			require(xmodel_load_gl(model) == 0, "enhanced model upload");
			const GLenum upload_error = glGetError();
			require(upload_error == GL_NO_ERROR || (baseline && upload_error == GL_INVALID_ENUM), "model upload GL status (known BR-0197 baseline)");
		}
		if (scenario == 1 || scenario == 2) {
			binding_font_mode();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			const std::string empty = readback_hash();
			g3s_lrgb light{ F1_0, F1_0, F1_0 };
			auto before = gles3_shim_get_draw_stats();
			xmodel_show(model, scenario == 1 ? -1 : 0, &light);
			auto after = gles3_shim_get_draw_stats();
			require(after.calls - before.calls == 1 && after.triangle_vertices - before.triangle_vertices == 3, "actual enhanced-model VBO draw");
			require(readback_hash() != empty, "enhanced model changes pixels");
		}
		if (scenario == 3) {
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			g3s_lrgb light{ F1_0, F1_0, F1_0 };
			xmodel_show(model, 0, &light);
			xmodel_free_gl(model);
		}
		std::string actual = binding_font();
		bool correct = actual == reference;
		if (!baseline) require(correct, "native font pixels after enhanced model mutation");
		results.push_back({ { "context", generation }, { "kind", "enhanced_native" }, { "scenario", scenario }, { "correct", correct }, { "native_sha256", actual }, { "reference_sha256", reference } });
	}
	xmodel_free(model);
	glActiveTexture(GL_TEXTURE0);
	require(glGetError() == GL_NO_ERROR, "binding fixture cleanup");
#ifdef GLES3_SHIM_TEXTURE_UNIT_COUNT
	if (!baseline) test_binding_owner_controls(results, generation);
#endif
}

static void test_xmodel_mipmaps(nlohmann::json &results, int generation, bool baseline)
{
	// Isolate mipmap generation from the separate tightly packed RGB upload issue
	GLint unpack_alignment;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GameCfg.ClassicDepth = 0;
	for (int variant = 0; variant < 4; ++variant) {
		binding_assets(variant);
		void *model = xmodel_load("binding.ase");
		require(model, "load RGB/RGBA POT/NPOT model");
		require(xmodel_load_gl(model) == 0, "upload model texture matrix");
		GLenum upload_error = glGetError();
		if (!baseline) require(upload_error == GL_NO_ERROR, "model mipmap upload has no GL error");
		else require(upload_error == GL_NO_ERROR || upload_error == GL_INVALID_ENUM, "known legacy mipmap parameter baseline");
		for (int team = 0; team < 2; ++team) {
			binding_font_mode();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glScalef(0.03f, 0.03f, 1);
			std::string empty = readback_hash();
			g3s_lrgb light{ F1_0, F1_0, F1_0 };
			xmodel_show(model, team ? 0 : -1, &light);
			std::string mipped = readback_hash();
			GLint filter;
			glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
			require(filter == GL_LINEAR_MIPMAP_LINEAR, "model requests mipmapped filtering");
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			xmodel_show(model, team ? 0 : -1, &light);
			std::string reference = readback_hash();
			require(reference != empty, "base-level model reference changes pixels");
			if (!baseline) require(mipped == reference, "minified mipmapped texture equals uniform base-level reference");
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			results.push_back({ { "context", generation }, { "variant", variant }, { "team", team }, { "upload_error", upload_error }, { "mipmapped_sha256", mipped }, { "reference_sha256", reference }, { "correct", mipped == reference } });
		}
		xmodel_free(model);
		require(glGetError() == GL_NO_ERROR, "model matrix cleanup");
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
}
