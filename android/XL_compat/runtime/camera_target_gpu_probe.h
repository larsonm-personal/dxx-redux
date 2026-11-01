// Shared GL/GLES attachment, depth/stencil and nearest monitor sampling controls
struct CameraTargetPattern {
	GLuint program = 0, vao = 0;
	CameraTargetPattern()
	{
#ifdef ANDROID
		const std::string prefix = "#version 300 es\nprecision highp float; precision highp int; precision highp sampler2D;\n#define LOCATION(n) layout(location=n)\n";
#else
		const std::string prefix = "#version 130\n#define LOCATION(n)\n";
#endif
		const auto vertex = xl_compat::lightning_gpu_compile(GL_VERTEX_SHADER, prefix + "void main(){gl_Position=vec4(gl_VertexID==1?3.0:-1.0,gl_VertexID==2?3.0:-1.0,0.0,1.0);}");
		GLuint fragment = 0;
		try {
			fragment = xl_compat::lightning_gpu_compile(GL_FRAGMENT_SHADER, prefix +
			                                                                    "LOCATION(0) out vec4 color0; LOCATION(1) out vec4 color1; uniform vec4 first; uniform vec4 second; uniform float depthValue; uniform int sampleMode; uniform sampler2D image; uniform vec2 imageSize; uniform float offset; void main(){color0=sampleMode!=0?texture(image,(gl_FragCoord.xy+offset)/imageSize):first;color1=second;gl_FragDepth=depthValue;}");
			program = glCreateProgram();
			glAttachShader(program, vertex);
			glAttachShader(program, fragment);
#ifndef ANDROID
			glBindFragDataLocation(program, 0, "color0");
			glBindFragDataLocation(program, 1, "color1");
#endif
			glLinkProgram(program);
			GLint linked;
			glGetProgramiv(program, GL_LINK_STATUS, &linked);
			if (!linked) throw std::runtime_error("Camera target control program did not link");
			glGenVertexArrays(1, &vao);
		} catch (...) {
			if (program) glDeleteProgram(program);
			if (fragment) glDeleteShader(fragment);
			glDeleteShader(vertex);
			throw;
		}
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}
	~CameraTargetPattern()
	{
		glDeleteVertexArrays(1, &vao);
		glDeleteProgram(program);
	}
	void draw()
	{
		glBindVertexArray(vao);
#ifdef ANDROID
#undef glDrawArrays
		glDrawArrays(GL_TRIANGLES, 0, 3);
#define glDrawArrays gles3_shim_draw_arrays
#else
		glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
	}
	template <class Select>
	void paint(int width, int height, Select select)
	{
		xl_compat::lightning_gpu_program(program);
		glUniform1i(glGetUniformLocation(program, "sampleMode"), 0);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_SCISSOR_TEST);
#ifndef ANDROID
		glDisable(GL_ALPHA_TEST);
#endif
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glStencilMask(255);
#ifdef ANDROID
		glClearDepthf(1);
#else
		glClearDepth(1);
#endif
		glClearStencil(0);
		select(0);
		glClearColor(.25f, .5f, .75f, .125f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		select(1);
		glClearColor(.75f, .25f, .5f, .25f);
		glClear(GL_COLOR_BUFFER_BIT);
		select(-1);
		glViewport(0, 0, width, height);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 7, 255);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glEnable(GL_SCISSOR_TEST);
		glScissor(0, 0, width / 2, height / 2);
		glUniform4f(glGetUniformLocation(program, "first"), .125f, .25f, .375f, .1f);
		glUniform4f(glGetUniformLocation(program, "second"), .5f, .625f, .75f, .2f);
		glUniform1f(glGetUniformLocation(program, "depthValue"), .6f);
		draw();
		select(0);
		glDisable(GL_SCISSOR_TEST);
		glStencilFunc(GL_EQUAL, 7, 255);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glUniform4f(glGetUniformLocation(program, "first"), .875f, .125f, .0625f, .3f);
		glUniform1f(glGetUniformLocation(program, "depthValue"), .4f);
		draw();
		glUniform4f(glGetUniformLocation(program, "first"), 1, 1, 1, 1);
		glUniform1f(glGetUniformLocation(program, "depthValue"), .8f);
		draw();
		glStencilFunc(GL_EQUAL, 8, 255);
		glUniform1f(glGetUniformLocation(program, "depthValue"), .2f);
		draw();
	}
	std::vector<unsigned char> sample(GLuint texture, int width, int height, int offset)
	{
		xl_compat::LightningGlowState restore;
		GLuint fbo = 0, image = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glGenTextures(1, &image);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, image);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Camera sampling target incomplete");
		xl_compat::lightning_gpu_program(program);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(glGetUniformLocation(program, "image"), 0);
		glUniform1i(glGetUniformLocation(program, "sampleMode"), 1);
		glUniform2f(glGetUniformLocation(program, "imageSize"), float(width), float(height));
		glUniform1f(glGetUniformLocation(program, "offset"), float(offset));
		glViewport(0, 0, width, height);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
