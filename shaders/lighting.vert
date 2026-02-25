#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in vec3 aNormal;
layout (location = 4) in float aSkyLight;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    float SkyLight;
} vs_out;

uniform mat4 projection;
uniform mat4 view;

// Clipping plane for water reflection/refraction
uniform vec4 clipPlane;

void main()  {
    vec4 worldPosition = vec4(aPos, 1.0);
    
    vs_out.FragPos = aPos;
    vs_out.Normal = aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.SkyLight = aSkyLight;
    gl_Position = projection * view * worldPosition;
    
    // Clip geometry based on plane (used for water reflection/refraction)
    gl_ClipDistance[0] = dot(worldPosition, clipPlane);
}