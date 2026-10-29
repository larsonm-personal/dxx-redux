#ifndef DXX_XL_NAMED_SOUND_H
#define DXX_XL_NAMED_SOUND_H
#include <string>
#include <vector>
namespace xl_compat
{
// Match LoadAddonSound's numeric/prefix aliases before selecting a resource
std::string named_sound_filename(const std::string &authored);
// HOG first, then the incoming signed level and mission folders
std::vector<std::string> named_sound_paths(const std::string &mission, const std::string &authored, int ordinal);
} // namespace xl_compat
#endif
