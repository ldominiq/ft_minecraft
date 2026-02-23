//
// Created by lucas on 10/14/25.
//

#ifndef FT_MINECRAFT_GUI_HPP
#define FT_MINECRAFT_GUI_HPP

#include <glm/glm.hpp>

class GuiTexture {
public:
    explicit GuiTexture(int texture, glm::vec2 position, glm::vec2 scale, bool fbo = false);

    int getTexture() const { return texture; }
    glm::vec2 getPosition() const { return position; }
    glm::vec2 getScale() const { return scale; }
    bool getIsFBO() const { return isFBO; }

private:
    int texture;
    glm::vec2 position;
    glm::vec2 scale;
    bool isFBO = false;
};


#endif //FT_MINECRAFT_GUI_HPP