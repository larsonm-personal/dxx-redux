#include "../app/src/main/cpp/headless/headless_diagnostics.h"

#include <fstream>
#include <iterator>

int main()
{
	if (!freopen("test_headless_diagnostics.log", "w", stderr))
		return 1;
	char unknown[] = "Warning: can't convert unknown descent 1 texture #1056.";
	char another[] = "Warning: can't convert unknown descent 1 texture #1057.";
	char ordinary[] = "Warning: unrelated diagnostic";
	for (int i = 0; i < 20; ++i)
		headless_warning(unknown);
	headless_warning(another);
	headless_warning(ordinary);
	headless_warning(ordinary);
	headless_error("Fatal diagnostic");
	fclose(stderr);
	std::ifstream file("test_headless_diagnostics.log");
	const std::string actual((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const std::string expected = std::string(unknown) + "\n" + another + "\n" +
	                             ordinary + "\n" + ordinary + "\nFatal diagnostic\n";
	if (actual != expected) {
		printf("Unexpected headless diagnostics output\n");
		return 1;
	}
	return 0;
}
