#include "Typer.hpp"
#include <cmath>

Typer::Typer(const std::string& fontPath, unsigned int pixelSize) : shader("shaders/freetype.vert", "shaders/freetype.frag") {
    // Initialize FreeType
	FT_Library ft;
	if (FT_Init_FreeType(&ft))
	{
		std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
		return ;
	}

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return ;
    }
    else {
        // set size to load glyphs as
        FT_Set_Pixel_Sizes(face, 0, pixelSize);

        // disable byte-alignment restriction
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // load first 128 characters of ASCII set
        for (unsigned char c = 0; c < 128; c++)
        {
            // Load character glyph 
            if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                continue;
            }
            // generate texture
            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
            // set texture options
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // now store character for later use
            TypingCharacter character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<char, TypingCharacter>(c, character));
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    // destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // configure VAO/VBO for texture quads
    // -----------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

Typer::~Typer() {
    if (glfwGetCurrentContext()) {
        for (auto& [_, c] : Characters)
            glDeleteTextures(1, &c.TextureID);
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
    else {
        VAO = 0;
        VBO = 0;
    }
}

void Typer::setProjection(int width, int height) {
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));
    shader.use();
	shader.setMat4("projection", projection);
}

uint Typer::getPixelSizeOfString(const std::string &str)
{
    uint len = 0;
    for (auto &c : str)
    {
        TypingCharacter ch = Characters[c];
        len += (ch.Advance >> 6);
    }
    return static_cast<uint>(len * scale);
}

float Typer::getAscent()
{
    auto it = Characters.find('A');
    if (it == Characters.end()) return 0.0f;
    return it->second.Bearing.y * scale;
}

// render line of text (rotationDeg rotates around the starting anchor x,y)
void Typer::renderText(const std::string &text, float x, float y, const glm::vec3 &color, const float alpha /* = 1.0f */, float rotationDeg /* = 0.0f */)
{
    // activate corresponding render state
    shader.use();
	shader.setVec4("textColor", glm::vec4(color.x, color.y, color.z, alpha));

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    const float rad = rotationDeg * 3.14159265358979323846f / 180.0f;
    const float cs = std::cos(rad);
    const float sn = std::sin(rad);
    const float anchorX = x;
    const float anchorY = y;
    float penOffset = 0.0f; // offset along the (rotated) baseline

    auto rotatePoint = [&](float px, float py, float& ox, float& oy) {
        float dx = px - anchorX;
        float dy = py - anchorY;
        ox = anchorX + dx * cs - dy * sn;
        oy = anchorY + dx * sn + dy * cs;
    };

    // iterate through all characters
    for (auto c = text.begin(); c != text.end(); ++c)
    {
        TypingCharacter ch = Characters[*c];

        // position relative to anchor in un-rotated space
        float baseX = anchorX + penOffset + ch.Bearing.x * scale;
        float baseY = anchorY - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float tlX, tlY, blX, blY, brX, brY, trX, trY;
        rotatePoint(baseX,     baseY + h, tlX, tlY);
        rotatePoint(baseX,     baseY,     blX, blY);
        rotatePoint(baseX + w, baseY,     brX, brY);
        rotatePoint(baseX + w, baseY + h, trX, trY);

        float vertices[6][4] = {
            { tlX, tlY, 0.0f, 0.0f },
            { blX, blY, 0.0f, 1.0f },
            { brX, brY, 1.0f, 1.0f },

            { tlX, tlY, 0.0f, 0.0f },
            { brX, brY, 1.0f, 1.0f },
            { trX, trY, 1.0f, 0.0f }
        };
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        penOffset += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
