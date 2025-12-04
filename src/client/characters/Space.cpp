
#include "Space.hpp"

glm::mat4 extractScaleInverse(const glm::mat4& m)
{
    glm::vec3 scale(
        glm::length(glm::vec3(m[0])),
        glm::length(glm::vec3(m[1])),
        glm::length(glm::vec3(m[2]))
    );

    glm::vec3 invScale = 1.0f / scale;

    return glm::scale(glm::mat4(1.0f), invScale);
}

void Space::compute(const glm::mat4 &parentTransform, const glm::mat4& proj, const glm::mat4& view, const Shader &shader)
{
	// mat4 transform = parentTransform * rotation * scale * translation; * translation
	glm::mat4 transform = parentTransform * translation * extractScaleInverse(totalScale) * rotation * totalScale * scale;
	for (std::shared_ptr<Space> &child : childs)
	{
		child->totalScale = totalScale;
		child->compute(transform, proj, view, shader);
	}
}

void Space::drawScene(const Shader &shader, const glm::mat4& proj, const glm::mat4& view)
{
	for (std::shared_ptr<Space> &child : childs)
		child->drawScene(shader, proj, view);
}