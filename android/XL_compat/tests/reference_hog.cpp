#include "../hog.h"
#include "hog_fixture.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <cstring>
#define INTEL_INT(x) (x)
class CLevelHeader
{
  public:
	int32_t m_size = 0;
	char m_name[13]{}, m_longName[256]{};
	int32_t m_bExtended = 0;
	int32_t Read(FILE *fp);
};
#include "xl_reference_hog.h"
int main()
{
	try {
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
