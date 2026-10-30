#include "named_sound.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace xl_compat
{
std::string named_sound_filename(const std::string &authored)
{
	static constexpr std::array<std::string_view, 43> addons{
		"missileflight-small.wav", "missileflight-big.wav", "vulcan-firing.wav", "gauss-firing.wav",
		"gatling-speedup.wav", "flareburning.wav", "lowping.wav", "highping.wav", "lightning.wav",
		"slowdown.wav", "speedup.wav", "airbubbles.wav", "zoom1.wav", "zoom2.wav", "headlight.wav",
		"boiling-lava.wav", "busy-machine.wav", "computer.wav", "deep-hum.wav", "dripping-water.wav",
		"dripping-water-2.wav", "dripping-water-3.wav", "earthquake.wav", "energy.wav", "falling-rocks.wav",
		"falling-rocks-2.wav", "fast-fan.wav", "flowing-lava.wav", "flowing-water.wav", "insects.wav",
		"machine-gear.wav", "mighty-machine.wav", "static-buzz.wav", "steam.wav", "teleporter.wav",
		"waterfall.wav", "fire.wav", "jet-engine.wav", "nuke-explosion-short.wav", "scrape.wav",
		"scrape2.wav", "scrape3.wav", "scrape4.wav"
	};
	if (authored.empty() || authored.size() > 255) throw std::runtime_error("Named sound has an empty or oversized filename");
	if (authored[0] >= '0' && authored[0] <= '9') {
		unsigned index = 0;
		for (const char c : authored) {
			if (c < '0' || c > '9') break;
			index = index * 10 + unsigned(c - '0');
			if (index >= addons.size()) throw std::runtime_error("Named sound addon index is outside the source table");
		}
		return std::string(addons[index]);
	}
	for (const auto name : addons)
		if (name.substr(0, authored.size()) == authored) return std::string(name);
	if (authored == "." || authored == ".." || authored.find_first_of("/\\:") != std::string::npos)
		throw std::runtime_error("Named sound requires a filename within the selected resource scope");
	for (const unsigned char c : authored)
		if (c < 32 || c == 127) throw std::runtime_error("Named sound filename contains a control character");
	return authored;
}

std::vector<std::string> named_sound_paths(const std::string &mission, const std::string &authored, int ordinal)
{
	if (mission.empty() || mission.size() > 255 || mission == "." || mission == ".." ||
	    mission.find_first_of("/\\:") != std::string::npos || ordinal < -9999 || ordinal > 9999)
		throw std::runtime_error("Invalid XL sound mission identity or level ordinal");
	for (const unsigned char c : mission)
		if (c < 32 || c == 127) throw std::runtime_error("XL sound mission identity contains a control character");
	const auto filename = named_sound_filename(authored);
	std::vector<std::string> paths{ filename };
	const auto root = "mods/" + mission + "/sounds/";
	if (ordinal) {
		char folder[32];
		std::snprintf(folder, sizeof(folder), "%s%02d/", ordinal < 0 ? "slevel" : "level", ordinal < 0 ? -ordinal : ordinal);
		paths.push_back(root + folder + filename);
	}
	paths.push_back(root + filename);
	return paths;
}
} // namespace xl_compat
