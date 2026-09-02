#include <iostream>
#include <string>

extern "C" const char *dxx_level_metadata_analyze_json(const char *request_text);

int main()
{
	std::ios::sync_with_stdio(false);
	std::string request;
	while (std::getline(std::cin, request)) {
		if (request.empty())
			continue;
		const char *result = dxx_level_metadata_analyze_json(request.c_str());
		std::cout << "DXXMETA\t" << (result ? result : "{\"status\":\"failed\",\"problems\":[\"worker returned no result\"]}") << '\n';
		std::cout.flush();
	}
	return 0;
}
