#include "scoped_texture.h"
#include <iostream>
#include <stdexcept>
static void require(bool value)
{
	if (!value) throw std::runtime_error("Scoped static texture path contract failed");
}
int main()
{
	using xl_compat::static_mission_texture_path;
	require(static_mission_texture_path("missions/single/boilpnt.hog", "ceil002") == "mods/boilpnt/textures/ceil002.tga");
	require(static_mission_texture_path("missions\\BOILPNT.HOG", "CEIL002") == "mods/BOILPNT/textures/CEIL002.tga");
	for (const char *name : { "", "..", "../ceil002", "x/ceil002", "x\\ceil002", "C:ceil002", "door#0", "bad\n" })
		require(static_mission_texture_path("boilpnt.hog", name).empty());
	for (const char *hog : { "", ".hog", "archive.zip", "..hog", "C:bad.hog" })
		require(static_mission_texture_path(hog, "ceil002").empty());
	require(static_mission_texture_path(nullptr, "ceil002").empty());
	require(static_mission_texture_path("boilpnt.hog", nullptr).empty());
	require(static_mission_texture_path(std::string(4096, 'a').c_str(), "ceil002").empty());
	require(static_mission_texture_path("boilpnt.hog", std::string(128, 'a').c_str()).empty());
	std::cout << "Static mission texture: selected HOG identity, exact TGA and bounded components passed\n";
}
