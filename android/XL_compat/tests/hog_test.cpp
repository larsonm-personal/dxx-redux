#include "../hog.h"
#include "hog_fixture.h"
#include <algorithm>
#include <iostream>

static void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}
static std::vector<xl_compat::HogEntry> index(const xl_compat::Bytes &bytes)
{
	return xl_compat::index_hog(bytes.size(), [&](std::uint64_t offset, unsigned char *out, std::size_t count) { std::copy_n(bytes.data() + offset, count, out); });
}
static void rejects(const xl_compat::Bytes &bytes)
{
	try {
		index(bytes);
	} catch (const std::runtime_error &) {
		return;
	}
	throw std::runtime_error("Malformed HOG accepted");
}
int main()
{
	try {
		int cases = 0;
		for (const auto signature : { "DHF", "D2X" })
			for (int name_size : { 1, 12, 13, 14, 254, 255 })
				for (int size : { 1, 2, 256, 4096 }) {
					xl_compat::Bytes bytes(signature, signature + 3);
					const std::string long_name(static_cast<std::size_t>(name_size), 'L');
					xl_test::hog_member(bytes, "empty", {}, false);
					xl_test::hog_member(bytes, long_name, xl_compat::Bytes(size, 77), true);
					xl_test::hog_member(bytes, "ABCDEFGHIJKLM", { 1, 2, 3 }, false);
					const auto entries = index(bytes);
					require(entries.size() == 3 && entries[0].size == 0 && !entries[0].extended && entries[1].extended && entries[1].name == long_name && entries[1].size == static_cast<unsigned>(size) && entries[2].name == "ABCDEFGHIJKLM", "HOG names/lengths differ");
					const auto members = xl_compat::read_hog(bytes);
					require(members.size() == 3 && members[1].name == long_name && members[1].bytes == xl_compat::Bytes(size, 77), "Inspection and index differ");
					for (std::size_t cut = 1; cut < bytes.size(); ++cut) {
						// Exact boundaries are valid shorter archives; every partial record rejects
						if (cut == 3 || cut == entries[0].offset || cut == entries[1].offset + entries[1].size) continue;
						rejects(xl_compat::Bytes(bytes.begin(), bytes.begin() + cut));
					}
					++cases;
				}
		for (const auto name : { "", ".", "..", "a/b", "a\\b", "a:b", "a\nb" }) {
			xl_compat::Bytes bytes{ 'D', '2', 'X' };
			xl_test::hog_member(bytes, "valid", { 1 }, true);
			std::fill(bytes.begin() + 20, bytes.begin() + 276, 0);
			std::copy(name, name + std::char_traits<char>::length(name), bytes.begin() + 20);
			rejects(bytes);
		}
		xl_compat::Bytes bytes{ 'D', '2', 'X' };
		xl_test::hog_member(bytes, "Long-Name.txt", { 1 }, true);
		auto bad = bytes;
		std::fill(bad.begin() + 20, bad.begin() + 276, 'a');
		rejects(bad);
		bad = bytes;
		bad[16] = bad[17] = bad[18] = 0;
		bad[19] = 128;
		rejects(bad);
		xl_test::hog_member(bytes, "long-name.TXT", { 2 }, true);
		rejects(bytes);
		bytes[0] = 'X';
		rejects(bytes);
		std::cout << cases << " mixed HOG index layouts, all truncations and malformed names/sizes passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
