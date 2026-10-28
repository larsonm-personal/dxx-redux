#include "../projectile_history.h"
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
int main()
{
	try {
		xl_projectile_state first{}, second{};
		for (int victim = 0; victim < 20000; ++victim) {
			require(xl_compat::projectile_add_hit(first, victim) == 1, "New victim was suppressed");
			const int count = (std::min) (victim + 1, XL_PROJECTILE_HITS);
			require(first.count == count, "Projectile history exceeded its bound or lost an entry");
			for (int i = 0; i < count; ++i) {
				const int retained = victim - count + 1 + i;
				require(first.victims[i] == retained && xl_compat::projectile_has_hit(first, retained), "Projectile lost the latest distinct victim order");
				require(xl_compat::projectile_add_hit(first, retained) == -1 && first.victims[i] == retained, "Duplicate contact changed hit order");
			}
			if (victim >= XL_PROJECTILE_HITS) require(!xl_compat::projectile_has_hit(first, victim - XL_PROJECTILE_HITS), "Oldest victim was not evicted");
			require(!xl_compat::projectile_has_hit(second, victim) && second.count == 0, "Separate projectile acquired hit history");
		}
		require(xl_compat::projectile_add_hit(first, 0) == 1 && first.victims[19] == 0 && !xl_compat::projectile_has_hit(first, 19980), "Evicted victim could not reenter the bounded history");
		first = {};
		require(first.count == 0 && !xl_compat::projectile_has_hit(first, 0) && xl_compat::projectile_add_hit(first, 0) == 1, "Fresh projectile history retained old victims");
		std::cout << "Public XL projectile history retention, duplicates, eviction and independent owners passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
