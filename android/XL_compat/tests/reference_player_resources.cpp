// Actual XL leaf/Set/Update code, with default ship/storage/death adapters
#include "../player_resources.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <cmath>
#include <stdexcept>
using fix = std::int32_t;
int FRound(float value)
{
	return static_cast<int>(std::round(value));
}
#define I2X(v)           ((v) * 65536)
#define X2F(v)           (float(v) / 65536.0f)
#define TT_SHIELD_DAMAGE 17
#define TT_ENERGY_DRAIN  18
#define IsMultiGame      0
template <class T>
T Clamp(T value, T low, T high)
{
	return std::clamp(value, low, high);
}
struct CShipEnergy {
	fix *m_current = nullptr;
	struct {
		void Start() {}
		void Setup(int) {}
	} m_toRecharge;
	fix Get() const
	{
		return *m_current;
	}
	fix Max() const
	{
		return I2X(200);
	}
	float Scale() const
	{
		return 1.0f;
	}
	void SetRechargeDelay(int) {}
	int RechargeDelay(int)
	{
		return 0;
	}
	bool Set(fix, bool = false);
#include "xl_reference_resource_update.h"
};
struct {
	int nRechargeDelay = 0;
} extraGameInfo[2];
struct {
	fix shields = 0, energy = 0;
	CShipEnergy shield_value, energy_value;
	int nObject = 0;
	fix Shield() const
	{
		return shields;
	}
	fix Energy() const
	{
		return energy;
	}
	void UpdateShield(fix delta)
	{
		shield_value.Update(delta);
	}
	void UpdateEnergy(fix delta)
	{
		energy_value.Update(delta);
	}
} local_player;
#define LOCALPLAYER local_player
struct {
	struct {
		bool bPlayerIsDead = false, bD1Mission = false;
	} app;
} gameStates;
struct CTrigger {
	fix value = 0;
	fix GetValue() const
	{
		return value;
	}
} action;
#define GEOTRIGGER(index) (&action)
int dummy_object = 0, deaths = 0;
#define OBJECT(index) (&dummy_object)
void StartPlayerDeathSequence(int *)
{
	++deaths;
	gameStates.app.bPlayerIsDead = true;
}
#include "xl_reference_player_resources.h"
static void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
int main()
{
	try {
		int cases = 0;
		for (int type : { 17, 18 })
			for (int current : { -65537, -1, 0, 1, 65535, 65536, 3276801, 6553600, 13107200, 32768000 })
				for (int value : { -65536000, -3276800, -16777219, -16777217, -65537, -65536, -65535, -1, 0, 1, 65535, 65536, 196608, 327680, 6553600, 13172736, 16777215, 16777216, 16777217, 16777219, 65536000 })
					for (bool dead : { false, true }) {
						local_player.shields = local_player.energy = current;
						local_player.shield_value.m_current = &local_player.shields;
						local_player.energy_value.m_current = &local_player.energy;
						action.value = value;
						deaths = 0;
						gameStates.app.bPlayerIsDead = dead;
						reference_resource_action(type, 0);
						const auto expected = dead ? current : xl_compat::player_resource_after(current, value, type == 18);
						require((type == 18 ? local_player.energy : local_player.shields) == expected, "Production resource result differs from pinned leaf/Set/Update");
						require(deaths == (!dead && type == 17 && expected < 0 ? 1 : 0), "Resource death boundary differs");
						++cases;
					}
		// Source narrowing/addition is undefined at these extremes; defined extension
		for (int current : { 0, 6553600, 13107200 }) {
			require(xl_compat::player_resource_after(current, INT32_MAX, false) == -1, "Extreme shield damage did not clamp");
			require(xl_compat::player_resource_after(current, INT32_MIN, false) == 13107200, "Extreme shield replenishment did not clamp");
			if (current) {
				require(xl_compat::player_resource_after(current, INT32_MAX, true) == -1, "Extreme energy drain did not clamp");
				require(xl_compat::player_resource_after(current, INT32_MIN, true) == 13107200, "Extreme energy replenishment did not clamp");
			}
		}
		std::cout << "Pinned XL player resources: " << cases << " leaf/Set/Update cases and defined extreme controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
