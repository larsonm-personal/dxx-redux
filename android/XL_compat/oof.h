#ifndef DXX_XL_OOF_H
#define DXX_XL_OOF_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
using OofVector = std::array<float, 3>;
struct OofCorner {
	int vertex;
	float u, v;
};
struct OofFace {
	OofVector normal{};
	int texture = -1;
	std::array<uint8_t, 3> color{};
	std::vector<OofCorner> corners;
	float length = 0, width = 0;
};
struct OofSubmodel {
	int index = -1, parent = -1, movement = 0, axis = 0;
	OofVector normal{}, plane_vertex{}, offset{}, center{};
	float plane_distance = 0, radius = 0;
	int tree_offset = 0, data_offset = 0;
	std::string name, properties;
	std::vector<int> facing_lists;
	std::vector<OofVector> vertices, normals;
	std::vector<float> alpha;
	std::vector<OofFace> faces;
};
struct OofChunk {
	std::string type;
	std::vector<uint8_t> bytes;
};
struct OofSpecialPoint {
	std::string name, properties;
	OofVector position{};
	float radius = 0;
};
struct OofRotationFrame {
	int start = 0, angle = 0;
	OofVector axis{}, normalized_axis{};
};
struct OofRotationTrack {
	int first = 0, last = 0;
	std::vector<OofRotationFrame> frames;
};
struct OofModel {
	int version = 0;
	float radius = 0;
	OofVector low{}, high{};
	std::vector<int> detail_levels;
	std::vector<std::string> textures;
	std::map<int, OofSubmodel> submodels;
	// Preserve raw extension bytes even when typed metadata is qualified
	std::vector<OofChunk> additional_chunks;
	bool has_special_points = false, has_rotation_tracks = false;
	std::vector<OofSpecialPoint> special_points;
	std::vector<OofRotationTrack> rotation_tracks;
};

namespace oof_detail
{
class Reader
{
	const uint8_t *data;
	std::size_t size, position = 0;

  public:
	Reader(const uint8_t *bytes, std::size_t length) : data(bytes), size(length) {}
	std::size_t remaining() const
	{
		return size - position;
	}
	const uint8_t *take(std::size_t count)
	{
		if (count > remaining()) throw std::runtime_error("Truncated OOF record");
		const auto *result = data + position;
		position += count;
		return result;
	}
	int integer()
	{
		const auto *p = take(4);
		const uint32_t bits = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
		return bits <= INT32_MAX ? int(bits) : -1 - int(~bits);
	}
	int count(int limit)
	{
		const int result = integer();
		if (result < 0 || result > limit) throw std::runtime_error("OOF count exceeds bounds");
		return result;
	}
	float real()
	{
		const auto value = integer();
		float result;
		std::memcpy(&result, &value, 4);
		if (!std::isfinite(result)) throw std::runtime_error("Nonfinite OOF value");
		return result;
	}
	OofVector vector()
	{
		return { real(), real(), real() };
	}
	std::string string()
	{
		const auto length = count(4096);
		const auto *p = take(std::size_t(length));
		std::string result(reinterpret_cast<const char *>(p), std::size_t(length));
		if (!result.empty() && result.back() == '\0') result.pop_back();
		if (result.find('\0') != std::string::npos) throw std::runtime_error("Embedded NUL in OOF string");
		return result;
	}
	// Metadata uses source C strings with optional zero padding; resource names stay strict
	std::string metadata_string()
	{
		const auto length = count(4096);
		const auto *p = take(std::size_t(length));
		std::string result(reinterpret_cast<const char *>(p), std::size_t(length));
		const auto nul = result.find('\0');
		if (nul != std::string::npos) {
			if (result.find_first_not_of('\0', nul) != std::string::npos) throw std::runtime_error("Nonzero OOF metadata string padding");
			result.resize(nul);
		}
		return result;
	}
	void end()
	{
		if (remaining()) throw std::runtime_error("Unexpected bytes in OOF record");
	}
};

inline OofSubmodel submodel(Reader &r)
{
	OofSubmodel result;
	result.index = r.count(63);
	result.parent = r.integer();
	result.normal = r.vector();
	result.plane_distance = r.real();
	result.plane_vertex = r.vector();
	result.offset = r.vector();
	result.radius = r.real();
	result.tree_offset = r.integer();
	result.data_offset = r.integer();
	result.center = r.vector();
	result.name = r.string();
	result.properties = r.string();
	result.movement = r.integer();
	result.axis = r.integer();
	for (int count = r.count(65536); count; --count) result.facing_lists.push_back(r.integer());
	const int vertices = r.count(65536);
	for (int i = 0; i < vertices; ++i) result.vertices.push_back(r.vector());
	for (int i = 0; i < vertices; ++i) result.normals.push_back(r.vector());
	for (int i = 0; i < vertices; ++i) {
		const float alpha = r.real();
		if (alpha < 0 || alpha > 1) throw std::runtime_error("OOF vertex alpha outside range");
		result.alpha.push_back(alpha);
	}
	for (int count = r.count(65536); count; --count) {
		OofFace face;
		face.normal = r.vector();
		const int corners = r.count(1024);
		if (corners < 3) throw std::runtime_error("OOF face has fewer than three corners");
		const bool textured = r.integer() != 0;
		if (textured) face.texture = r.count(4095);
		else {
			const auto *rgb = r.take(3);
			std::copy(rgb, rgb + 3, face.color.begin());
		}
		for (int i = 0; i < corners; ++i) {
			const int vertex = r.integer();
			if (vertex < 0 || vertex >= vertices) throw std::runtime_error("OOF face references missing vertex");
			face.corners.push_back({ vertex, r.real(), r.real() });
		}
		face.length = r.real();
		face.width = r.real();
		result.faces.push_back(std::move(face));
	}
	r.end();
	return result;
}
} // namespace oof_detail

