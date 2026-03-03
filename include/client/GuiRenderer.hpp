//
// Created by lucas on 10/14/25.
//

#ifndef FT_MINECRAFT_GUIRENDERER_HPP
#define FT_MINECRAFT_GUIRENDERER_HPP

#include <memory>
#include <glad/glad.h>
#include "GuiTexture.hpp"
#include "Loader.hpp"
#include "Shader.hpp"


class GuiRenderer {
public:
    explicit GuiRenderer(Loader loader);

    void render(const std::vector<GuiTexture> &guis, float nearPlane, float farPlane);
private:
    RawModel quad;
    std::shared_ptr<Shader> shader;
};


#endif //FT_MINECRAFT_GUIRENDERER_HPP