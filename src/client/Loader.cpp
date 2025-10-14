//
// Created by lucas on 10/14/25.
//

#include "Loader.hpp"

RawModel Loader::loadToVAO(const std::vector<float>& positions) {
    const GLuint vaoID = createVAO();
    storeDataInAttributeList(0, 2, positions);
    unbindVAO();
    return RawModel(vaoID, positions.size() / 2);
}

GLuint Loader::createVAO() {
    GLuint vaoID;
    glGenVertexArrays(1, &vaoID);
    vaos.push_back(vaoID);
    glBindVertexArray(vaoID);
    return vaoID;
}

void Loader::storeDataInAttributeList(int attributeNumber, int coordinateSize, const std::vector<float>& data) {
    GLuint vboID;
    glGenBuffers(1, &vboID);
    vbos.push_back(vboID);
    glBindBuffer(GL_ARRAY_BUFFER, vboID);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
    glVertexAttribPointer(attributeNumber, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(attributeNumber);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Loader::unbindVAO() {
    glBindVertexArray(0);
}

void Loader::cleanUp() const {
    for (GLuint vao : vaos) {
        glDeleteVertexArrays(1, &vao);
    }
    for (GLuint vbo : vbos) {
        glDeleteBuffers(1, &vbo);
    }
}