#ifndef ANDROID
		glDisable(GL_ALPHA_TEST);
#endif
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		draw();
		std::vector<unsigned char> result(size_t(width) * height * 4);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
		glDeleteTextures(1, &image);
		glDeleteFramebuffers(1, &fbo);
		return result;
	}
};
static std::vector<GLint> camera_target_gl_state()
{
	std::vector<GLint> result;
	for (GLenum field : { GL_READ_FRAMEBUFFER_BINDING, GL_DRAW_FRAMEBUFFER_BINDING, GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING,
	                      GL_ACTIVE_TEXTURE, GL_TEXTURE_BINDING_2D, GL_PIXEL_UNPACK_BUFFER_BINDING, GL_DEPTH_FUNC, GL_DEPTH_WRITEMASK,
	                      GL_STENCIL_FUNC, GL_STENCIL_REF, GL_STENCIL_VALUE_MASK, GL_STENCIL_WRITEMASK, GL_STENCIL_FAIL, GL_STENCIL_PASS_DEPTH_FAIL, GL_STENCIL_PASS_DEPTH_PASS,
	                      GL_STENCIL_BACK_FUNC, GL_STENCIL_BACK_REF, GL_STENCIL_BACK_VALUE_MASK, GL_STENCIL_BACK_WRITEMASK,
	                      GL_STENCIL_BACK_FAIL, GL_STENCIL_BACK_PASS_DEPTH_FAIL, GL_STENCIL_BACK_PASS_DEPTH_PASS }) {
		GLint value;
		glGetIntegerv(field, &value);
		result.push_back(value);
	}
	for (GLenum field : { GL_VIEWPORT, GL_SCISSOR_BOX }) {
		GLint value[4];
		glGetIntegerv(field, value);
		result.insert(result.end(), value, value + 4);
	}
	for (GLenum cap : { GL_DEPTH_TEST, GL_STENCIL_TEST, GL_SCISSOR_TEST, GL_BLEND, GL_CULL_FACE }) result.push_back(glIsEnabled(cap));
	return result;
}
unsigned camera_target_device_controls(std::vector<unsigned char> &retained)
{
	using namespace xl_compat;
	LightningGlowState restore;
	CameraTargetPattern pattern;
	static std::array<CameraTargetGpu, 8> targets;
	unsigned cases = 0, target_index = 0;
	for (const auto &screen : { std::array<int, 2>{ 14, 10 }, { 32, 24 }, { 63, 35 }, { 64, 47 } })
		for (bool hires : { false, true }) {
			auto &target = targets[target_index++];
			if (target.valid()) throw std::runtime_error("Camera image validity survived actual context loss");
			const auto spec = camera_target_spec(screen[0], screen[1], hires);
			const auto before = camera_target_gl_state();
			const auto capture = [&] { target.capture(spec, [&] { pattern.paint(spec.width, spec.height, [&](int buffer) { target.select(buffer); }); }); };
			capture();
			if (!target.valid() || camera_target_gl_state() != before) throw std::runtime_error("Camera capture lost caller GL state");
			for (unsigned color = 0; color < 2; ++color)
				for (int offset : { -3, 0, 3 }) {
					const auto pixels = pattern.sample(target.texture(color), spec.width, spec.height, offset);
					for (int y = 0; y < spec.height; ++y)
						for (int x = 0; x < spec.width; ++x) {
							const int sx = (std::max) (0, (std::min) (spec.width - 1, x + offset));
							const int sy = (std::max) (0, (std::min) (spec.height - 1, y + offset));
							const bool inside = sx < spec.width / 2 && sy < spec.height / 2;
							const std::array<int, 4> expected = color ? (inside ? std::array<int, 4>{ 128, 160, 191, 255 } : std::array<int, 4>{ 191, 64, 128, 255 })
							                                          : (inside ? std::array<int, 4>{ 223, 32, 16, 255 } : std::array<int, 4>{ 64, 128, 191, 255 });
							for (unsigned c = 0; c < 4; ++c)
								if (std::abs(int(pixels[(size_t(y) * spec.width + x) * 4 + c]) - expected[c]) > 1)
									throw std::runtime_error("Camera RGB/depth/stencil/nearest sampling differs");
						}
					retained.insert(retained.end(), pixels.begin(), pixels.end());
					++cases;
				}
			bool failed = false;
			const auto failure_before = camera_target_gl_state();
			try {
				target.capture(spec, [] { throw std::runtime_error("Controlled camera callback failure"); });
			} catch (const std::runtime_error &) {
				failed = true;
			}
			if (!failed || target.valid() || camera_target_gl_state() != failure_before) throw std::runtime_error("Failed camera callback published an image or leaked GL state");
			capture();
			if (!target.valid() || glGetError() != GL_NO_ERROR) throw std::runtime_error("Camera target did not recover from callback failure");
		}
	return cases;
}
