#version 330 core

layout(location = 0) in vec3 aPos;     // quad vertex positions (-0.5..0.5)
layout(location = 1) in vec2 aTex;     // UVs (0..1)

uniform mat4 projection;
uniform mat4 view;
uniform vec3 propPosition;

out vec2 TexCoord;

void main()
{
    // Billboard so the quad faces the camera:
    mat3 camRotation = mat3(view);  // extract rotation part of view matrix
    vec3 billboardRight = vec3(camRotation[0][0], camRotation[1][0], camRotation[2][0]);
    vec3 billboardUp    = vec3(camRotation[0][1], camRotation[1][1], camRotation[2][1]);

    vec3 worldPos = propPosition 
                  + billboardRight * aPos.x 
                  + billboardUp    * aPos.y;

    gl_Position = projection * view * vec4(worldPos, 1.0);
    TexCoord = aTex;
}
