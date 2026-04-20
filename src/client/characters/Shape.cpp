
#include "Shape.hpp"

Shape::Shape(glm::vec3 color) : Space()
{
	this->color = color;
}

Shape::~Shape() {}

//assumes the shader is already in use
void Shape::draw(const glm::mat4 &mvp, const Shader &shader)
{
	shader.setMat4("uMVP", mvp);
	shader.setVec3("uColor", color);
	drawCube();
}

void Shape::drawScene(const Shader &shader, const glm::mat4& proj, const glm::mat4& view)
{
	glm::mat4 pvm = proj * view * transform;
	draw(pvm, shader);
	for (std::shared_ptr<Space> &child : childs)
		child->drawScene(shader, proj, view);
}

void Shape::compute(const glm::mat4 &parentTransform, const glm::mat4& proj, const glm::mat4& view, const Shader &shader)
{
	transform = parentTransform * extractScaleInverse(totalScale) * rotation * totalScale * scale * translation;

	glm::mat4 pvm = proj * view * transform;
	draw(pvm, shader);

	totalScale = totalScale * scale;
	for (std::shared_ptr<Space> &child : childs)
	{
		child->totalScale = totalScale;
		child->compute(transform, proj, view, shader);
	}
}
