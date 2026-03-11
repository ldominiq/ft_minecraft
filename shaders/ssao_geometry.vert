#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in float aTexLayer;
layout (location = 4) in vec3 aNormal;

out vec3 FragPos;
out vec2 TexCoords;
flat out float TexLayer;
out vec3 Normal;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 viewPos = view * vec4(aPos, 1.0);
    FragPos = viewPos.xyz; 
    TexCoords = aTexCoords;
    TexLayer = aTexLayer;
    
    Normal = normalize(mat3(view) * aNormal);
    
    gl_Position = projection * viewPos;
}

