#ifndef DXX_XL_ASE_H
#define DXX_XL_ASE_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
// Keep all authored records, including unknown properties, for semantic validation
struct AseRecord {
	std::string name;
	std::vector<std::string> values;
	std::vector<AseRecord> children;
	bool block = false;
	std::size_t line = 0;
};

namespace ase_detail
{
class Reader
{
	const std::string &text;
	std::size_t position = 0, line = 1, records = 0, tokens = 0;
	[[noreturn]] void fail(const char *message) const
	{
		throw std::runtime_error("ASE line " + std::to_string(line) + ": " + message);
	}
	void spaces()
	{
		while (position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\r')) ++position;
	}
	std::string token()
	{
		if (++tokens > 8000000) fail("token budget exceeded");
		std::string result;
		if (text[position] == '"') {
			++position;
			while (position < text.size() && text[position] != '"') {
				if (text[position] == '\n' || result.size() >= 4096) fail("invalid quoted value");
				// Backslashes in authored Windows texture paths are literal
				result.push_back(text[position++]);
			}
			if (position == text.size()) fail("unterminated quoted value");
			++position;
		} else {
			while (position < text.size()) {
				const char c = text[position];
				if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '{' || c == '}') break;
				if (c == '"' || result.size() >= 4096) fail("invalid token");
				result.push_back(c);
				++position;
			}
		}
		return result;
	}
	std::vector<AseRecord> block(unsigned depth)
	{
		if (depth > 32) fail("block nesting exceeds bounds");
		std::vector<AseRecord> result;
		while (true) {
			spaces();
			if (position == text.size()) {
				if (depth) fail("unterminated block");
				return result;
			}
			if (text[position] == '\n') {
				++line;
				++position;
				continue;
			}
			if (text[position] == '}') {
				if (!depth) fail("unexpected closing brace");
				++position;
				return result;
			}
			if (text[position] != '*') fail("expected record");
			if (++records > 1000000) fail("record budget exceeded");
			AseRecord record;
			record.line = line;
			record.name = token();
			if (record.name.size() < 2) fail("empty record name");
			while (true) {
				spaces();
				if (position == text.size() || text[position] == '\n' || text[position] == '}') break;
				if (text[position] == '{') {
					++position;
					record.block = true;
					record.children = block(depth + 1);
					break;
				}
				record.values.push_back(token());
			}
			result.push_back(std::move(record));
		}
	}

  public:
	explicit Reader(const std::string &source) : text(source) {}
	std::vector<AseRecord> read()
	{
		if (text.empty() || text.size() > 64 * 1024 * 1024 || text.find('\0') != std::string::npos)
			fail("invalid document size or embedded NUL");
		auto result = block(0);
		if (result.empty() || result.front().name != "*3DSMAX_ASCIIEXPORT" ||
		    result.front().block || result.front().values != std::vector<std::string>{ "200" })
			fail("unsupported ASE header");
		return result;
	}
};
} // namespace ase_detail

inline std::vector<AseRecord> read_ase_records(const std::string &source)
{
	return ase_detail::Reader(source).read();
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
