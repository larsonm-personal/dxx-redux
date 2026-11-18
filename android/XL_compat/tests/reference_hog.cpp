#include "../hog.h"
#include "hog_fixture.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <cstring>
#include <set>
#ifdef _WIN32
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif
#define INTEL_INT(x) (x)
class CLevelHeader
{
  public:
	int32_t m_size = 0;
	char m_name[13]{}, m_longName[256]{};
	int32_t m_bExtended = 0;
	int32_t Read(FILE *fp);
};
struct tHogFile {
	char name[256]{};
	int32_t offset, length;
};
class CHogFile
{
  public:
	void QuickSort(tHogFile *, int32_t, int32_t);
	tHogFile *BinSearch(tHogFile *, int32_t, const char *);
};
#include "xl_reference_hog.h"

static std::size_t compare_lookup(FILE *file)
{
	if (std::fseek(file, 0, SEEK_END)) throw std::runtime_error("Cannot size archive");
	const long size = std::ftell(file);
	if (size < 3 || size > INT32_MAX || std::fseek(file, 3, SEEK_SET)) throw std::runtime_error("Invalid reference archive size");
	std::vector<tHogFile> source;
	std::set<std::string> names;
	while (std::ftell(file) < size) {
		CLevelHeader header;
		if (!header.Read(file)) throw std::runtime_error("Reference header read failed");
		tHogFile record{};
		const char *name = header.m_bExtended ? header.m_longName : header.m_name;
		std::memcpy(record.name, name, header.m_bExtended ? sizeof(header.m_longName) : sizeof(header.m_name));
		record.offset = static_cast<int32_t>(std::ftell(file));
		record.length = header.m_bExtended ? -header.m_size : header.m_size;
		if (record.offset < 0 || record.length < 0 || record.length > size - record.offset || !std::memchr(record.name, 0, sizeof(record.name)) || std::fseek(file, record.length, SEEK_CUR))
			throw std::runtime_error("Invalid reference member span");
		source.push_back(record);
		names.insert(xl_compat::hog_key(record.name));
	}
	CHogFile reference;
	if (!source.empty()) reference.QuickSort(source.data(), 0, static_cast<int32_t>(source.size()) - 1);
	const auto selected = xl_compat::index_hog(size, [file](std::uint64_t offset, unsigned char *out, std::size_t count) {
		if (std::fseek(file, static_cast<long>(offset), SEEK_SET) || std::fread(out, 1, count, file) != count) throw std::runtime_error("Cannot read indexed header");
	});
	std::uint64_t end = 0;
	for (const auto &entry : selected) {
		const auto *match = reference.BinSearch(source.data(), static_cast<int32_t>(source.size()), entry.name.c_str());
		if (!match || entry.name != match->name || entry.offset != static_cast<std::uint64_t>(match->offset) || entry.size != static_cast<std::uint64_t>(match->length) || entry.offset < end || !names.erase(xl_compat::hog_key(entry.name)))
			throw std::runtime_error("Reference-selected span differs for " + entry.name);
		end = entry.offset + entry.size;
	}
	if (!names.empty()) throw std::runtime_error("Omitted a source-selected name");
	return selected.size();
}

int main(int argc, char **argv)
{
	try {
		std::size_t lookups = 0;
		for (int i = 1; i < argc; ++i) {
			std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(argv[i], "rb"), std::fclose);
			if (!file) throw std::runtime_error("Cannot open input HOG");
			lookups += compare_lookup(file.get());
		}
		for (unsigned count = 1; count <= 32; ++count) {
			xl_compat::Bytes bytes{ 'D', '2', 'X' };
			for (unsigned i = 0; i < count; ++i) {
				const std::string name = i % 4 == 1 ? "Level.rl2" : i % 4 == 3 ? "LEVEL.RL2"
				                                                : i % 4 == 2   ? "duplicate long texture name.tga"
				                                                               : "a";
				xl_test::hog_member(bytes, name, { static_cast<unsigned char>(i % 5) }, i % 4 != 0);
			}
			std::unique_ptr<FILE, decltype(&std::fclose)> file(std::tmpfile(), std::fclose);
			if (!file || std::fwrite(bytes.data(), 1, bytes.size(), file.get()) != bytes.size()) throw std::runtime_error("Cannot write lookup fixture");
			lookups += compare_lookup(file.get());
		}
		std::cout << lookups << " source-selected names and physical spans matched\n";
		int cases = 0;
		for (const auto signature : { "DHF", "D2X" })
			for (int name_size : { 1, 12, 13, 14, 254, 255 })
				for (int size : { 1, 2, 256, 4096 }) {
					xl_compat::Bytes bytes(signature, signature + 3);
					xl_test::hog_member(bytes, "short", {}, false);
					xl_test::hog_member(bytes, std::string(name_size, 'L'), xl_compat::Bytes(size, 77), true);
					xl_test::hog_member(bytes, "after", { 1, 2, 3 }, false);
					const auto entries = xl_compat::index_hog(bytes.size(), [&](std::uint64_t at, unsigned char *out, std::size_t count) { std::copy_n(bytes.data() + at, count, out); });
					const auto close = [](FILE *value) { std::fclose(value); };
					std::unique_ptr<FILE, decltype(close)> file(std::tmpfile(), close);
					if (!file || std::fwrite(bytes.data(), 1, bytes.size(), file.get()) != bytes.size() || std::fseek(file.get(), 3, SEEK_SET)) throw std::runtime_error("Cannot prepare HOG source sample");
					for (const auto &entry : entries) {
						CLevelHeader source;
						if (!source.Read(file.get()) || entry.name != (source.m_bExtended ? source.m_longName : source.m_name) || entry.size != static_cast<std::uint64_t>(source.m_bExtended ? -source.m_size : source.m_size) || entry.offset != static_cast<std::uint64_t>(std::ftell(file.get())) || entry.extended != (source.m_bExtended != 0)) throw std::runtime_error("Source HOG name/size/offset differs");
						if (std::fseek(file.get(), static_cast<long>(entry.size), SEEK_CUR)) throw std::runtime_error("Cannot skip source payload");
						++cases;
					}
				}
		std::cout << cases << " source HOG names, signed sizes, modes and offsets matched\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
