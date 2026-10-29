#ifndef DXX_XL_LEVEL_TEXT_H
#define DXX_XL_LEVEL_TEXT_H
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xl_compat
{
struct LevelText {
	std::int32_t id, lines;
	std::string text;
};
using LevelTextTable = std::vector<LevelText>;
// MSG and SND share the source reader; their runtime consumers remain separate
LevelTextTable decode_level_text(std::string_view bytes);
const LevelText *find_level_text(const LevelTextTable &table, std::int32_t id);
} // namespace xl_compat
#endif
