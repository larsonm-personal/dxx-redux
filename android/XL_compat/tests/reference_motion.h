#ifndef XL_REFERENCE_MOTION_H
#define XL_REFERENCE_MOTION_H
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
		int bScrape = 0;
	} sound;
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
	CAngleVector &operator/=(int divisor)
	{
		v.coord.p /= divisor;
		v.coord.b /= divisor;
		v.coord.h /= divisor;
		return *this;
	}
	CAngleVector &operator+=(const CAngleVector &value)
	{
		v.coord.p += value.v.coord.p;
		v.coord.b += value.v.coord.b;
		v.coord.h += value.v.coord.h;
		return *this;
	}
	CAngleVector operator-(const CAngleVector &value) const
	{
		auto result = *this;
		result -= value;
		return result;
	}
	CAngleVector operator/(int divisor) const
	{
		auto result = *this;
		result /= divisor;
		return result;
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
	void SetZero()
	{
		Set(0, 0, 0);
	}
	bool IsZero() const
	{
		return !(v.coord.x || v.coord.y || v.coord.z);
	}
	CFixVector &operator+=(const CFixVector &a)
	{
		v.coord.x += a.v.coord.x;
		v.coord.y += a.v.coord.y;
		v.coord.z += a.v.coord.z;
		return *this;
	}
	CFixVector &operator-=(const CFixVector &a)
	{
		v.coord.x -= a.v.coord.x;
		v.coord.y -= a.v.coord.y;
		v.coord.z -= a.v.coord.z;
		return *this;
	}
	CFixVector operator+(const CFixVector &a) const
	{
		auto result = *this;
		result += a;
		return result;
	}
	CFixVector operator-(const CFixVector &a) const
	{
		auto result = *this;
		result -= a;
		return result;
	}
	CFixVector &operator*=(fix amount)
	{
		v.coord.x = FixMul(v.coord.x, amount);
		v.coord.y = FixMul(v.coord.y, amount);
		v.coord.z = FixMul(v.coord.z, amount);
		return *this;
	}
	CFixVector operator*(fix amount) const
	{
		auto result = *this;
		result *= amount;
		return result;
	}
	fix Mag() const;
	static const fix Normalize(CFixVector &);
	static const CFixVector Random();
	CFixVector &operator*=(float value)
	{
		v.coord.x = fix(float(v.coord.x) * value);
		v.coord.y = fix(float(v.coord.y) * value);
		v.coord.z = fix(float(v.coord.z) * value);
		return *this;
	}
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
	const CAngleVector ComputeAngles() const;
};
struct tObjTransformation {
	CFixMatrix mOrient;
	CFixVector vPos;
};
struct CPhysSimData;
struct CObject {
	int Type() const
	{
		return info.nType;
	}
	int Id() const
	{
		return info.nId;
	}
	int Segment() const
	{
		return info.nSegment;
	}
	bool IsMissile() const
	{
		return info.nType == 5;
	}
	CFixVector &Position()
	{
		return info.position.vPos;
	}
	CFixVector StartVel() const
	{
		return {};
	}
	int32_t UpdateOffset(CPhysSimData &);
	int32_t ApplyWallPhysics(int16_t, int16_t);
	int32_t CheckSegmentPhysics();
	int32_t CheckWallPhysics();
	void ScrapeOnWall(int16_t, int16_t, CFixVector &);
	void Bump(CFixVector, fix);
	void ApplyForce(CFixVector);
	int Index() const
	{
		return 0;
	}
	bool IsGuideBot() const
	{
		return false;
	}
	void Die()
	{
		throw std::runtime_error("Unexpected lethal oracle sample");
	}
	void ApplyDamageToPlayer(CObject *, fix);
	void ApplyDamageToRobot(fix, int);
	void CollideWeaponAndWall(fix, int, int, CFixVector &) {}
	void CollideDebrisAndWall(fix, int, int, CFixVector &) {}
	int MaxSpeedScaled() const
	{
		return 60;
	}
	CFixVector &Velocity()
	{
		return mType.physInfo.velocity;
	}
	void ProcessDrag(CPhysSimData &);
	struct {
		tObjTransformation position;
		int nType = 0, nId = 0, nSegment = 0, movementType = 1;
		fix xShield = I2X(100), xSize = 0;
	} info;
	struct {
		struct {
			CFixVector velocity, thrust, rotVel;
			fix mass = I2X(1), drag = 0;
			int flags = 0;
		} physInfo;
	} mType;
} objects[2];
auto &object = objects[0];
struct FluidWall {
	int nType = 0, flags = 0;
};
struct CSide {
	int m_nBaseTex = 0;
	FluidWall *wall = nullptr;
	CFixVector m_normals[2];
	FluidWall *Wall()
	{
		return wall;
	}
	int32_t Physics(fix &, bool);
};
struct Segment {
	CSide m_sides[6];
	bool lava = false, protected_segment = false;
	int mask = 0;
	struct Mask {
		int m_side;
	};
	Mask Masks(CFixVector, fix)
	{
		return { mask };
	}
	int32_t Physics(fix &);
	int32_t Physics(int side, fix &damage)
	{
		return m_sides[side].Physics(damage, child == -1);
	}
	bool HasNoDamageProp() const
	{
		return protected_segment;
	}
	CFixVector normal, center;
	bool water = false;
	bool HasWaterProp() const
	{
		return water;
	}
	bool HasLavaProp() const
	{
		return lava;
	}
	int m_function = 0;
	int child = -1;
	int ChildId(int)
	{
		return child;
	}
	CFixVector SideCenter(int)
	{
		return center;
	}
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
} segments[10];
auto &segment = segments[0];
CObject *OBJECT(int index)
{
	return &objects[index];
}
Segment *SEGMENT(int index)
{
	return &segments[index];
}
struct {
	struct {
		int bD1Mission = 0, bNostalgia = 0, nDifficultyLevel = 0;
		int bPlayerIsDead = 0, bD2XLevel = 1, bHaveExtraGameInfo[2] = {}, bEndLevelSequence = 0;
	} app;
	struct {
		CFixVector vTgtDir;
		int nDirSteps = 0;
		struct {
			float fSpeed = 1.0f;
		} slowmo[2];
	} gameplay;
	struct {
		int bControlsSkipFrame = 0;
	} input;
} gameStates;
#endif
