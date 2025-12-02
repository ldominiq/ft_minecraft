//
// Created by lucas on 10/14/25.
//

#ifndef FT_MINECRAFT_RAWMODEL_HPP
#define FT_MINECRAFT_RAWMODEL_HPP

#include <glad/glad.h>

class RawModel {
public:
    RawModel();
    RawModel(GLuint vaoID, int vertexCount);

    GLuint getVaoID() const { return vaoID; }

    int getVertexCount() const { return vertexCount; }

private:
    GLuint vaoID;
    int vertexCount;

};


#endif //FT_MINECRAFT_RAWMODEL_HPP