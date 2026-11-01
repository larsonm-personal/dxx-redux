// GL_CLAMP for indexed/truecolor sprite bodies, including rectangular mip tails
// ES lacks legacy GL_CLAMP: sample the original mip texels with a zero border
namespace
{
struct SpriteBitmapGpu {
	GLuint program = 0, vao = 0, buffer = 0;
	void discard(bool abandoned)
	{
		if (!abandoned) {
			if (program) glDeleteProgram(program);
			if (vao) glDeleteVertexArrays(1, &vao);
			if (buffer) glDeleteBuffers(1, &buffer);
		}
		*this = {};
	}
} sprite_bitmap_gpu;

void prepare_sprite_bitmap()
{
	if (sprite_bitmap_gpu.program) return;
#ifdef ANDROID
	const std::string prefix = "#version 300 es\nprecision highp float; precision highp int; precision highp sampler2D;\n";
#else
	const std::string prefix = "#version 130\n";
#endif
	const auto vertex = particle_shader(GL_VERTEX_SHADER, prefix +
	                                                          "in vec3 position; in vec2 uv; in vec4 color; uniform mat4 mvp; out vec2 spriteUV; out vec4 spriteColor; void main(){spriteUV=uv; spriteColor=color; gl_Position=mvp*vec4(position,1.0);}");
	GLuint fragment = 0, program = 0;
	try {
		fragment = particle_shader(GL_FRAGMENT_SHADER, prefix + R"GLSL(
uniform sampler2D image;
uniform int minFilter, magFilter;
in vec2 spriteUV;
in vec4 spriteColor;
out vec4 pixel;
vec4 bordered(ivec2 p, ivec2 size, int level) {
    if (any(lessThan(p,ivec2(0))) || any(greaterThanEqual(p,size))) return vec4(0.0);
    return texelFetch(image,p,level);
}
vec4 sampleLevel(int level, bool linear) {
    ivec2 size=textureSize(image,level);
    vec2 uv=clamp(spriteUV,vec2(0.0),vec2(1.0));
    if (!linear) return texelFetch(image,clamp(ivec2(floor(uv*vec2(size))),ivec2(0),size-1),level);
    vec2 p=uv*vec2(size)-0.5;
    ivec2 lower=ivec2(floor(p));
    vec2 f=fract(p);
    return mix(mix(bordered(lower,size,level),bordered(lower+ivec2(1,0),size,level),f.x),
               mix(bordered(lower+ivec2(0,1),size,level),bordered(lower+ivec2(1,1),size,level),f.x),f.y);
}
void main() {
    vec2 size=vec2(textureSize(image,0));
    vec2 dx=dFdx(spriteUV)*size, dy=dFdy(spriteUV)*size;
    float lod=0.5*log2(max(max(dot(dx,dx),dot(dy,dy)),1.0e-20));
    float crossover=(magFilter==9729 && minFilter==9984)?0.5:0.0;
    vec4 color;
    if (lod<=crossover) color=sampleLevel(0,magFilter==9729);
    else if (minFilter==9728 || minFilter==9729) color=sampleLevel(0,minFilter==9729);
    else {
        float last=floor(log2(max(size.x,size.y)));
        lod=clamp(lod,0.0,last);
        bool linear=(minFilter==9985 || minFilter==9987);
        if (minFilter==9984 || minFilter==9985) color=sampleLevel(int(floor(lod+0.5)),linear);
        else {
            int lower=int(floor(lod));
            color=mix(sampleLevel(lower,linear),sampleLevel(min(lower+1,int(last)),linear),fract(lod));
        }
    }
    pixel=color*spriteColor;
}
)GLSL");
		program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glBindAttribLocation(program, 0, "position");
		glBindAttribLocation(program, 1, "uv");
		glBindAttribLocation(program, 2, "color");
		glLinkProgram(program);
		GLint linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (!linked) throw std::runtime_error("XL sprite bitmap shader link failed");
		glGenVertexArrays(1, &sprite_bitmap_gpu.vao);
		glGenBuffers(1, &sprite_bitmap_gpu.buffer);
		sprite_bitmap_gpu.program = program;
	} catch (...) {
		if (fragment) glDeleteShader(fragment);
		if (program) glDeleteProgram(program);
		glDeleteShader(vertex);
		throw;
	}
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void draw_sprite_bitmap_gpu(GLuint image, const xl_compat::ParticleQuad &quad)
{
	ParticleSoftRestore restore;
	prepare_sprite_bitmap();
	GLfloat matrix[16]{};
#ifdef ANDROID
	std::copy_n(gles3_shim_get_mvp(), 16, matrix);
#else
	GLfloat projection[16], model[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projection);
	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	for (unsigned column = 0; column < 4; ++column)
		for (unsigned row = 0; row < 4; ++row)
			for (unsigned k = 0; k < 4; ++k) matrix[column * 4 + row] += projection[k * 4 + row] * model[column * 4 + k];
	glDisable(GL_ALPHA_TEST);
#endif
	particle_program(sprite_bitmap_gpu.program);
	glUniformMatrix4fv(glGetUniformLocation(sprite_bitmap_gpu.program, "mvp"), 1, GL_FALSE, matrix);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, image);
	GLint min_filter, mag_filter;
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
	glUniform1i(glGetUniformLocation(sprite_bitmap_gpu.program, "image"), 0);
	glUniform1i(glGetUniformLocation(sprite_bitmap_gpu.program, "minFilter"), min_filter);
	glUniform1i(glGetUniformLocation(sprite_bitmap_gpu.program, "magFilter"), mag_filter);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(sprite_bitmap_gpu.vao);
	glBindBuffer(GL_ARRAY_BUFFER, sprite_bitmap_gpu.buffer);
	using Vertex = xl_compat::ParticleVertex;
	// Explicit triangles preserve quad coverage; nearest-boundary interpolation may differ from GL_QUADS
	const std::array<Vertex, 6> triangles{ quad.vertices[0], quad.vertices[1], quad.vertices[3], quad.vertices[1], quad.vertices[2], quad.vertices[3] };
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangles), triangles.data(), GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
#ifdef ANDROID
#undef glDrawArrays
	glDrawArrays(GL_TRIANGLES, 0, 6);
#define glDrawArrays gles3_shim_draw_arrays
#else
	glDrawArrays(GL_TRIANGLES, 0, 6);
#endif
	if (glGetError() != GL_NO_ERROR) throw std::runtime_error("XL sprite bitmap draw failed");
}
} // namespace
