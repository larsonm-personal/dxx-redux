// Pinned reader adapters: valid controls only; malformed input is tested against Redux
#include "oof_fixture.h"
#include <fstream>
#include <iostream>
#include <iterator>

namespace oof_reference
{
struct CFile {
	const std::vector<uint8_t> &bytes;
	std::size_t position = 0;
	int32_t ReadInt()
	{
		if (position + 4 > bytes.size()) throw std::runtime_error("Reference EOF");
		uint32_t n = 0;
		for (int i = 0; i < 4; ++i) n |= uint32_t(bytes[position++]) << (8 * i);
		return int32_t(n);
	}
	float ReadFloat()
	{
		auto n = ReadInt();
		float f;
		std::memcpy(&f, &n, 4);
		return f;
	}
	bool Read(void *out, std::size_t size, std::size_t count)
	{
		auto n = size * count;
		if (n > bytes.size() - position) throw std::runtime_error("Reference EOF");
		std::memcpy(out, bytes.data() + position, n);
		position += n;
		return true;
	}
};
struct CFloatVector {
	struct {
		struct {
			float x = 0, y = 0, z = 0;
		} coord;
	} v;
	float Mag() const
	{
		return std::sqrt(v.coord.x * v.coord.x + v.coord.y * v.coord.y + v.coord.z * v.coord.z);
	}
	CFloatVector &operator/=(float f)
	{
		v.coord.x /= f;
		v.coord.y /= f;
		v.coord.z /= f;
		return *this;
	}
};
template <class T>
struct Array : std::vector<T> {
	bool Create(std::size_t n, const char * = nullptr)
	{
		if (n > 1048576) throw std::runtime_error("Oracle allocation limit");
		this->resize(n);
		return true;
	}
	void Clear(int)
	{
		std::fill(this->begin(), this->end(), T{});
	}
	void Destroy()
	{
		this->clear();
	}
	std::size_t Length() const
	{
		return this->size();
	}
};
namespace OOF
{
struct CModel;
struct CFrameInfo {
	int m_nFrames = 0, m_nFirstFrame = 0, m_nLastFrame = 0;
	int32_t Read(CFile &, CModel *, int32_t);
};
struct CModel {
	CFrameInfo m_frameInfo;
};
struct CRotFrame {
	int m_nStartTime = 0, m_nAngle = 0;
	CFloatVector m_vAxis;
	int32_t Read(CFile &, int32_t);
};
struct CAnim : CFrameInfo {
	int m_nTicks = 0;
	Array<uint8_t> m_remapTicks;
};
struct CRotAnim : CAnim {
	Array<CRotFrame> m_frames;
	int32_t Read(CFile &, CModel *, int32_t);
	void Destroy()
	{
		m_frames.Destroy();
		m_remapTicks.Destroy();
		m_nTicks = 0;
	}
};
struct CSpecialPoint {
	char *m_pszName = nullptr, *m_pszProps = nullptr;
	CFloatVector m_vPos;
	float m_fRadius = 0;
	void Init()
	{
		m_pszName = nullptr;
		m_pszProps = nullptr;
		m_vPos = {};
		m_fRadius = 0;
	}
	void Destroy()
	{
		delete[] m_pszName;
		delete[] m_pszProps;
		m_pszName = m_pszProps = nullptr;
	}
	int32_t Read(CFile &);
	~CSpecialPoint()
	{
		Destroy();
	}
};
struct CSpecialList : Array<CSpecialPoint> {
	int32_t Read(CFile &);
};
} // namespace OOF
using namespace OOF;
int nIndent = 0;
#define NEW               new
#define OOF_PrintLog(...) ((void) 0)
#include "xl_reference_oof_readers.h"
#undef OOF_PrintLog
#undef NEW

void check(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
void inspect(const xl_compat::OofModel &model)
{
	for (const auto &chunk : model.additional_chunks) {
		CFile file{ chunk.bytes };
		if (chunk.type == "SPCL") {
			CSpecialList points;
			check(points.Read(file) == 1, "Reference SPCL failed");
			check(file.position == chunk.bytes.size(), "Reference SPCL boundary differs");
			check(model.has_special_points && model.special_points.size() == points.size(), "Typed SPCL count differs");
			for (std::size_t i = 0; i < points.size(); ++i) {
				const auto &a = points[i];
				const auto &b = model.special_points[i];
				check(b.name == a.m_pszName && b.properties == a.m_pszProps && b.radius == a.m_fRadius && b.position == xl_compat::OofVector{ a.m_vPos.v.coord.x, a.m_vPos.v.coord.y, a.m_vPos.v.coord.z }, "Typed SPCL differs from pinned reader");
			}
			std::cout << "SPCL points=" << points.size();
			for (const auto &p : points) std::cout << " name=" << p.m_pszName << " properties=" << p.m_pszProps << " radius=" << p.m_fRadius;
			std::cout << '\n';
		} else if (chunk.type == "RANI") {
			CModel owner;
			for (std::size_t i = 0; i < model.submodels.size(); ++i) {
				CRotAnim track;
				check(track.Read(file, &owner, 1) == 1, "Reference RANI failed");
				check(model.has_rotation_tracks && model.rotation_tracks.size() == model.submodels.size(), "Typed RANI track count differs");
				const auto &decoded = model.rotation_tracks.at(i);
				check(decoded.first == track.m_nFirstFrame && decoded.last == track.m_nLastFrame && decoded.frames.size() == track.m_frames.size(), "Typed RANI frame info differs");
				for (std::size_t n = 0; n < decoded.frames.size(); ++n) {
					const auto &a = track.m_frames[n];
					const auto &b = decoded.frames[n];
					check(a.m_nStartTime == b.start && a.m_nAngle == b.angle && b.normalized_axis == xl_compat::OofVector{ a.m_vAxis.v.coord.x, a.m_vAxis.v.coord.y, a.m_vAxis.v.coord.z }, "Typed RANI frame differs");
				}
				std::cout << "RANI submodel=" << i << " frames=" << track.m_nFrames << " first=" << track.m_nFirstFrame << " last=" << track.m_nLastFrame << " ticks=" << track.m_nTicks;
				for (const auto &frame : track.m_frames) std::cout << " [" << frame.m_nStartTime << "," << frame.m_vAxis.v.coord.x << "," << frame.m_vAxis.v.coord.y << "," << frame.m_vAxis.v.coord.z << "," << frame.m_nAngle << "]";
				std::cout << '\n';
			}
			check(file.position == chunk.bytes.size(), "Reference RANI boundary differs");
		}
	}
}
} // namespace oof_reference
int main(int argc, char **argv)
{
	try {
		if (argc == 1)
			for (int variant : { 0, 1 }) oof_reference::inspect(xl_compat::read_oof(xl_oof_test::metadata_fixture(variant)));
		for (int i = 1; i < argc; ++i) {
			std::ifstream file(argv[i], std::ios::binary);
			if (!file) throw std::runtime_error("Missing original");
			const auto model = xl_compat::read_oof(std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {}));
			oof_reference::inspect(model);
		}
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
