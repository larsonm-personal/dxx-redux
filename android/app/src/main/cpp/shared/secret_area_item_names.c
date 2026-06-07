#include "secret_area_item_names.h"

const char *secret_area_fallback_powerup_name(int descent2, int id)
{
	if (descent2) {
		switch (id) {
			case 0: return "Extra life";
			case 1: return "Energy";
			case 2: return "Shield boost";
			case 3: return "Laser";
			case 4: return "Blue key";
			case 5: return "Red key";
			case 6: return "Gold key";
			case 10: return "Concussion missile";
			case 11: return "Concussion missile 4-pack";
			case 12: return "Quad lasers";
			case 13: return "Vulcan cannon";
			case 14: return "Spreadfire cannon";
			case 15: return "Plasma cannon";
			case 16: return "Fusion cannon";
			case 17: return "Proximity mine";
			case 18: return "Homing missile";
			case 19: return "Homing missile 4-pack";
			case 20: return "Smart missile";
			case 21: return "Mega missile";
			case 22: return "Vulcan ammo";
			case 23: return "Cloak";
			case 24: return "Turbo";
			case 25: return "Invulnerability";
			case 27: return "Megawow";
			case 28: return "Gauss cannon";
			case 29: return "Helix cannon";
			case 30: return "Phoenix cannon";
			case 31: return "Omega cannon";
			case 32: return "Super laser";
			case 33: return "Full map";
			case 34: return "Energy-to-shield converter";
			case 35: return "Ammo rack";
			case 36: return "Afterburner";
			case 37: return "Headlight";
			case 38: return "Flash missile";
			case 39: return "Flash missile 4-pack";
			case 40: return "Guided missile";
			case 41: return "Guided missile 4-pack";
			case 42: return "Smart mine";
			case 43: return "Mercury missile";
			case 44: return "Mercury missile 4-pack";
			case 45: return "Earthshaker missile";
			case 46: return "Blue flag";
			case 47: return "Red flag";
			default: return 0;
		}
	}
	switch (id) {
		case 0: return "Extra life";
		case 1: return "Energy";
		case 2: return "Shield boost";
		case 3: return "Laser";
		case 4: return "Blue key";
		case 5: return "Red key";
		case 6: return "Gold key";
		case 7: return "Robot radar";
		case 8: return "Powerup radar";
		case 9: return "Full map";
		case 10: return "Concussion missile";
		case 11: return "Concussion missile 4-pack";
		case 12: return "Quad lasers";
		case 13: return "Vulcan cannon";
		case 14: return "Spreadfire cannon";
		case 15: return "Plasma cannon";
		case 16: return "Fusion cannon";
		case 17: return "Proximity mine";
		case 18: return "Homing missile";
		case 19: return "Homing missile 4-pack";
		case 20: return "Smart missile";
		case 21: return "Mega missile";
		case 22: return "Vulcan ammo";
		case 23: return "Cloak";
		case 24: return "Turbo";
		case 25: return "Invulnerability";
		case 26: return "Headlight";
		case 27: return "Megawow";
		default: return 0;
	}
}
