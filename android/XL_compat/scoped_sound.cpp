#include "scoped_sound.h"
#include <cstdio>
#include <stdexcept>
namespace xl_compat
{
namespace
{
bool component(const std::string &value)
{
	if (value.empty() || value.size() > 255 || value == "." || value == "..") return false;
	for (unsigned char c : value)
		if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':') return false;
	return true;
}
} // namespace
ScopedSound resolve_scoped_sound(const std::string &identity, const std::string &sample,
                                 int ordinal, int bank_rate,
                                 const std::function<std::string(const std::string &)> &owned_file)
{
	if (!component(identity) || !component(sample) || ordinal < -9999 || ordinal > 9999 || bank_rate <= 0)
		throw std::runtime_error("Invalid scoped sound identity, ordinal or rate");
	const auto root = "mods/" + identity + "/";
	if (const auto bank = owned_file(root + identity + (bank_rate >= 22050 ? ".s22" : ".s11")); !bank.empty())
		return { bank, "mission_bank", true };
	if (ordinal) {
		char folder[32];
		std::snprintf(folder, sizeof(folder), "%s%02d/", ordinal < 0 ? "slevel" : "level", ordinal < 0 ? -ordinal : ordinal);
		if (const auto file = owned_file(root + "sounds/" + folder + sample + ".wav"); !file.empty())
			return { file, ordinal < 0 ? "secret_level" : "level", false };
	}
	if (const auto file = owned_file(root + "sounds/" + sample + ".wav"); !file.empty())
		return { file, "mission", false };
	return {};
}
} // namespace xl_compat
