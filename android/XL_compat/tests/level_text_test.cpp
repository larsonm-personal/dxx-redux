#include "level_text.h"
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}
void rejected(const std::string &bytes)
{
	try {
		xl_compat::decode_level_text(bytes);
	} catch (const std::runtime_error &) {
		return;
	}
	throw std::runtime_error("Invalid level text was accepted");
}
} // namespace

int main()
{
	try {
		const auto table = xl_compat::decode_level_text("9:Later\\nSecond line\r\n1:First\r\n5:voice.wav");
		require(table.size() == 3 && table[0].id == 1 && table[1].id == 5 && table[2].id == 9, "Message IDs were not retained and sorted");
		require(table[0].text == "First" && table[1].text == "voice.wav" && table[2].text == "Later\nSecond line" && table[2].lines == 2, "Message text or escaped lines changed");
		require(xl_compat::find_level_text(table, 5) == &table[1] && !xl_compat::find_level_text(table, 6), "Message lookup changed identity");
		for (const std::string bytes : { "", "\n", "\r\n", "0:ignored", "# comment\n" })
			require(xl_compat::decode_level_text(bytes).empty(), "Empty/ignored text created a message");
		const auto cr = xl_compat::decode_level_text("2:second\r1:first");
		require(cr.size() == 2 && cr[0].text == "first" && cr[1].text == "second", "Bounded CR-only lines failed");
		rejected("1:first\n1:duplicate");
		rejected("2147483648:overflow");
		rejected("-2147483649:overflow");
		rejected("123");
		rejected(std::string("1:hello\0hidden", 14));
		rejected(std::string(1024 * 1024 + 1, 'x'));
		std::cout << "XL level text: sorted identities, exact text, lookup, empty/CR-only safety and malformed inputs passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
