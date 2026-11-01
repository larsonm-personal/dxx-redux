#include "camera_orientation.h"
#include "camera_object_state.h"
#include "inventory.h"
#include <cassert>
#include <iostream>
#include <fstream>
namespace camera_matrix_reference
{
using fix = std::int32_t;
using fixang = std::int16_t;
using time_t = std::int64_t;
constexpr double PI = 3.141592653589793240;
constexpr int AIB_NORMAL = 129;
struct {
	struct {
		int nMathFormat = 2;
		struct {
			int nSpeed = 5000;
		} cameras;
	} render;
} options;
auto *gameOpts = &options;
struct {
	struct {
		time_t nSDLTicks[1]{ 100 };
	} app;
} gameStates;
struct tQuadInt {
	int low, high;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected legacy camera matrix math");
}
fix QuadSqrt(int, int)
{
	throw std::runtime_error("Unexpected legacy camera matrix magnitude");
}
struct CAngleVector {
	struct {
		struct {
			fixang p = 0, b = 0, h = 0;
		} coord;
	} v;
};
struct CFixVector {
	struct {
		struct {
			fix x = 0, y = 0, z = 0;
		} coord;
	} v;
	static const CFixVector Create(fix x, fix y, fix z)
	{
		return { { { x, y, z } } };
	}
	static const CFixVector Cross(const CFixVector &, const CFixVector &);
	static const fix Dot(const CFixVector &, const CFixVector &);
	static const fix Normalize(CFixVector &);
	fix Mag() const;
};
struct CFixMatrix {
	struct {
		struct {
			CFixVector r, u, f;
		} dir;
	} m;
	static const CFixMatrix Create(fix, fix, fix, fix, fix, fix);
	static const CFixMatrix Create(const CAngleVector &);
	static const CFixMatrix CreateFU(const CFixVector &, const CFixVector &);
	CFixMatrix Mul(const CFixMatrix &);
	CFixMatrix operator*(const CFixMatrix &);
	void CheckAndFix();
};
struct CObject {
	struct {
		struct {
			int behavior = 129;
		} aiInfo;
	} cType;
	struct {
		struct {
			fix brakes = 0;
		} physInfo;
	} mType;
	struct {
		struct {
			CFixMatrix mOrient;
		} position;
	} info;
};
struct CCamera {
	struct {
		CObject *pObj = nullptr;
		fixang curAngle = 0, curDelta = 0;
		time_t t0 = 0;
		CFixMatrix orient;
	} m_data;
	void Rotate();
};
#include "xl_reference_camera_matrix.h"
CFixVector vector(const xl_compat::FixedLightPoint &p)
{
	return CFixVector::Create(p[0], p[1], p[2]);
}
xl_compat::FixedLightPoint point(const CFixVector &v)
{
	return { v.v.coord.x, v.v.coord.y, v.v.coord.z };
}
CFixMatrix matrix(const xl_compat::CameraMatrix &m)
{
	return { { { vector(m[0]), vector(m[1]), vector(m[2]) } } };
}
xl_compat::CameraMatrix rows(const CFixMatrix &m)
{
	return { point(m.m.dir.r), point(m.m.dir.u), point(m.m.dir.f) };
}
void compare(const xl_compat::CameraMatrix &original, fixang heading)
{
	CAngleVector angle;
	angle.v.coord.h = heading;
	auto source = matrix(original) * CFixMatrix::Create(angle);
	if (source.m.dir.f.Mag() == 0) throw std::runtime_error("Invalid original camera forward direction");
	source.CheckAndFix();
	if (rows(source) != xl_compat::camera_orientation(original, heading))
		throw std::runtime_error("Camera matrix differs from original composition/repair at heading " + std::to_string(heading));
}
} // namespace camera_matrix_reference
void camera_matrix_source_controls()
{
	using namespace camera_matrix_reference;
	unsigned count = 0;
	for (int pitch : { -16384, -7111, 0, 5237, 16384 })
		for (int bank : { -6123, 0, 4096 }) {
			CAngleVector angle;
			angle.v.coord.p = fixang(pitch);
			angle.v.coord.b = fixang(bank);
			angle.v.coord.h = 3179;
			const auto original = rows(CFixMatrix::Create(angle));
			for (int heading = -32768; heading < 32768; heading += 17) {
				compare(original, fixang(heading));
				++count;
			}
			CObject object;
			CCamera camera;
			camera.m_data.pObj = &object;
			camera.m_data.orient = matrix(original);
			xl_compat::CameraRotation portable;
			fix brakes = 0;
			for (int frame = 0; frame < 1000; ++frame) {
				gameStates.app.nSDLTicks[0] = frame / 3;
				options.render.cameras.nSpeed = (frame / 200 + 1) * 1000;
				const bool changed = portable.advance(true, 129, gameStates.app.nSDLTicks[0], options.render.cameras.nSpeed, brakes);
				camera.Rotate();
				if (camera.m_data.curAngle != portable.angle || camera.m_data.curDelta != portable.delta ||
				    object.mType.physInfo.brakes != brakes || (changed && rows(object.info.position.mOrient) != xl_compat::camera_orientation(original, portable.angle)))
					throw std::runtime_error("Full camera controller matrix sequence differs");
				++count;
			}
		}
	for (const auto forward : { xl_compat::FixedLightPoint{ 0, 65536, 0 }, xl_compat::FixedLightPoint{ 0, -65536, 0 }, xl_compat::FixedLightPoint{ 1137, -7991, 65531 } })
		for (const auto up : { xl_compat::FixedLightPoint{}, forward, xl_compat::FixedLightPoint{ 1, 1, -1 } }) {
			const auto source = CFixMatrix::CreateFU(vector(forward), vector(up));
			if (rows(source) != xl_compat::camera_repair_orientation(forward, up)) throw std::runtime_error("Degenerate camera up-vector repair differs");
			++count;
		}
	bool rejected = false;
	try {
		xl_compat::camera_repair_orientation({}, {});
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	if (!rejected) throw std::runtime_error("Zero camera forward direction accepted");
	std::cout << "Pinned camera matrices: " << count << " composition, repair and full controller comparisons passed\n";
}
void camera_matrix_original_controls(const xl_compat::Inventory &level)
{
	unsigned count = 0;
	for (const auto &object : level.objects) {
		if (object.type != 16) continue;
		xl_compat::CameraMatrix original{};
		for (unsigned column = 0; column < 3; ++column)
			for (unsigned row = 0; row < 3; ++row) original[column][row] = object.orientation[column * 3 + row];
		for (int heading = -8192; heading <= 8192; heading += 128) {
			camera_matrix_reference::compare(original, std::int16_t(heading));
			++count;
		}
	}
	if (count) std::cout << "Original camera orientation: " << count << " source comparisons passed\n";
}

void camera_controller_runtime_controls(const char *path)
{
	using namespace camera_matrix_reference;
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Missing native camera controller receipt");
	std::array<std::int64_t, 38> row{};
	unsigned count = 0;
	while (input >> row[0]) {
		for (unsigned i = 1; i < row.size(); ++i)
			if (!(input >> row[i])) throw std::runtime_error("Truncated camera controller receipt");
		const auto read_matrix = [&](unsigned at) {
			xl_compat::CameraMatrix value{};
			for (auto &column : value)
				for (auto &axis : column) axis = fix(row[at++]);
			return value;
		};
		CObject object;
		CCamera camera;
		object.cType.aiInfo.behavior = int(row[0]);
		gameStates.app.nSDLTicks[0] = row[1];
		options.render.cameras.nSpeed = int(row[2]);
		camera.m_data.pObj = &object;
		camera.m_data.curAngle = fixang(row[3]);
		camera.m_data.curDelta = fixang(row[4]);
		camera.m_data.t0 = row[5];
		object.mType.physInfo.brakes = fix(row[6]);
		camera.m_data.orient = matrix(read_matrix(7));
		object.info.position.mOrient = matrix(read_matrix(16));
		camera.Rotate();
		if (camera.m_data.curAngle != row[25] || camera.m_data.curDelta != row[26] || camera.m_data.t0 != row[27] ||
		    object.mType.physInfo.brakes != row[28] || rows(object.info.position.mOrient) != read_matrix(29))
			throw std::runtime_error("Native CT_CAMERA differs from pinned full controller at row " + std::to_string(count));
		++count;
	}
	if (!input.eof() || count != 10800) throw std::runtime_error("Incomplete native camera controller receipt");
	std::cout << "Native CT_CAMERA: " << count << " full source comparisons passed\n";
}
