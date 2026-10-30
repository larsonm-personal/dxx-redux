#ifndef DXX_XL_NAMED_SOUND_H
#define DXX_XL_NAMED_SOUND_H
#include <string>
namespace xl_compat
{
// Match LoadAddonSound's numeric/prefix aliases before selecting a resource
std::string named_sound_filename(const std::string &authored);
} // namespace xl_compat
#endif
