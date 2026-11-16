#include "../afterburner_emission.h"
#include <iostream>

namespace afterburner_reference
{
using fix = int32_t;
struct CAngleVector {
};
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	CFixVector operator+(const CFixVector &other) const
	{
		return { { { v.coord.x + other.v.coord.x, v.coord.y + other.v.coord.y, v.coord.z + other.v.coord.z } } };
	}
	CFixVector &operator+=(const CFixVector &other)
	{
		return *this = *this + other;
	}
	CFixVector operator*(fix scale) const
	{
		return { { { fix((int64_t(v.coord.x) * scale) >> 16), fix((int64_t(v.coord.y) * scale) >> 16), fix((int64_t(v.coord.z) * scale) >> 16) } } };
	}
	CFixVector &operator*=(const CFixVector &)
	{
		throw std::runtime_error("Unexpected source model scale path");
	}
	bool IsZero() const
	{
		return !v.coord.x && !v.coord.y && !v.coord.z;
	}
	CAngleVector ToAnglesVec() const
	{
		throw std::runtime_error("Unexpected source decorative thruster path");
	}
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector f, r, u;
		} dir;
	} m;
	CFixMatrix Transpose() const
	{
		throw std::runtime_error("Unexpected source spectator transform");
	}
	CFixVector operator*(const CFixVector &) const
	{
		throw std::runtime_error("Unexpected source model thruster transform");
	}
	static CFixMatrix Create(CAngleVector)
	{
		throw std::runtime_error("Unexpected source decorative thruster rotation");
	}
};
struct tObjTransformation {
	CFixVector vPos;
	CFixMatrix mOrient;
};
constexpr int OBJ_PLAYER = 4, OBJ_ROBOT = 2, OBJ_WEAPON = 5;
constexpr int EARTHSHAKER_ID = 54, MEGAMSL_ID = 18, EARTHSHAKER_MEGA_ID = 55, SMARTMSL_ID = 17;
constexpr int REAR_THRUSTER = 1, FRONTAL_THRUSTER = 2;
struct CObject {
	struct {
		int nType = OBJ_PLAYER, nId = 0;
		fix xSize = 0;
		tObjTransformation position;
	} info;
	struct {
		struct {
			bool CLOAKED = false;
		} aiInfo;
	} cType;
	bool IsMissile() const
	{
		return info.nType == OBJ_WEAPON;
	}
	int ModelId() const
	{
		return 0;
	}
	CFixMatrix *View(int)
	{
		return &info.position.mOrient;
	}
};
struct Thrusters {
	int nCount = 0;
	float fSize[2]{};
	int nType[2]{};
	CFixVector vPos[2], vDir[2];
};
struct tHitbox {
	CFixVector vMin, vMax;
};
struct ThrusterPath {
	void *GetPoint()
	{
		return nullptr;
	}
};
struct ThrusterData {
	ThrusterPath path;
};
struct tThrusterInfo {
	void *pp = nullptr;
	Thrusters *pThrusters = nullptr;
	float fLength[2]{}, fSize[2]{}, fScale = 0;
	int nType[2]{};
	CFixVector vPos[2], vDir[2];
	CFixMatrix mRot[2];
};
struct {
	struct {
		Thrusters thrusters[1];
		struct {
			tHitbox hitboxes[1];
		} hitboxes[1];
		CFixVector vScale;
	} modelData;
	struct {
		ThrusterData thrusters[1];
	} renderData;
} gameData;
struct Options {
	struct {
		bool bHiresModels[1]{};
	} render;
} options, *gameOpts = &options;
struct {
	struct {
		bool bFixModels = false;
	} app;
} gameStates;
bool has_ase = false;
bool GetASEModel(int)
{
	return has_ase;
}
struct CThrusterFlames {
	tThrusterInfo m_ti;
	ThrusterData *m_pData = nullptr;
	int m_nThrusters = 0, m_nStyle = 0;
	bool m_bSpectate = false;
	void CalcPosOnShip(CObject *, CFixVector *);
	int32_t CalcPos(CObject *, tThrusterInfo *, int32_t);
};
#define OBJPOS(p)     (&(p)->info.position)
#define EGI_FLAG(...) 1
#define SPECTATOR(p)  false
#include "xl_reference_afterburner_emission.h"
#undef SPECTATOR
#undef EGI_FLAG
#undef OBJPOS
CFixVector vector(const xl_compat::EmissionPoint &point)
{
	return { { { point[0], point[1], point[2] } } };
}
xl_compat::EmissionPoint point(const CFixVector &value)
{
	return { value.v.coord.x, value.v.coord.y, value.v.coord.z };
}
} // namespace afterburner_reference

void afterburner_emission_source_controls()
{
	using namespace afterburner_reference;
	unsigned cases = 0;
	for (bool hires : { false, true })
		for (bool ase : { false, true })
			for (int type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_WEAPON })
				for (int size : { 1, 65535, 65536, 262149 })
					for (int span : { 0, 1, 65535, 65536, 262149 })
						for (int phase : { 0, 1, 2 }) {
							const xl_compat::EmissionPoint origin{ 123456, -987654, 654321 };
							const xl_compat::EmissionPoint forward = phase == 0 ? xl_compat::EmissionPoint{ 0, 0, 65536 } : phase == 1 ? xl_compat::EmissionPoint{ 65536, 0, 0 }
							                                                                                                           : xl_compat::EmissionPoint{ -23456, 12537, 54321 };
							const xl_compat::EmissionPoint right = phase == 0 ? xl_compat::EmissionPoint{ 65536, 0, 0 } : phase == 1 ? xl_compat::EmissionPoint{ 0, 0, -65536 }
							                                                                                                         : xl_compat::EmissionPoint{ 54321, -3212, 23456 };
							CObject object;
							object.info.nType = type;
							object.info.xSize = size;
							object.info.position.vPos = vector(origin);
							object.info.position.mOrient.m.dir.f = vector(forward);
							object.info.position.mOrient.m.dir.r = vector(right);
							gameOpts->render.bHiresModels[0] = hires;
							has_ase = ase;
							auto &box = gameData.modelData.hitboxes[0].hitboxes[0];
							box.vMin.v.coord.z = -12345;
							box.vMax.v.coord.z = -12345 + span;
							CThrusterFlames source;
							tThrusterInfo expected;
							const int count = source.CalcPos(&object, &expected, 1);
							const auto actual = xl_compat::afterburner_emission(origin, forward, right, size, type == OBJ_PLAYER, hires, ase, -12345, -12345 + span);
							if (int(actual.count) != count) throw std::runtime_error("Afterburner emission count differs from pinned CalcPos");
							for (int i = 0; i < count; ++i)
								if (actual.positions[i] != point(expected.vPos[i])) throw std::runtime_error("Afterburner emission point differs from pinned CalcPos");
							++cases;
						}
	std::cout << "Pinned afterburner emission: " << cases << " position/model/profile controls passed\n";
}
