#ifndef SHAPE_HPP
#define SHAPE_HPP

#include "Space.hpp"

class Shape : public Space
{
	glm::vec3 color{0.0f,0.0f,1.0f};
	glm::mat4 transform = glm::mat4(1.0f);

	public:

		Shape(glm::vec3 color);
		~Shape();

		void compute(const glm::mat4 &parentTransform, const glm::mat4& proj, const glm::mat4& view, const Shader &shader) override;

		void draw(const glm::mat4 &mvp, const Shader &shader);
		void drawScene(const Shader &shader, const glm::mat4& proj, const glm::mat4& view) override;
};

#endif