//
// Created by lucas on 10/14/25.
//

#ifndef FT_MINECRAFT_GUI_HPP
#define FT_MINECRAFT_GUI_HPP

#include <glm/glm.hpp>

class GuiTexture {
public:
    explicit GuiTexture(int texture, glm::vec2 position, glm::vec2 scale);

    int getTexture() const { return texture; }
    glm::vec2 getPosition() const { return position; }
    glm::vec2 getScale() const { return scale; }

private:
    int texture;
    glm::vec2 position;
    glm::vec2 scale;
};


#endif //FT_MINECRAFT_GUI_HPP