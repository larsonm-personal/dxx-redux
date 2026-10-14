/* Opt-in desktop test readback before swap, independent of GL_FRONT support
 * Request: DXX_HOST_PROBE_DIR/capture.request
 * Result: capture.ppm (top-down RGB) and capture.json written last
 */
#ifndef DXX_HOST_RENDER_PROBE_H
#define DXX_HOST_RENDER_PROBE_H

static void host_render_probe_json_string(FILE *file, const GLubyte *value)
{
	fputc('"', file);
	if (value)
		for (; *value; ++value) {
			if (*value == '"' || *value == '\\')
				fputc('\\', file);
			if (*value >= 32)
				fputc(*value, file);
		}
	fputc('"', file);
}

static void host_render_probe_capture(void)
{
	const char *directory = getenv("DXX_HOST_PROBE_DIR");
	char path[4096];
	FILE *file;
	SDL_Surface *surface;
	GLint read_buffer, alignment, row_length, skip_rows, skip_pixels;
	GLenum previous_error, buffer_error, read_error, restore_error;
	unsigned char *pixels;
	size_t stride, size;
	int width, height, y, written = 0;
	if (!directory || !*directory || strlen(directory) > sizeof(path) - 32)
		return;
	snprintf(path, sizeof(path), "%s/capture.request", directory);
	file = fopen(path, "rb");
	if (!file)
		return;
	fclose(file);
	remove(path);
	surface = SDL_GetVideoSurface();
	if (!surface || surface->w <= 0 || surface->h <= 0 || surface->w > 16384 || surface->h > 16384)
		return;
	width = surface->w;
	height = surface->h;
	stride = (size_t) width * 3;
	size = stride * height;
	pixels = (unsigned char *) malloc(size);
	if (!pixels)
		return;
	previous_error = glGetError();
	glGetIntegerv(GL_READ_BUFFER, &read_buffer);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	/* SDL compatibility layers may render into an FBO whose current read
	 * attachment is already the back image; GL_BACK is invalid there
	 */
	if (read_buffer == GL_FRONT || read_buffer == GL_BACK)
		glReadBuffer(GL_BACK);
	buffer_error = glGetError();
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	memset(pixels, 0, size);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	read_error = glGetError();
	glReadBuffer(read_buffer);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	restore_error = glGetError();
	snprintf(path, sizeof(path), "%s/capture.ppm", directory);
	file = fopen(path, "wb");
	if (file) {
		written = fprintf(file, "P6\n%d %d\n255\n", width, height) > 0;
		for (y = height - 1; y >= 0; --y)
			if (fwrite(pixels + (size_t) y * stride, 1, stride, file) != stride)
				written = 0;
		if (fclose(file) != 0)
			written = 0;
	}
	free(pixels);
	snprintf(path, sizeof(path), "%s/capture.json", directory);
	file = fopen(path, "wb");
	if (file) {
		fprintf(file, "{\n  \"width\": %d,\n  \"height\": %d,\n  \"previous_gl_error\": %u,\n  \"buffer_gl_error\": %u,\n  \"read_gl_error\": %u,\n  \"restore_gl_error\": %u,\n  \"original_read_buffer\": %d,\n  \"written\": %s,\n  \"renderer\": ", width, height, (unsigned) previous_error, (unsigned) buffer_error, (unsigned) read_error, (unsigned) restore_error, (int) read_buffer, written ? "true" : "false");
		host_render_probe_json_string(file, glGetString(GL_RENDERER));
		fprintf(file, ",\n  \"version\": ");
		host_render_probe_json_string(file, glGetString(GL_VERSION));
		fprintf(file, "\n}\n");
		fclose(file);
	}
}
#endif
