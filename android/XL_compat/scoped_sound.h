#ifndef DXX_XL_SCOPED_SOUND_H
#define DXX_XL_SCOPED_SOUND_H
#include <functional>
#include <string>
namespace xl_compat
{
struct ScopedSound {
	std::string path, scope;
	bool bank = false;
};
// Callback returns a case-correct path owned by the selected context, or empty
// Identity is the resolved mod/HOG stem, not an archive name or display title
ScopedSound resolve_scoped_sound(const std::string &identity, const std::string &sample,
                                 int ordinal, int bank_rate,
                                 const std::function<std::string(const std::string &)> &owned_file);
} // namespace xl_compat
#endif
