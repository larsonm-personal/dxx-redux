#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace gun_reference
{
using fix = int32_t;
#define ENTER(...)
#define RETVAL(value) return (value);
template <typename T>
void Swap(T &a, T &b)
{
	std::swap(a, b);
}
struct CAngleVector {
	struct {
		struct {
			int16_t p, b, h;
		} coord;
	} v;
};
struct CFixVector {
	struct {
		struct {
			fix x, y, z;
		} coord;
	} v;
	void Set(fix x, fix y, fix z)
	{
		v.coord = { x, y, z };
	}
	static const fix Dot(const CFixVector &, const CFixVector &);
	const CFixVector &operator+=(const CFixVector &);
	const CFixVector operator+(const CFixVector &) const;
};
struct CFixMatrix {
	union {
		struct {
			CFixVector r, u, f;
		} dir;
		fix vec[9];
	} m;
	static const CFixMatrix Create(fix, fix, fix, fix, fix, fix);
	static const CFixMatrix Create(const CAngleVector &);
	static CFixMatrix &Transpose(CFixMatrix &);
	CFixVector operator*(const CFixVector &);
};
struct {
	struct {
		int nMathFormat = 2;
	} render;
} options;
auto *gameOpts = &options;
struct tRobotInfo {
	int gunSubModels[8]{};
	CFixVector gunPoints[8]{};
} robot;
struct CPolyModel {
	struct {
		CFixVector offsets[10]{};
		int parents[10]{};
	} parts;
	auto &SubModels()
	{
		return parts;
	}
} models[1];
struct {
	struct {
		CPolyModel *polyModels[1]{ models };
	} modelData;
} gameData;
struct CObject {
	struct {
		struct {
			CFixVector vPos{};
		} position;
	} info;
	struct {
		struct {
			CAngleVector animAngles[10]{};
		} polyObjInfo;
	} rType;
	CFixMatrix orientation{}, view{};
	int ModelId() const
	{
		return 0;
	}
	CFixMatrix *View(int)
	{
		view = orientation;
		CFixMatrix::Transpose(view);
		return &view;
	}
};
tRobotInfo *ROBOTINFO(CObject *)
{
	return &robot;
}
// Original HAM/HXM point array; native sampling rejects model marker overrides
CFixVector *GetGunPoints(CObject *, int)
{
	return robot.gunPoints;
}
#include "xl_reference_gun_point.h"
} // namespace gun_reference

void compare_gun_points(const char *path)
{
	using namespace gun_reference;
	std::ifstream input(path);
	std::string token;
	if (!(input >> token) || token != "xl-gun-points-v1") throw std::runtime_error("Invalid native gun receipt");
	unsigned cases = 0, mismatches = 0, fixed_headers = 0;
	int64_t maximum_error = 0;
	const auto vector = [&](CFixVector &v) { input >> v.v.coord.x >> v.v.coord.y >> v.v.coord.z; };
	while (input >> token && token == "case") {
		int actor, id, model, state, gun, count, part;
		input >> actor >> id >> model >> state >> gun >> count >> part;
		if (gun < 0 || gun >= 8 || part < 0 || part >= 10) throw std::runtime_error("Invalid gun receipt indices");
		CObject object{};
		robot.gunSubModels[gun] = part;
		vector(robot.gunPoints[gun]);
		vector(object.info.position.vPos);
		vector(object.orientation.m.dir.r);
		vector(object.orientation.m.dir.u);
		vector(object.orientation.m.dir.f);
		for (int i = 0; i < 10; ++i) {
			input >> models[0].parts.parents[i];
			vector(models[0].parts.offsets[i]);
			auto &a = object.rType.polyObjInfo.animAngles[i].v.coord;
			input >> a.p >> a.b >> a.h;
		}
		CFixVector native{}, source{};
		vector(native);
		if (!input) throw std::runtime_error("Truncated native gun receipt");
		int steps = 0;
		for (int i = part; i; i = models[0].parts.parents[i])
			if (i < 0 || i >= 10 || ++steps >= 10) throw std::runtime_error("Invalid native gun parent chain");
		if (!CalcGunPoint(&source, &object, gun)) throw std::runtime_error("Pinned gun transform rejected original definitions");
		const int64_t error = std::max({ std::abs(int64_t(native.v.coord.x) - source.v.coord.x),
		                                 std::abs(int64_t(native.v.coord.y) - source.v.coord.y),
		                                 std::abs(int64_t(native.v.coord.z) - source.v.coord.z) });
		if (error && mismatches++ < 8)
			std::cerr << "Gun mismatch actor=" << actor << " robot=" << id << " model=" << model << " state=" << state
			          << " gun=" << gun << " max_axis_error=" << error << '\n';
		maximum_error = std::max(maximum_error, error);
		fixed_headers += part >= count;
		++cases;
	}
	unsigned declared = 0;
	if (token != "end" || !(input >> declared) || declared != cases || !cases || !fixed_headers || (input >> token))
		throw std::runtime_error("Incomplete native gun receipt or missing fixed-header cases");
	std::cout << "Pinned gun transforms: " << cases << " cases, " << fixed_headers << " fixed-header cases, "
	          << mismatches << " mismatches, maximum fixed axis error " << maximum_error << '\n';
	if (mismatches) throw std::runtime_error("Native gun positions differ from pinned default XL math");
}
