#include "level_text.h"
#include <algorithm>
#include <stdexcept>

namespace xl_compat
{
namespace
{
bool digit(char value)
{
	return value >= '0' && value <= '9';
}
bool space(char value)
{
	return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\v' || value == '\f';
}
std::int32_t text_id(const std::string &buffer, std::size_t cursor)
{
	while (cursor < buffer.size() && space(buffer[cursor])) ++cursor;
	bool negative = false;
	if (cursor < buffer.size() && (buffer[cursor] == '-' || buffer[cursor] == '+')) negative = buffer[cursor++] == '-';
	std::uint32_t value = 0;
	const std::uint32_t limit = negative ? 2147483648u : 2147483647u;
	while (cursor < buffer.size() && digit(buffer[cursor])) {
		const auto next = static_cast<unsigned>(buffer[cursor++] - '0');
		if (value > (limit - next) / 10u) throw std::runtime_error("XL level text ID exceeds signed 32-bit range");
		value = value * 10 + next;
	}
	return static_cast<std::int32_t>(negative ? -static_cast<std::int64_t>(value) : value);
}
} // namespace

LevelTextTable decode_level_text(std::string_view bytes)
{
	if (bytes.size() > 1024 * 1024) throw std::runtime_error("XL level text exceeds the supported 1 MiB limit");
	if (bytes.find('\0') != std::string_view::npos) throw std::runtime_error("XL level text contains an embedded NUL");
	std::string input("\n");
	input.append(bytes);
	input.push_back('\0');
	std::string output(input.size(), '\0');
	struct Entry {
		std::int32_t id, lines;
		std::size_t offset;
	};
	std::vector<Entry> entries;
	std::size_t write = 0;
	std::int32_t lines = 0;
	// Preserve LoadTextData's cursor rules, including its single separator byte
	// and whitespace behavior. Owned offsets replace its unchecked raw pointers
	for (std::size_t cursor = 0; cursor < input.size(); ++cursor) {
		const char value = input[cursor];
		if (!value) {
			output[write++] = '\0';
			if (!entries.empty()) entries.back().lines = lines;
			break;
		}
		if (value == '\r' || value == '\n') {
			if (lines && !entries.empty()) entries.back().lines = lines;
			output[write++] = '\0';
			cursor += value == '\r' && input[cursor + 1] == '\n' ? 2 : 1;
			if (!input[cursor]) {
				output[write++] = '\0';
				break;
			}
			const auto id = text_id(input, cursor);
			if (!id) continue;
			while (digit(input[cursor])) ++cursor;
			if (!input[cursor]) throw std::runtime_error("XL level text ID has no separator or text");
			entries.push_back({ id, 0, write });
			lines = 1;
		} else if (value == '\\' && input[cursor + 1] == 'n') {
			output[write++] = '\n';
			++lines;
			++cursor;
		} else output[write++] = value;
	}
	LevelTextTable result;
	result.reserve(entries.size());
	for (const auto &entry : entries) result.push_back({ entry.id, entry.lines, output.c_str() + entry.offset });
	std::sort(result.begin(), result.end(), [](const LevelText &a, const LevelText &b) { return a.id < b.id; });
	for (std::size_t i = 1; i < result.size(); ++i)
		if (result[i - 1].id == result[i].id) throw std::runtime_error("Duplicate XL level text ID " + std::to_string(result[i].id));
	return result;
}

const LevelText *find_level_text(const LevelTextTable &table, std::int32_t id)
{
	const auto found = std::lower_bound(table.begin(), table.end(), id, [](const LevelText &row, std::int32_t value) { return row.id < value; });
	return found != table.end() && found->id == id ? &*found : nullptr;
}
} // namespace xl_compat
