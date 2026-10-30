#ifndef DXX_XL_AUDIO_RESOURCE_H
#define DXX_XL_AUDIO_RESOURCE_H
#include <string>
namespace xl_compat
{
struct AudioResource {
	std::string path, bytes;
};
// Resolve only the selected HOG and its owned mission/level sound folders
AudioResource load_audio_resource(const std::string &authored, const char *level, int ordinal);
} // namespace xl_compat
#endif
