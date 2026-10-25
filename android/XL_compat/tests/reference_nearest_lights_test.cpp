#include "lighting.h"
#include <iostream>
#include <stdexcept>
struct tLightDist {
	int32_t nIndex, nDist;
};
#include "xl_reference_nearest_lights.h"

int main()
{
	try {
		unsigned checked = 0;
		for (int size : { 0, 1, 2, 31, 32, 33, 252, 1024 })
			for (int pattern = 0; pattern < 5; ++pattern) {
				std::vector<xl_compat::GeometryLightDistance> candidates;
				std::vector<tLightDist> reference;
				for (int i = 0; i < size; ++i) {
					const int distance = pattern == 0 ? 0 : pattern == 1 ? i
					                                    : pattern == 2   ? size - i
					                                    : pattern == 3   ? i % 3 - 1
					                                                     : (i * 7919) % 65536;
					candidates.push_back({ i, distance });
					reference.push_back({ i, distance });
				}
				if (size) QSortLightDist(reference.data(), 0, size - 1);
				if (reference.size() > 32) reference.resize(32);
				const auto actual = xl_compat::nearest_geometry_lights(candidates);
				if (actual.size() != reference.size()) throw std::runtime_error("nearest-light capacity differs");
				for (std::size_t i = 0; i < actual.size(); ++i)
					if (actual[i].index != reference[i].nIndex || actual[i].distance != reference[i].nDist)
						throw std::runtime_error("nearest-light selection/order differs from pinned source");
				++checked;
			}
		std::cout << checked << " candidate lists match pinned selection order; visibility not qualified\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
