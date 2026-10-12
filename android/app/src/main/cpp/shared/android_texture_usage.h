#ifndef ANDROID_TEXTURE_USAGE_H
#define ANDROID_TEXTURE_USAGE_H
#ifdef __cplusplus
extern "C" {
#endif
/* Engine-thread functions; renderer slots retain IDs, never borrowed strings */
unsigned int android_texture_source_id(const char *filename);
const char *android_texture_usage_json(void);
const char *android_texture_source(const char *filename);
const char *android_texture_source_name(unsigned int id);
#ifdef __cplusplus
}
#endif
#endif
