//
// Created by lucas on 10/14/25.
//

#include "RawModel.hpp"

RawModel::RawModel() : vaoID(0), vertexCount(0) {
}

RawModel::RawModel(const GLuint vaoID, const int vertexCount) : vaoID(vaoID), vertexCount(vertexCount) {

}
