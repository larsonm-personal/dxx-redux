#include "lightning_emitter.h"
#include <cmath>
#include <cstring>
#include <iostream>

namespace lightning_path_reference
{
using fix = int;
#include "xl_reference_particle_math.h"
struct CFixVector {
	struct {
		struct {
			int x, y, z;
		} coord;
	} v{};
	void Set(int x, int y, int z)
	{
		v.coord = { x, y, z };
	}
	void SetZero()
	{
		Set(0, 0, 0);
	}
	static CFixVector Create(int x, int y, int z)
	{
		CFixVector result;
		result.Set(x, y, z);
		return result;
	}
	const CFixVector &operator+=(const CFixVector &);
	const CFixVector &operator-=(const CFixVector &);
	const CFixVector &operator*=(fix);
	const CFixVector operator+(const CFixVector &) const;
	const CFixVector operator-(const CFixVector &) const;
	const CFixVector operator*(fix) const;
	fix Mag() const;
	bool operator==(const CFixVector &) const;
	static const fix Dist(const CFixVector &, const CFixVector &);
	void Check();
	bool IsZero() const
	{
		return !v.coord.x && !v.coord.y && !v.coord.z;
	}
	static const fix Normalize(CFixVector &);
	static const fix Dot(const CFixVector &, const CFixVector &);
	static const CFixVector Perp(const CFixVector &, const CFixVector &, const CFixVector &);
	static const CFixVector Normal(const CFixVector &, const CFixVector &, const CFixVector &);
	static const CFixVector Cross(const CFixVector &, const CFixVector &);
};
[[noreturn]] void unsupported()
{
	throw std::runtime_error("Lightning oracle entered a path outside the original style-0 profile");
}
struct Child {
	void Setup(bool)
	{
		unsupported();
	}
	void Move(CFixVector, int)
	{
		unsupported();
	}
	void Animate(int, int)
	{
		unsupported();
	}
};
struct CLightningNode {
	CFixVector m_vBase, m_vPos, m_vNewPos, m_vOffs, m_vDelta[2];
	int m_nDelta[2]{};
	Child *m_child = nullptr;
	template <class... Args>
	bool CreateChild(Args...)
	{
		unsupported();
	}
	void Setup(bool, CFixVector *, CFixVector *);
	void Move(const CFixVector &, int16_t);
	void Move(const CFixVector &, const CFixVector &, const CFixVector &, const CFixVector &, float, int16_t);
	void Animate(bool, int16_t, int32_t, int32_t);
	void ComputeOffset(int32_t);
	CFixVector CreateErratic(CFixVector *, CFixVector *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
	int Clamp(CFixVector *, CFixVector *, int);
	void CreatePerlin(double, double, int)
	{
		unsupported();
	}
	CFixVector CreateJaggy(CFixVector *, CFixVector *, CFixVector *, CFixVector *, int, int, int, int, int, int)
	{
		unsupported();
	}
};
struct Nodes {
	std::vector<CLightningNode> values;
	bool Create(int count)
	{
		values.resize(count);
		return true;
	}
	void Clear()
	{
		std::fill(values.begin(), values.end(), CLightningNode{});
	}
	CLightningNode *Buffer()
	{
		return values.data();
	}
	CLightningNode *operator+(int i)
	{
		return values.data() + i;
	}
	CLightningNode &operator[](int i)
	{
		return values.at(i);
	}
};
struct Allocation {
	bool Create(int count)
	{
		return count >= 0;
	}
};
struct CLightning {
	Nodes m_nodes;
	CFixVector m_vPos, m_vEnd;
	int m_nNodes, m_bRandom = 1, m_nFrames, m_nSmoothe = 0, m_bClamp = 0, m_nAmplitude, m_nLength = 18 * 65536, m_bInPlane = 0;
	CFixVector m_vBase, m_vRefEnd, m_vDelta, m_vDir;
	int m_nObject = 307, m_nSegment = 362, m_nAngle = 0, m_nOffset = 0, m_iStep = 0;
	int m_nChildren = 0, m_nNode = -1, m_bGlow = 1, m_bBlur = 1, m_bLight = 1, m_nStyle = 0;
	int m_nLife = -300, m_nTTL = 300, m_nDelay = 500;
	float m_width = 20;
	std::array<float, 4> m_color{};
	CLightning *m_parent = nullptr;
	Allocation m_plasmaTexCoord, m_plasmaVerts, m_coreVerts;
	std::vector<CFixVector> drawn;
	void Setup(bool);
	void Move(CFixVector, CFixVector, int16_t);
	void Move(CFixVector, int16_t);
	bool Create(char, int32_t);
	void Animate(int32_t, int32_t);
	int32_t SetLife();
	void DestroyNodes()
	{
		m_nodes.values.clear();
		m_nNodes = 0;
	}
	int ComputeChildEnd(CFixVector *, CFixVector *, CFixVector *, CFixVector *, int)
	{
		unsupported();
	}
	void RenderSetup(int, int)
	{
		drawn.clear();
		for (const auto &node : m_nodes.values) drawn.push_back(node.m_vPos);
	}
	void Smoothe();
	void ComputeOffsets();
	void CreatePath(int32_t, int32_t);
	void Rotate(int)
	{
		unsupported();
	}
	void Scale(int, int)
	{
		unsupported();
	}
};
struct Noise {
	void Setup(double, double, int)
	{
		unsupported();
	}
} noiseX[1], noiseY[1];
constexpr double ampScale = 1, persistence = 0;
constexpr int octaves = 0;
struct {
	struct {
		int nMathFormat = 2;
		struct {
			bool bGlow = true;
			int nStyle = 0;
		} lightning;
	} render;
} options;
auto *gameOpts = &options;
struct {
	int bUseLightning = 1;
} extraGameInfo[1];
struct {
	struct {
		int nSDLTicks[1]{};
		struct {
			int nTime = 25;
		} tick40fps;
	} app;
} gameStates;
struct CObject {
	struct {
		CFixVector vPos;
	} position;
	struct {
		int nSegment = 362;
	} info;
	int Type() const
	{
		return 20;
	}
	int Frame() const
	{
		return 0;
	}
} object;
constexpr int OBJ_ROBOT = 2, OBJ_POWERUP = 7;
struct {
	struct {
		int nFrameCount = 0;
	} appData;
} gameData;
struct tQuadInt {
	unsigned low;
	int high;
};
void FixMulAccum(tQuadInt *, int, int)
{
	unsupported();
}
unsigned QuadSqrt(unsigned, int)
{
	unsupported();
}
unsigned random_index;
std::vector<int> bounded_script;
unsigned bounded_index;
bool cone_samples = false;
double sample(unsigned i)
{
	if (cone_samples) {
		// Scrambled counter supplies all directions; the legacy linear stream correlates adjacent axes
		i += 0x9e3779b9u;
		i = (i ^ (i >> 16)) * 0x85ebca6bu;
		i = (i ^ (i >> 13)) * 0xc2b2ae35u;
		return double((i ^ (i >> 16)) & 32767u) / 32768.0;
	}
	return double((i * 7919u + 12345u) % 32768u) / 32768.0;
}
double RandDouble()
{
	return sample(random_index++);
}
int Rand(int limit)
{
	if (!bounded_script.empty()) return bounded_script.at(bounded_index++) % limit;
	return int(sample(random_index++) * 32768) % limit;
}
double DRound(double value)
{
	return std::round(value);
}
#define Max(a, b)     ((a) > (b) ? (a) : (b))
#define FixDiv(a, b)  (int(double(a) / double(b) * 65536.0))
#define mul64(a, b)   (int64_t(a) * int64_t(b))
#define OBJECT(index) (&object)
#define ENTER(...)
#define RETVAL(value)         return (value);
#define RETURN                return;
#define I2X(value)            ((value) * 65536)
#define X2D(value)            (double(value) / 65536.0)
#define X2F(value)            (float(value) / 65536.0f)
#define OBJPOS(obj)           (&(obj)->position)
#define SHOW_LIGHTNING(index) true
#include "xl_reference_lightning_path.h"
struct Bolts {
	std::vector<CLightning> values;
	CLightning *Buffer()
	{
		return values.data();
	}
	CLightning *operator+(int i)
	{
		return values.data() + i;
	}
	CLightning &operator[](int i)
	{
		return values.at(i);
	}
};
struct CLightningEmitter {
	Bolts m_lightning;
	int m_bValid = 1, m_nObject = 307, m_nBolts = 0, m_bDestroy = 0, m_tUpdate = -1, m_nKey[2]{};
	unsigned sounds = 0;
	void Animate(int32_t, int32_t, int32_t);
	int32_t SetLife();
	int32_t Update(int32_t);
	void Destroy()
	{
		unsupported();
	}
	void UpdateSound(int)
	{
		++sounds;
	}
	void MoveForObject();
	void Move(CFixVector, int16_t);
};
struct {
	void Destroy(CLightningEmitter *, void *)
	{
		unsupported();
	}
} lightningManager;
#include "xl_reference_lightning_emitter.h"
#undef ENTER
#undef RETURN
#undef STYLE
#undef I2X
#undef X2D
#undef X2F
#undef OBJPOS
#undef SHOW_LIGHTNING
#undef RETVAL
#undef OBJECT
#undef mul64
#undef FixDiv
#undef Max

CFixVector vector(const xl_compat::LightningPoint &point)
{
	CFixVector result;
	result.Set(point[0], point[1], point[2]);
	return result;
}
xl_compat::LightningPoint point(const CFixVector &vector)
{
	return { vector.v.coord.x, vector.v.coord.y, vector.v.coord.z };
}
void verify(const xl_compat::LightningNode &node, const CLightningNode &source)
{
	if (node.base != point(source.m_vBase) || node.position != point(source.m_vPos) || node.next != point(source.m_vNewPos) ||
	    node.offset != point(source.m_vOffs) || node.delta != std::array<int, 2>{ source.m_nDelta[0], source.m_nDelta[1] })
		throw std::runtime_error("Lightning path coordinates or displacement history differ from pinned source");
}
} // namespace lightning_path_reference

void lightning_path_controls()
{
	using namespace lightning_path_reference;
	using namespace xl_compat;
	unsigned paths = 0, animations = 0;
	for (int count : { 5, 9, 24 })
		for (int frames : { 1, 2, 7 })
			for (int amplitude : { 0, 65536, 5 * 65536 })
				for (bool plane : { false, true }) {
					std::vector<LightningNode> nodes(count);
					CLightning source;
					source.m_nodes.values.resize(count);
					source.m_nNodes = count;
					source.m_nFrames = frames;
					source.m_nAmplitude = amplitude;
					source.m_bInPlane = plane;
					const std::array<LightningPoint, 2> basis{ { { 61793, -10923, 0 }, { -2341, -4013, 65500 } } };
					CFixVector oracle_basis[2] = { vector(basis[0]), vector(basis[1]) };
					unsigned native_index = 0;
					random_index = 0;
					for (int restart = 0; restart < 2; ++restart) {
						for (int i = 0; i < count; ++i) {
							const LightningPoint position{ -327685 + i * 15234 + restart * 713, 127051 - i * 2317, -81239 + i * 98372 };
							lightning_node_setup(nodes[i], position, basis);
							auto pos = vector(position);
							source.m_nodes[i].Setup(!restart, &pos, oracle_basis);
							verify(nodes[i], source.m_nodes[i]);
						}
						source.m_vPos = source.m_nodes[0].m_vPos;
						source.m_vEnd = source.m_nodes[count - 1].m_vPos;
						for (int cycle = 0; cycle < 4; ++cycle) {
							source.CreatePath(1, 0);
							lightning_random_erratic_path(nodes, frames, amplitude, plane, [&] { return sample(native_index++); });
							if (random_index != native_index) throw std::runtime_error("Lightning path random ordering differs");
							for (int i = 0; i < count; ++i) verify(nodes[i], source.m_nodes[i]);
							++paths;
							for (int frame = 0; frame < frames; ++frame) {
								const bool initial = !cycle && !frame;
								for (int i = 1; i < count; ++i) source.m_nodes[i].Animate(initial, 362, 1, 0);
								lightning_animate_nodes(nodes, initial, true);
								for (int i = 0; i < count; ++i) verify(nodes[i], source.m_nodes[i]);
								++animations;
							}
						}
					}
				}
	std::cout << "Pinned lightning geometry: " << paths << " paths and " << animations << " interpolation frames passed\n";
}

void lightning_cone_boundaries()
{
	using namespace lightning_path_reference;
	using namespace xl_compat;
	for (int angle : { 1, 4, 30, 90, 127 }) {
		// Zero vector retry, opposite direction, a near-boundary rejection, accepted axis, then perpendicular plane
		bounded_script = { 90, 90, 90, 90, 90, 180 };
		if (angle <= 4) bounded_script.insert(bounded_script.end(), { 63, 90, 3 });
		if (angle == 90) bounded_script.insert(bounded_script.end(), { 0, 90, 90 });
		else bounded_script.insert(bounded_script.end(), { 90, 90, 0 });
		bounded_script.insert(bounded_script.end(), { 90, 0, 90 });
		bounded_index = 0;
		CLightning source;
		source.m_nAngle = angle;
		source.m_nNodes = 36;
		source.m_bRandom = 6;
		source.m_nFrames = -4;
		source.m_nAmplitude = 6 * 65536;
		source.m_vBase = vector({ 0, 0, 0 });
		source.m_vRefEnd = source.m_vEnd = vector({ 0, 0, 19 * 65536 });
		source.m_nodes.values.resize(36);
		source.Setup(true);
		struct Random {
			unsigned index = 0;
			int bounded(int limit)
			{
				return bounded_script.at(index++) % limit;
			}
			double unit()
			{
				throw std::runtime_error("Unexpected cone setup unit sample");
			}
		} random;
		LightningBolt bolt{};
		bolt.definition.angle = angle;
		bolt.reference_end = { 0, 0, 19 * 65536 };
		bolt.lifetime = { -300, 300, 300, 36, 6, 18 * 65536, 0, -4, 0, false };
		bolt.nodes.resize(36);
		lightning_bolt_setup(bolt, random);
		if (random.index != bounded_index || random.index != bounded_script.size() || bolt.direction != point(source.m_vDir) || bolt.end != point(source.m_vEnd))
			throw std::runtime_error("Directed lightning threshold or rejected-sample sequence differs");
		for (unsigned i = 0; i < 36; ++i) verify(bolt.nodes[i], source.m_nodes[i]);
		if (angle == 90 && lightning_dot({ 0, 0, 65536 }, bolt.direction) != 0)
			throw std::runtime_error("Directed lightning rejected equality at its cone threshold");
	}
	bounded_script.clear();
	Object owner{};
	owner.render = 12;
	owner.render_data = { 300, 30, 20, 6, 2, -1, 3, 2, 1, 30, 5, 4, 3, 4, 1, 1, 1, 0, 1, 1, 0, 115, 25, 25, 200, 1 };
	const auto definition = lightning_definition(owner, 0);
	struct NoRandom {
		unsigned calls = 0;
		double unit()
		{
			++calls;
			return 0;
		}
		int bounded(int)
		{
			++calls;
			return 0;
		}
	} random;
	for (int variant = 0; variant < 5; ++variant) {
		auto invalid = definition;
		std::optional<LightningPoint> end = LightningPoint{ 0, 0, 65536 };
		if (variant == 0) end.reset();
		if (variant == 1) end = LightningPoint{};
		if (variant == 2) invalid.angle = -1;
		if (variant == 3) invalid.angle = 128;
		if (variant == 4) invalid.in_plane = true;
		bool rejected = false;
		try {
			(void) create_lightning_bolt(invalid, {}, random, end);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected || random.calls) throw std::runtime_error("Malformed directed bolt consumed RNG or was admitted");
	}
	LightningBolt degenerate{};
	degenerate.definition = definition;
	degenerate.lifetime.random_nodes = 6;
	bool rejected = false;
	try {
		lightning_bolt_setup(degenerate, random);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	if (!rejected || random.calls) throw std::runtime_error("Coincident restart did not fail before random sampling");
	std::cout << "Pinned directed lightning: five cone boundaries, zero/rejected samples and threshold equality passed\n";
}

void lightning_bolt_controls()
{
	lightning_cone_boundaries();
	using namespace lightning_path_reference;
	using namespace xl_compat;
	unsigned ticks = 0;
	for (unsigned seed : { 0u, 17u, 991u })
		for (int frames : { 2, 3, 4, 7 })
			for (int delay : { 0, 30, 50, -50 })
				for (int offset : { 0, 1, 2 })
					for (int profile : { 0, 1, 2, 3, 4 }) {
						cone_samples = profile == 4;
						Object object{};
						object.render = 12;
						object.render_data = { 300, delay, 18, 5, offset, -1, 3, 0, 0, 8, 0, frames, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, 1 };
						if (profile == 1) object.render_data = { 2000, delay, 30, 5, offset, -1, 50, 0, 0, 50, 5, frames, 3, 0, 1, 1, 1, 0, 0, 1, 0, 32, 64, 255, 100, 1 };
						if (profile >= 2) object.render_data = { 4500, delay, 75, 13, offset, -1, 5, 1, 2, 30, 2, frames, 3, 0, 2, 1, 1, 1, 0, 0, 0, 255, 38, 76, 100, 1 };
						if (profile == 4) object.render_data = { 300, delay, 19, 6, offset, -1, 3, 2, 1, 30, 5, frames, 3, 4, 1, 1, 1, 0, 1, 1, 0, 115, 25, 25, 200, 1 };
						const auto definition = lightning_definition(object, authored_lightning_global_style);
						const LightningPoint origin = profile == 4 ? LightningPoint{ 22686019, 6375183, 43982521 } : LightningPoint{ -2621440, -33182233, 6619136 };
						const std::optional<LightningPoint> endpoint = profile == 4 ? std::optional<LightningPoint>({ 22449866, 7653026, 43981970 }) : profile >= 2 ? std::optional<LightningPoint>({ origin[0], origin[1] + (profile == 2 ? 75 : -75) * 65536, origin[2] })
						                                                                                                                                            : std::nullopt;
						struct Random {
							unsigned index;
							double unit()
							{
								return sample(index++);
							}
							int bounded(int limit)
							{
								return int(sample(index++) * 32768) % limit;
							}
						} random{ seed };
						// Initial fields have their separate extracted Init comparison
						CLightning source;
						source.m_bRandom = !endpoint || definition.angle > 0 ? definition.nodes / 5 : 0;
						source.m_nAngle = definition.angle;
						source.m_nNodes = definition.nodes + source.m_bRandom;
						source.m_nLife = definition.life;
						source.m_nTTL = std::abs(definition.life);
						source.m_nStyle = definition.style;
						source.m_nSmoothe = definition.smooth;
						source.m_bClamp = definition.clamp;
						source.m_nFrames = -frames;
						source.m_nAmplitude = definition.amplitude;
						source.m_nLength = definition.length * source.m_nNodes / definition.nodes;
						source.m_nDelay = std::abs(delay) * 10;
						source.m_nOffset = definition.offset;
						source.m_vBase = vector(origin);
						if (endpoint) source.m_vEnd = source.m_vRefEnd = vector(*endpoint);
						random_index = seed;
						if (!source.Create(0, 0)) throw std::runtime_error("Pinned bolt creation failed");
						auto bolt = create_lightning_bolt(definition, origin, random, endpoint);
						const auto verify_bolt = [&] {
							const auto &s = bolt.lifetime;
							if (s.ttl != source.m_nTTL || s.nodes != source.m_nNodes || s.length != source.m_nLength || s.frames != source.m_nFrames || s.step != source.m_iStep ||
							    bolt.reference_end != point(source.m_vRefEnd) || bolt.position != point(source.m_vPos) || bolt.end != point(source.m_vEnd) || bolt.direction != point(source.m_vDir) || random.index != random_index)
								throw std::runtime_error("Lightning bolt placement, lifetime or random visits differ from pinned source");
							for (unsigned i = 0; i < bolt.nodes.size(); ++i) {
								verify(bolt.nodes[i], source.m_nodes[i]);
								for (unsigned axis = 0; axis < 2; ++axis)
									if (bolt.nodes[i].plane[axis] != point(source.m_nodes[i].m_vDelta[axis])) throw std::runtime_error("Lightning bolt plane differs");
							}
							if (bolt.draw_positions.size() != source.drawn.size()) throw std::runtime_error("Lightning render staging differs");
							for (unsigned i = 0; i < bolt.draw_positions.size(); ++i)
								if (bolt.draw_positions[i] != point(source.drawn[i])) throw std::runtime_error("Lightning staged draw coordinates differ");
						};
						verify_bolt();
						for (int frame = 0; frame < (profile >= 2 ? 400 : 160); ++frame) {
							gameStates.app.tick40fps.nTime = frame % 3 == 0 ? 31 : 25;
							source.Animate(0, 0);
							if (!source.SetLife() || !lightning_bolt_frame(bolt, gameStates.app.tick40fps.nTime, random)) throw std::runtime_error("Persistent lightning expired");
							verify_bolt();
							++ticks;
						}
					}
	cone_samples = false;
	std::cout << "Pinned lightning bolts: " << ticks << " complete create/frame/restart ticks passed\n";
}

void lightning_emitter_controls()
{
	using namespace lightning_path_reference;
	using namespace xl_compat;
	unsigned ticks = 0;
	for (unsigned seed : { 0u, 17u, 991u })
		for (int count : { 1, 3, 7 })
			for (int profile : { 0, 1, 2 }) {
				const bool fixed = profile == 1, cone = profile == 2;
				cone_samples = cone;
				Object authored{};
				authored.render = 12;
				authored.render_data = { 300, 50, 18, 5, 1, -1, count, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, 1 };
				if (fixed) authored.render_data = { 4500, 0, 75, 13, 1, -1, count, 1, 2, 30, 2, 7, 3, 0, 2, 1, 1, 1, 0, 0, 0, 255, 38, 76, 100, 1 };
				if (cone) authored.render_data = { 300, 30, 19, 6, 2, -1, count, 2, 1, 30, 5, 4, 3, 4, 1, 1, 1, 0, 1, 1, 0, 115, 25, 25, 200, 1 };
				const auto definition = lightning_definition(authored, 2);
				const LightningPoint origin{ -2621440, -33182233, 6619136 };
				const std::optional<LightningPoint> endpoint = fixed || cone ? std::optional<LightningPoint>({ origin[0], origin[1] + 75 * 65536, origin[2] }) : std::nullopt;
				struct Random {
					unsigned index;
					double unit()
					{
						return sample(index++);
					}
					int bounded(int limit)
					{
						return int(sample(index++) * 32768) % limit;
					}
				} random{ seed };
				random_index = seed;
				CLightningEmitter source;
				source.m_nBolts = count;
				source.m_lightning.values.resize(count);
				for (auto &bolt : source.m_lightning.values) {
					bolt.m_bRandom = fixed ? 0 : definition.nodes / 5;
					bolt.m_nAngle = definition.angle;
					bolt.m_nNodes = definition.nodes + bolt.m_bRandom;
					bolt.m_nFrames = -definition.frames;
					bolt.m_nLife = definition.life;
					bolt.m_nTTL = -definition.life;
					bolt.m_nDelay = std::abs(definition.delay) * 10;
					bolt.m_nStyle = definition.style;
					bolt.m_nSmoothe = definition.smooth;
					bolt.m_bClamp = definition.clamp;
					bolt.m_nAmplitude = definition.amplitude;
					bolt.m_nLength = definition.length * bolt.m_nNodes / definition.nodes;
					bolt.m_nOffset = definition.offset;
					bolt.m_vBase = vector(origin);
					if (endpoint) bolt.m_vEnd = bolt.m_vRefEnd = vector(*endpoint);
					if (!bolt.Create(0, 0)) throw std::runtime_error("Pinned emitter bolt creation failed");
				}
				object.position.vPos = vector(origin);
				object.info.nSegment = 362;
				auto emitter = create_lightning_emitter(definition, origin, random, 362, endpoint);
				int time = 1000, last_tick = 1000;
				unsigned updates = 0;
				for (int frame = 0; frame < (fixed ? 800 : 240); ++frame) {
					if (frame % 13 == 0) {
						object.position.vPos.v.coord.x += 17231;
						object.position.vPos.v.coord.y -= 3127;
						object.info.nSegment = 362 + frame % 3;
					}
					time += std::array<int, 3>{ 7, 11, 31 }[frame % 3];
					gameStates.app.nSDLTicks[0] = time;
					gameStates.app.tick40fps.nTime = time - last_tick;
					if (time - last_tick >= 25) last_tick = time;
					emitter.enabled = !(frame >= 40 && frame < 60) && !(frame >= 100 && frame < 110);
					source.m_bValid = emitter.enabled ? 1 : -1;
					if (source.Update(0) != count) throw std::runtime_error("Pinned emitter lost a persistent bolt");
					if (lightning_emitter_frame(emitter, time, gameStates.app.tick40fps.nTime, random)) {
						++updates;
						lightning_emitter_move(emitter, point(object.position.vPos), object.info.nSegment);
					}
					if (emitter.updated != source.m_tUpdate || random.index != random_index || source.sounds != updates)
						throw std::runtime_error("Lightning emitter gate, two-pass random ordering or post-update scheduling differs");
					for (int i = 0; i < count; ++i) {
						const auto &bolt = emitter.bolts[i];
						const auto &expected = source.m_lightning[i];
						const auto &s = bolt.lifetime;
						if (s.ttl != expected.m_nTTL || s.nodes != expected.m_nNodes || s.length != expected.m_nLength || s.frames != expected.m_nFrames || s.step != expected.m_iStep ||
						    bolt.reference_end != point(expected.m_vRefEnd) || bolt.segment != expected.m_nSegment || bolt.base != point(expected.m_vBase) || bolt.position != point(expected.m_vPos) || bolt.end != point(expected.m_vEnd) || bolt.direction != point(expected.m_vDir) || bolt.draw_positions.size() != expected.drawn.size())
							throw std::runtime_error("Lightning emitter bolt state differs");
						for (unsigned n = 0; n < bolt.nodes.size(); ++n) verify(bolt.nodes[n], expected.m_nodes.values[n]);
						for (unsigned n = 0; n < bolt.draw_positions.size(); ++n)
							if (bolt.draw_positions[n] != point(expected.drawn[n])) throw std::runtime_error("Lightning emitter drawing state differs");
					}
					++ticks;
				}
			}
	cone_samples = false;
	std::cout << "Pinned lightning emitters: " << ticks << " multi-bolt update/disable/re-enable ticks passed\n";
}
