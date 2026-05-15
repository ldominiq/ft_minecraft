#ifndef SHAPE_HPP
#define SHAPE_HPP

#include <array>

#include "Space.hpp"

class Shape : public Space
{
	glm::vec3 color{0.0f,0.0f,1.0f};
	glm::mat4 transform = glm::mat4(1.0f);

	// Per-face UV rects in normalized skin-auth space. xy = (u_left, v_top),
	// zw = (u_right, v_bottom). Face indexing matches cube.cpp:
	//   0=+X(front) 1=-X(back) 2=+Y(top) 3=-Y(bot) 4=-Z(right) 5=+Z(left).
	// The manager decides (via uUseTexture) whether these get sampled this frame.
	std::array<glm::vec4, 6> faceUVs{};

	public:

		Shape(glm::vec3 color);
		~Shape();

		// The held-item renderer reads this from the right forearm to pin a weapon to
		// the actual rendered hand
		const glm::mat4& getTransform() const { return transform; }

		void compute(const glm::mat4 &parentTransform, const glm::mat4& proj, const glm::mat4& view, const Shader &shader) override;

		void draw(const glm::mat4 &mvp, const Shader &shader);
		void drawScene(const Shader &shader, const glm::mat4& proj, const glm::mat4& view) override;

		// Assign per-face UV rects computed via boxUVs(). Enables texture sampling for this Shape.
		void setSkinBox(const std::array<glm::vec4, 6>& uvs);
};

#endif