
#include "Shape.hpp"

Shape::Shape(glm::vec3 color) : Space()
{
	this->color = color;
}

Shape::~Shape() {}

void Shape::setSkinBox(const std::array<glm::vec4, 6>& uvs)
{
	faceUVs = uvs;
}

//assumes the shader is already in use
void Shape::draw(const glm::mat4 &mvp, const Shader &shader)
{
    shader.setMat4("uModelRel", mvp);
	shader.setVec3("uColor", color);
	GLint loc = glGetUniformLocation(shader.ID, "uFaceUVs");
	if (loc >= 0)
		glUniform4fv(loc, 6, &faceUVs[0].x);
	drawCube();
}

void Shape::drawScene(const Shader &shader, const glm::mat4& proj, const glm::mat4& view)
{
    (void)proj;
	(void)view;
	draw(transform, shader);
	for (std::shared_ptr<Space> &child : childs)
		child->drawScene(shader, proj, view);
}

void Shape::compute(const glm::mat4 &parentTransform, const glm::mat4& proj, const glm::mat4& view, const Shader &shader)
{
	transform = parentTransform * extractScaleInverse(totalScale) * rotation * totalScale * scale * translation;

    (void)proj;
	(void)view;
	draw(transform, shader);

	totalScale = totalScale * scale;
	for (std::shared_ptr<Space> &child : childs)
	{
		child->totalScale = totalScale;
		child->compute(transform, proj, view, shader);
	}
}
