/*
 * Smooth-line geometry/coverage and clipping adapted from Mesa
 * draw_pipe_aaline.c and draw_pipe_clip.c
 * Copyright 2007-2018 VMware, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef DXX_XL_LIGHTNING_LINE_H
#define DXX_XL_LIGHTNING_LINE_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "lightning_render.h"
namespace xl_compat
{
struct LightningLineVertex {
	std::array<float, 4> position, coverage;
};
inline bool lightning_clip_line(std::array<float, 4> &a, std::array<float, 4> &b)
{
	float first = 0, last = 0;
	for (unsigned axis = 0; axis < 3; ++axis)
		for (float sign : { 1.f, -1.f }) {
			const float da = a[3] + sign * a[axis], db = b[3] + sign * b[axis];
			if (da < 0 && db < 0) return false;
			if (da < 0) first = (std::max) (first, da / (da - db));
			if (db < 0) last = (std::max) (last, db / (db - da));
			if (first + last >= 1) return false;
		}
	const auto original_a = a, original_b = b;
	for (unsigned i = 0; i < 4; ++i) {
		if (first) a[i] = original_a[i] + first * (original_b[i] - original_a[i]);
		if (last) b[i] = original_b[i] + last * (original_a[i] - original_b[i]);
	}
	return a[3] > 0 && b[3] > 0;
}
inline std::vector<LightningLineVertex> lightning_line_triangles(const std::vector<LightningVertex> &positions, std::size_t count,
                                                                 float width, const std::array<float, 16> &matrix, const std::array<int, 4> &viewport,
                                                                 const std::array<int, 4> &raster_viewport)
{
	if (count > positions.size() || viewport[2] <= 0 || viewport[3] <= 0 || !std::isfinite(width))
		throw std::runtime_error("Lightning line has invalid raster dimensions");
	const float half_width = width <= 1 ? 1 : width * .5f + .5f;
	std::vector<LightningLineVertex> result;
	for (std::size_t segment = 1; segment < count; ++segment) {
		auto a = lightning_clip_vertex(positions[segment - 1], {}, matrix).position;
		auto b = lightning_clip_vertex(positions[segment], {}, matrix).position;
		for (unsigned i = 0; i < 4; ++i)
			if (!std::isfinite(a[i]) || !std::isfinite(b[i])) throw std::runtime_error("Lightning line clip coordinate is not finite");
		if (!lightning_clip_line(a, b)) continue;
		for (auto *p : { &a, &b }) {
			const float reciprocal = 1.f / (*p)[3];
			for (unsigned axis = 0; axis < 3; ++axis) (*p)[axis] *= reciprocal;
			for (unsigned axis = 0; axis < 2; ++axis) (*p)[axis] = (*p)[axis] * (float(viewport[axis + 2]) * .5f) + (float(viewport[axis]) + float(viewport[axis + 2]) * .5f);
		}
		const float dx = b[0] - a[0], dy = b[1] - a[1];
		const float length = std::sqrt(dx * dx + dy * dy);
		if (!length) continue;
		const float along_x = dx / length, along_y = dy / length;
		const float half_length = length * .5f + .5f;
		std::array<LightningLineVertex, 4> quad;
		for (unsigned i = 0; i < 4; ++i) {
			const bool end = i >= 2;
			const float along = end ? .5f : -.5f;
			const float across = (i & 1) ? -half_width : half_width;
			auto &vertex = quad[i];
			vertex.position = end ? b : a;
			vertex.position[0] += along * along_x - across * along_y;
			vertex.position[1] += along * along_y + across * along_x;
			for (unsigned axis = 0; axis < 2; ++axis)
				vertex.position[axis] = (vertex.position[axis] - float(raster_viewport[axis])) / (float(raster_viewport[axis + 2]) * .5f) - 1.f;
			// Geometry is already projected; retain reciprocal w for coverage only
			vertex.position[3] = 1.f / vertex.position[3];
			vertex.coverage = { -across, half_width, end ? half_length : -half_length, half_length };
		}
		for (unsigned i : { 2u, 1u, 0u, 3u, 1u, 2u }) result.push_back(quad[i]);
	}
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
