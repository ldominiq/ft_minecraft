#ifndef CUBE_H
#define CUBE_H

#include <glad/glad.h>

// Creates the cube VAO/VBO/EBO (call once)
void createCube();
// Destroys buffers
void destroyCube();

void drawCube();


#endif
