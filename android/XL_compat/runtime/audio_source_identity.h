#ifndef DXX_XL_AUDIO_SOURCE_IDENTITY_H
#define DXX_XL_AUDIO_SOURCE_IDENTITY_H
#include <string>
#include "picosha2.h"

namespace xl_compat
{
// Older saves included the filesystem's spelling of owned paths in their digest
struct AudioSourceIdentity {
	std::string portable, legacy;

	void append(const std::string &value)
	{
		portable += value;
		legacy += value;
	}

	void append_path(std::string path)
	{
		legacy += path;
		for (char &c : path)
			if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		portable += path;
	}

	void finish(std::string &identity, std::string &legacy_identity) const
	{
		identity = picosha2::hash256_hex_string(portable);
		legacy_identity = legacy == portable ? identity : picosha2::hash256_hex_string(legacy);
	}
};
} // namespace xl_compat
#endif
