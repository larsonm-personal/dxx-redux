#include "../projectile_history.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
// Only the external storage and engine primitives are supplied by the harness
#include "xl_reference_projectile_constants.h"
struct CObject {
	int index = 0;
	struct {
		struct {
			int16_t nLastHitObj = 0;
		} laserInfo;
	} cType;
	struct {
		struct {
			int flags = 0;
		} physInfo;
	} mType;
	struct {
		int xShield = 0, nType = 0, nFlags = 0, nId = 0;
	} info;
	int Index() const
	{
		return index;
	}
	bool IsMine() const
	{
		return info.nId == 16 || info.nId == 38 || info.nId == 51 || info.nId == 53;
	}
	void Die()
	{
		info.nFlags |= OF_SHOULD_BE_DEAD;
	}
	void SetShield(int shield)
	{
		info.xShield = shield;
	}
	void MaybeKillWeapon(CObject *);
};
struct {
	struct {
		int16_t nHitObjects[40]{};
	} objData;
} gameData;
#define ENTER(a, b)
#define RETVAL(value) return (value);
#define RETURN        return;
#include "xl_reference_projectile.h"
static_assert(MAX_HIT_OBJECTS == XL_PROJECTILE_HITS, "Projectile history capacity differs");
static void require(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
	try {
		unsigned samples = 0;
		for (int base : { 0, 127, 980, 19980, 32747 }) {
			CObject source;
			xl_projectile_state actual{};
			for (int hit = 0; hit < MAX_HIT_OBJECTS; ++hit) {
				const int victim = base + hit;
				require(AddHitObject(&source, int16_t(victim)) == xl_compat::projectile_add_hit(actual, victim), "New hit differs from pinned XL");
				for (int prior = 0; prior <= hit; ++prior) {
					require(AddHitObject(&source, int16_t(base + prior)) == xl_compat::projectile_add_hit(actual, base + prior), "Duplicate hit differs from pinned XL");
					require(bool(FindHitObject(&source, int16_t(base + prior))) == xl_compat::projectile_has_hit(actual, base + prior), "Hit lookup differs from pinned XL");
					++samples;
				}
				require(source.cType.laserInfo.nLastHitObj == actual.count, "Hit count differs from pinned XL");
				for (int i = 0; i < actual.count; ++i) require(gameData.objData.nHitObjects[i] == actual.victims[i], "Hit order differs from pinned XL");
				++samples;
			}
		}
		// Do not execute the source's undefined overlapping memcpy. Exercise the
		// documented FIFO correction against an independently maintained sequence
		xl_projectile_state actual{};
		std::vector<int> expected;
		for (int i = 0; i < 500; ++i) {
			const int victim = (i * 17) % 37;
			const auto found = std::find(expected.begin(), expected.end(), victim);
			const bool duplicate = found != expected.end();
			require(xl_compat::projectile_add_hit(actual, victim) == (duplicate ? -1 : 1), "Corrected bounded history returned wrong hit decision");
			if (!duplicate) {
				if (expected.size() == XL_PROJECTILE_HITS) expected.erase(expected.begin());
				expected.push_back(victim);
			}
			require(std::vector<int>(actual.victims, actual.victims + actual.count) == expected, "Corrected bounded history lost FIFO ordering");
			++samples;
		}
		if (argc > 1) {
			std::ifstream input(argv[1]);
			require(bool(input), "Cannot open native projectile impact samples");
			int id, persistent, shield, type, other_persistent, other_shield, result, dead;
			unsigned impacts = 0;
			while (input >> id >> persistent >> shield >> type >> other_persistent >> other_shield >> result >> dead) {
				CObject weapon, other;
				weapon.info.nId = id;
				weapon.info.xShield = shield;
				weapon.mType.physInfo.flags = persistent ? PF_PERSISTENT : 0;
				other.info.nType = type;
				other.info.xShield = other_shield;
				other.mType.physInfo.flags = other_persistent ? PF_PERSISTENT : 0;
				weapon.MaybeKillWeapon(&other);
				require(weapon.info.xShield == result && bool(weapon.info.nFlags & OF_SHOULD_BE_DEAD) == bool(dead), "Native impact differs from extracted MaybeKillWeapon");
				++impacts;
			}
			require(input.eof() && impacts > 100, "Missing or malformed native impact matrix");
			std::cout << impacts << " actual native projectile impacts matched extracted source\n";
		}
		std::cout << samples << " source history and explicit FIFO-correction controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
