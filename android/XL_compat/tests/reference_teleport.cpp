// Storage adapters for extracted XL code; no Redux math linked into the oracle
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using fix = std::int32_t;
using fixang = std::int16_t;
#define I2X(x)       ((x) * 65536)
#define F2X(x)       fix((x) * 65536)
#define FixMul(a, b) fix(double(b) / 65536.0 * double(a))
#define FixDiv(a, b) fix((b) ? double(a) / double(b) * 65536.0 : 1)
constexpr double PI = 3.141592653589793240;
constexpr int MAX_ORIENT_STEPS = 10;
struct Options {
	struct {
		int nMathFormat = 2;
	} render;
} options;
Options *gameOpts = &options;
struct tQuadInt {
	std::uint32_t low;
	std::int32_t high;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("legacy math reached");
}
unsigned QuadSqrt(unsigned, int)
{
	throw std::runtime_error("legacy math reached");
}
fixang FixASin(fix);
fixang FixACos(fix);
fixang FixAtan2(fix, fix);
struct CAngleVector {
	struct {
		struct {
			fixang p{}, b{}, h{};
		} coord;
	} v;
	CAngleVector &operator-=(const CAngleVector &a)
	{
		v.coord.p -= a.v.coord.p;
		v.coord.b -= a.v.coord.b;
		v.coord.h -= a.v.coord.h;
		return *this;
	}
	CAngleVector &operator/=(int)
	{
		throw std::runtime_error("speed boost branch reached");
	}
	CAngleVector &operator+=(const CAngleVector &)
	{
		throw std::runtime_error("speed boost branch reached");
	}
	CAngleVector operator-(const CAngleVector &) const
	{
		throw std::runtime_error("speed boost branch reached");
	}
	CAngleVector operator/(int) const
	{
		throw std::runtime_error("speed boost branch reached");
	}
};
struct CFixVector {
	struct {
		struct {
			fix x{}, y{}, z{};
		} coord;
	} v;
	void Set(fix x, fix y, fix z)
	{
		v.coord = { x, y, z };
	}
	void Neg()
	{
		v.coord.x = -v.coord.x;
		v.coord.y = -v.coord.y;
		v.coord.z = -v.coord.z;
	}
	CFixVector operator-() const
	{
		auto result = *this;
		result.Neg();
		return result;
	}
	fix Mag() const;
	static const fix Normalize(CFixVector &);
	static const fix Dot(const CFixVector &, const CFixVector &);
	const CAngleVector ToAnglesVecNorm() const;
	const CAngleVector ToAnglesVec() const;
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector r, u, f;
		} dir;
	} m;
	static const CFixMatrix Create(fix, fix, fix, fix, fix, fix);
	static const CFixMatrix Create(const CAngleVector &);
	CFixVector operator*(const CFixVector &);
	CAngleVector ComputeAngles() const
	{
		throw std::runtime_error("speed boost branch reached");
	}
};
struct tObjTransformation {
	CFixMatrix mOrient;
	CFixVector vPos;
};
struct CObject {
	struct {
		tObjTransformation position;
	} info;
	struct {
		struct {
			CFixVector velocity;
		} physInfo;
	} mType;
} object;
struct Segment {
	CFixVector normal, center;
	Segment *Side(int)
	{
		return this;
	}
	CFixVector &Normal(int)
	{
		return normal;
	}
	CFixVector Center()
	{
		return center;
	}
} segment;
CObject *OBJECT(int)
{
	return &object;
}
Segment *SEGMENT(int)
{
	return &segment;
}
struct {
	struct {
		CFixVector vTgtDir;
	} gameplay;
} gameStates;
#include "xl_reference_teleport.h"

int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected runtime transform samples");
		std::ifstream input(argv[1]);
		unsigned count = 0;
		fix values[18];
		while (input >> values[0]) {
			for (int i = 1; i < 18; ++i)
				if (!(input >> values[i])) throw std::runtime_error("Truncated transform sample");
			segment.normal.Set(values[0], values[1], values[2]);
			object.mType.physInfo.velocity.Set(values[3], values[4], values[5]);
			TriggerSetObjOrient(0, 0, 0, 1, 0);
			int at = 6;
			for (const auto *vector : { &object.info.position.mOrient.m.dir.r, &object.info.position.mOrient.m.dir.u, &object.info.position.mOrient.m.dir.f, &object.mType.physInfo.velocity }) {
				for (const fix value : { vector->v.coord.x, vector->v.coord.y, vector->v.coord.z })
					if (value != values[at++]) throw std::runtime_error("Teleport transform differs at sample " + std::to_string(count) + " field " + std::to_string(at - 1) + " expected " + std::to_string(value) + " actual " + std::to_string(values[at - 1]));
			}
			++count;
		}
		if (count < 200) throw std::runtime_error("Insufficient transform samples");
		std::cout << count << " extracted-reference teleport transforms passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
