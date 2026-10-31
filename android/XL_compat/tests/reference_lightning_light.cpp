#include "lightning_light.h"
#include <cstring>
#include <iostream>
namespace lightning_light_reference
{
using fix = int;
struct CFixVector {
	struct {
		struct {
			int x, y, z;
		} coord;
	} v{};
	void SetZero()
	{
		v.coord = {};
	}
	CFixVector &operator+=(const CFixVector &b)
	{
		v.coord.x += b.v.coord.x;
		v.coord.y += b.v.coord.y;
		v.coord.z += b.v.coord.z;
		return *this;
	}
};
struct CFloatVector {
	std::array<float, 4> values{};
	float &Red()
	{
		return values[0];
	}
	float &Green()
	{
		return values[1];
	}
	float &Blue()
	{
		return values[2];
	}
	float &Alpha()
	{
		return values[3];
	}
};
template <class T>
struct Array {
	std::vector<T> values;
	T *Buffer()
	{
		return values.empty() ? nullptr : values.data();
	}
	T &operator[](int i)
	{
		return values.at(i);
	}
	T *operator+(int i)
	{
		return values.data() + i;
	}
};
struct CLightningNode {
	CFixVector m_vPos;
	bool SetLight(int16_t, CFloatVector *);
};
struct CLightning {
	int m_bLight = 1, m_nNodes = 0, m_nLength = 0, m_nSegment = 0;
	Array<CLightningNode> m_nodes;
	CFloatVector m_color;
	int32_t SetLight();
};
struct CObject {
	int type = 20;
	int Type() const
	{
		return type;
	}
} object;
struct CLightningEmitter {
	int m_bValid = 1, m_nObject = 0, m_nBolts = 0;
	Array<CLightning> m_lightning;
	int32_t SetLight();
};
struct Pool : Array<CLightningEmitter> {
	CLightningEmitter *GetFirst(int &i)
	{
		i = 0;
		return values.empty() ? nullptr : &values[0];
	}
	CLightningEmitter *GetNext(int &i)
	{
		return ++i < int(values.size()) ? &values[i] : nullptr;
	}
};
struct tLightningLight {
	int nFrame = -1, nSegment = -1, nNext = -1, nLights = 0, nBrightness = 0, nDynLight = -1;
	CFixVector vPos;
	CFloatVector color;
};
struct CLightningManager {
	Array<tLightningLight> m_lights;
	Pool m_emitters;
	int m_nFirstLight = -1;
	void SetSegmentLight(int16_t, CFixVector *, CFloatVector *);
	void ResetLights(int32_t);
	void SetLights();
} lightningManager;
struct {
	struct {
		int nSegments = 6;
	} segData;
	struct {
		int nFrameCount = 0;
	} appData;
} gameData;
struct {
	struct {
		int nLightingMethod = 0;
	} render;
} gameStates;
struct {
	void DeleteLightning() {}
	int Add(void *, CFloatVector *, int, int segment, int, int, int, CFixVector *)
	{
		return segment;
	}
} lightManager;
std::vector<std::array<int, 4>> queries;
int rejected_node = -1;
int FindSegByPos(const CFixVector &position, int start, int exhaustive, int skybox)
{
	if (exhaustive || skybox) throw std::runtime_error("Lightning node requested exhaustive/skybox lookup");
	queries.push_back({ position.v.coord.x, position.v.coord.y, position.v.coord.z, start });
	if (position.v.coord.x == rejected_node) return -1;
	return std::abs(position.v.coord.x) % gameData.segData.nSegments;
}
double DRound(double value)
{
	return value < 0 ? value - 0.5 : value + 0.5;
}
#define ENTER(...)
#define RETURN                return;
#define RETVAL(value)         return (value);
#define I2X(value)            ((value) * 65536)
#define F2X(value)            (fix((value) * 65536))
#define SHOW_LIGHTNING(value) 1
#define LEVEL_SEGMENTS        gameData.segData.nSegments
#define OBJECT(index)         ((index) >= 0 ? &object : nullptr)
#define OBJ_POWERUP           7
#define USE_OPENMP            0
#include "xl_reference_lightning_light.h"
} // namespace lightning_light_reference

