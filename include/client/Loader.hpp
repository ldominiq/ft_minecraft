//
// Created by lucas on 10/14/25.
//

#ifndef FT_MINECRAFT_LOADER_HPP
#define FT_MINECRAFT_LOADER_HPP
#include <vector>
#include <glad/glad.h>

#include "RawModel.hpp"


class Loader {
public:
    RawModel loadToVAO(const std::vector<float> &positions);
    void cleanUp() const;

private:
    std::vector<GLuint> vaos;
    std::vector<GLuint> vbos;
    GLuint createVAO();
    void storeDataInAttributeList(int attributeNumber, int coordinateSize, const std::vector<float> &data);

    void unbindVAO();
};


#endif //FT_MINECRAFT_LOADER_HPP