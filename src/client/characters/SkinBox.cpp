#include "SkinBox.hpp"

namespace
{
	inline glm::vec4 rect(int x, int y, int w, int h, float texW, float texH)
	{
		return glm::vec4(
			static_cast<float>(x) / texW,
			static_cast<float>(y) / texH,
			static_cast<float>(x + w) / texW,
			static_cast<float>(y + h) / texH
		);
	}
}

std::array<glm::vec4, 6> boxUVs(int u, int v, int w, int h, int d,
                                int texW, int texH)
{
	const float tW = static_cast<float>(texW);
	const float tH = static_cast<float>(texH);

	std::array<glm::vec4, 6> out{};
	//   0=+X(front) 1=-X(back) 2=+Y(top) 3=-Y(bottom) 4=-Z(right) 5=+Z(left)
	out[0] = rect(u + d,             v + d, w, h, tW, tH); // front
	out[1] = rect(u + d + w + d,     v + d, w, h, tW, tH); // back
	out[2] = rect(u + d,             v,     w, d, tW, tH); // top
	out[3] = rect(u + d + w,         v,     w, d, tW, tH); // bottom
	out[4] = rect(u,                 v + d, d, h, tW, tH); // right (character's right side)
	out[5] = rect(u + d + w,         v + d, d, h, tW, tH); // left
	return out;
}

std::array<glm::vec4, 6> boxUVsSlice(int u, int v, int w, int h, int d,
                                     int texW, int texH,
                                     float vTop, float vBottom)
{
	std::array<glm::vec4, 6> full = boxUVs(u, v, w, h, d, texW, texH);

	// For side faces, interpolate V between the full rect's top/bottom.
	auto slice = [vTop, vBottom](glm::vec4 r) {
		float vMin = r.y + (r.w - r.y) * vTop;
		float vMax = r.y + (r.w - r.y) * vBottom;
		return glm::vec4(r.x, vMin, r.z, vMax);
	};

	full[0] = slice(full[0]); // front
	full[1] = slice(full[1]); // back
	full[4] = slice(full[4]); // right
	full[5] = slice(full[5]); // left
	// top/bottom untouched
	return full;
}