void lightning_light_controls()
{
	using namespace xl_compat;
	using namespace lightning_light_reference;
	unsigned cases = 0;
	lightningManager.m_lights.values.resize(6);
	for (int nodes : { 1, 5, 8, 9, 16, 33 })
		for (int length : { 0, 655359, 655360, 655361, 18 * 65536, 30 * 65536, 100 * 65536 })
			for (int reject : { -1, 0, 3, 8, 16 })
				for (unsigned flags = 0; flags < 16; ++flags) {
					LightningLightAccumulator native;
					std::vector<std::array<int, 4>> actual_queries;
					queries.clear();
					rejected_node = reject;
					object.type = flags & 8 ? 7 : 20;
					++gameData.appData.nFrameCount;
					gameStates.render.nLightingMethod = flags & 1;
					lightningManager.m_emitters.values.assign(2, {});
					for (unsigned owner = 0; owner < 2; ++owner) {
						auto &emitter = lightningManager.m_emitters.values[owner];
						emitter.m_bValid = flags & 2 ? -1 : 1;
						emitter.m_nObject = flags & 4 ? -1 : int(owner);
						emitter.m_nBolts = 3;
						for (unsigned b = 0; b < 3; ++b) {
							LightningBolt bolt{};
							bolt.definition.light = b != 2;
							bolt.definition.color = { float(owner + 1) / 7, float(b + 1) / 11, 149.0f / 255, 128.0f / 255 };
							bolt.lifetime.nodes = b == 1 && flags & 1 ? -nodes : nodes;
							bolt.lifetime.length = length;
							bolt.segment = int(owner + b);
							bolt.nodes.resize(nodes);
							CLightning source;
							source.m_bLight = bolt.definition.light;
							source.m_nNodes = bolt.lifetime.nodes;
							source.m_nLength = length;
							source.m_nSegment = bolt.segment;
							source.m_color.values = bolt.definition.color;
							for (int node = 0; node < nodes; ++node) {
								bolt.nodes[node].position = { node, int(owner) * 172 - 99, int(b) * 79 - 301 };
								source.m_nodes.values.push_back({ { { { node, bolt.nodes[node].position[1], bolt.nodes[node].position[2] } } } });
							}
							emitter.m_lightning.values.push_back(source);
							if (emitter.m_bValid > 0 && emitter.m_nObject >= 0 && object.type != 7)
								lightning_bolt_lights(bolt, [&](const auto &p, int start) {
									actual_queries.push_back({ p[0], p[1], p[2], start });
									return p[0] == reject ? -1 : std::abs(p[0]) % 6; }, [&](int segment, const auto &p, const auto &color) { native.add(segment, p, color); });
						}
					}
					lightningManager.SetLights();
					if (queries != actual_queries) throw std::runtime_error("Lightning light node sampling/order differs from source");
					const auto actual = native.finish();
					unsigned populated = 0;
					for (const auto &expected : lightningManager.m_lights.values) {
						if (expected.nFrame != gameData.appData.nFrameCount) continue;
						++populated;
						const auto &light = actual.at(expected.nSegment);
						if (light.samples != expected.nLights || light.brightness != expected.nBrightness || light.color != expected.color.values ||
						    light.position != LightningPoint{ expected.vPos.v.coord.x, expected.vPos.v.coord.y, expected.vPos.v.coord.z })
							throw std::runtime_error("Lightning segment aggregation differs from source");
					}
					if (populated != actual.size()) throw std::runtime_error("Lightning collected an extra segment light");
					++cases;
				}
	std::cout << cases << " lightning node sampling/segment aggregation cases match pinned source\n";
}
