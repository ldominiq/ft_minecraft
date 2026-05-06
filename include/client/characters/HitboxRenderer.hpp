
#ifndef HITBOX_RENDERER_HPP
#define HITBOX_RENDERER_HPP

#include <memory>
#include <glm/glm.hpp>

#include "Shader.hpp"
#include "LivingEntity.hpp"

class HitboxRenderer {
public:
    HitboxRenderer();
    ~HitboxRenderer();

    // draw AABB as wireframe lines (color RGB)
    void drawAABB(const AABB& box, const glm::mat4& view, const glm::mat4& proj,
                  const glm::dvec3& eyePos, const glm::vec3& color = glm::vec3(1.0f));

private:
    std::shared_ptr<Shader> shader;
    GLuint vao = 0;
    GLuint vbo = 0;
};

#endif