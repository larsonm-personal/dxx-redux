#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <array>
#include <algorithm>
#include "../capacity.h"
#include "xl_reference_object_capacity_constants.h"
#define FLARE_ID      9
#define LEVEL_OBJECTS gameData.objData.nMaxObjects
struct CObject {
	struct {
		int nType = 0, nId = 0, nFlags = 0;
	} info;
	struct {
		struct {
			int nDestroyedObj = -1;
		} explInfo;
	} cType;
	void Die()
	{
		info.nFlags |= OF_SHOULD_BE_DEAD;
	}
};
static std::vector<CObject> objects;
#define OBJECT(index) (&objects[index])
struct {
	struct {
		int nLastObject[2]{};
		int nMaxObjects = 32768;
	} objData;
} gameData;
using pFreeFilter = int32_t (*)(CObject *);
#include "xl_reference_object_capacity.h"
#define Max std::max
#include "xl_reference_object_capacity_init.h"
#undef Max
#include "xl_reference_object_signature.h"
static_assert(MAX_OBJECTS_D2X == 20000, "Reference static helper table changed");
static void require(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
	try {
		require(argc == 3, "Expected native object reclamation and signature samples");
		std::ifstream signatures(argv[2]);
		int seed, actual;
		unsigned signature_count = 0;
		while (signatures >> seed >> actual) {
			int next = seed + 1;
			require(seed > 0 && seed < 2147483646 && actual == reference_signature(next), "Lifetime signature differs from source");
			++signature_count;
		}
		require(signatures.eof() && signature_count == 57, "Missing native lifetime samples");
		std::cout << signature_count << " native lifetime assignments match extracted source\n";
		for (int count = 0; count <= 20001; ++count)
			require(static_cast<unsigned long long>(reference_capacity(count)) == xl_object_capacity_for_count(count), "Authored runtime headroom differs from source");
		std::cout << "20002 authored-count runtime capacities match extracted initialization\n";
		std::ifstream input(argv[1]);
		int count, requested;
		unsigned matched = 0, corrected = 0;
		while (input >> count >> requested) {
			require(count > 0 && count <= DXX_XL_MAX_OBJECTS && requested >= 0 && requested < DXX_XL_MAX_OBJECTS, "Invalid object sample bounds");
			objects.resize(count);
			std::vector<int> original;
			for (auto &object : objects) {
				input >> object.info.nType >> object.info.nId >> object.info.nFlags >> object.cType.explInfo.nDestroyedObj;
				original.push_back(object.info.nFlags);
			}
			int occupied;
			input >> occupied;
			gameData.objData.nLastObject[0] = count - 1;
			// Exact occupancy invokes the documented zero-count correction
			if (occupied != requested) {
				FreeObjectSlots(requested);
				++matched;
			} else ++corrected;
			for (int i = 0; i < count; ++i) {
				int result = -1;
				input >> result;
				require(result == (occupied == requested ? original[i] : objects[i].info.nFlags), "Native reclaim ordering or filter differs from source");
			}
			require(bool(input), "Truncated native object sample");
		}
		require(input.eof() && matched > 1000 && corrected > 0, "Missing native object matrix coverage");
		std::cout << matched << " native reclamation cases matched extracted source; " << corrected << " exact-occupancy corrections passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
