#include "level_texture_diagnostics.h"

#include <map>

namespace
{
std::map<int, unsigned> Invalid_texture_counts;
}

extern "C" void level_texture_diagnostics_reset(void)
{
	Invalid_texture_counts.clear();
}

extern "C" void level_texture_diagnostics_record(int texture)
{
	++Invalid_texture_counts[texture];
}

std::vector<std::string> level_texture_diagnostic_notes()
{
	std::vector<std::string> notes;
	for (const auto &entry : Invalid_texture_counts)
		notes.push_back("invalid texture " + std::to_string(entry.first) + ", " +
		                std::to_string(entry.second) +
		                (entry.second == 1 ? " occurrence" : " occurrences"));
	return notes;
}