// Decode original records without renderer state or mission/global table mutation
inline OofModel read_oof(const std::vector<uint8_t> &bytes)
{
	if (bytes.size() < 8 || bytes.size() > 64 * 1024 * 1024) throw std::runtime_error("Invalid OOF size");
	oof_detail::Reader file(bytes.data(), bytes.size());
	if (std::memcmp(file.take(4), "PSPO", 4)) throw std::runtime_error("Invalid OOF signature");
	OofModel model;
	model.version = file.integer();
	if (model.version != 2300) throw std::runtime_error("Unsupported OOF version");
	int submodels = -1;
	bool textures = false;
	unsigned chunks = 0;
	while (file.remaining()) {
		if (++chunks > 1024) throw std::runtime_error("Too many OOF chunks");
		const std::string type(reinterpret_cast<const char *>(file.take(4)), 4);
		const auto length = std::size_t(file.count(64 * 1024 * 1024));
		const auto *data = file.take(length);
		oof_detail::Reader r(data, length);
		if (type == "TXTR") {
			if (textures) throw std::runtime_error("Duplicate OOF textures");
			textures = true;
			for (int count = r.count(4096); count; --count) model.textures.push_back(r.string());
			r.end();
		} else if (type == "OHDR") {
			if (submodels >= 0) throw std::runtime_error("Duplicate OOF header");
			submodels = r.count(64);
			model.radius = r.real();
			model.low = r.vector();
			model.high = r.vector();
			for (int count = r.count(64); count; --count) model.detail_levels.push_back(r.integer());
			r.end();
		} else if (type == "SOBJ") {
			auto submodel = oof_detail::submodel(r);
			const int index = submodel.index;
			if (!model.submodels.emplace(index, std::move(submodel)).second) throw std::runtime_error("Duplicate OOF submodel");
		} else model.additional_chunks.push_back({ type, { data, data + length } });
	}
	if (!textures || submodels < 1 || model.submodels.size() != std::size_t(submodels)) throw std::runtime_error("Incomplete OOF model");
	// Version 2300 uses timed RANI. These records are retained, not animated: the
	// pinned static OOF render paths do not consume their frame matrices or tick maps
	for (const auto &chunk : model.additional_chunks) {
		oof_detail::Reader r(chunk.bytes.data(), chunk.bytes.size());
		if (chunk.type == "SPCL") {
			if (model.has_special_points) throw std::runtime_error("Duplicate OOF special points");
			model.has_special_points = true;
			for (int n = r.count(4096); n; --n) {
				OofSpecialPoint point;
				point.name = r.metadata_string();
				point.properties = r.metadata_string();
				point.position = r.vector();
				point.radius = r.real();
				if (point.radius < 0) throw std::runtime_error("Negative OOF special point radius");
				model.special_points.push_back(std::move(point));
			}
			r.end();
		} else if (chunk.type == "RANI") {
			if (model.has_rotation_tracks) throw std::runtime_error("Duplicate OOF rotation tracks");
			model.has_rotation_tracks = true;
			std::size_t ticks = 0;
			for (int part = 0; part < submodels; ++part) {
				OofRotationTrack track;
				const auto count = r.count(256);
				track.first = r.integer();
				track.last = r.integer();
				const auto span = std::int64_t(track.last) - track.first + 1;
				if (span < 1 || span > 1048576 || ticks + std::size_t(span) > 1048576) throw std::runtime_error("OOF rotation tick range exceeds bounds");
				ticks += std::size_t(span);
				int previous = track.first;
				for (int i = 0; i < count; ++i) {
					OofRotationFrame frame;
					frame.start = r.integer();
					frame.axis = r.vector();
					frame.angle = r.integer();
					if (frame.start < previous || frame.start > track.last) throw std::runtime_error("OOF rotation frame outside ordered range");
					previous = frame.start;
					const float magnitude = std::sqrt(frame.axis[0] * frame.axis[0] + frame.axis[1] * frame.axis[1] + frame.axis[2] * frame.axis[2]);
					if (!std::isfinite(magnitude)) throw std::runtime_error("OOF rotation axis magnitude exceeds bounds");
					frame.normalized_axis = frame.axis;
					if (magnitude > 0)
						for (auto &axis : frame.normalized_axis) axis /= magnitude;
					track.frames.push_back(frame);
				}
				model.rotation_tracks.push_back(std::move(track));
			}
			r.end();
		}
	}

	for (const auto &entry : model.submodels) {
		const auto &submodel = entry.second;
		if (entry.first >= submodels) throw std::runtime_error("OOF submodel index outside header range");
		int parent = submodel.parent;
		if (parent == submodel.index) parent = -1; // Source treats a self parent as a root
		for (int depth = 0; parent != -1; ++depth) {
			const auto found = model.submodels.find(parent);
			if (found == model.submodels.end() || depth >= submodels) throw std::runtime_error("Invalid or cyclic OOF hierarchy");
			parent = found->second.parent == parent ? -1 : found->second.parent;
		}
		for (const auto &face : submodel.faces)
			if (face.texture >= int(model.textures.size())) throw std::runtime_error("OOF face references missing texture");
	}
	return model;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
