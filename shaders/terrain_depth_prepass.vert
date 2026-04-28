#version 460 core
// Z-prepass for terrain. Reads the same VAO as lighting.vert (location 0 = pos,
// location 1 = uv, location 2 = layer). Emits position + texture coords only —
// just enough to alpha-test leaves so the depth buffer matches the color pass
// exactly (required for glDepthFunc(GL_EQUAL) to work).
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aTexLayer;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;
uniform vec3 chunkOriginWorld;
uniform vec4 clipPlane;

out vec2 TexCoord;
flat out float TexLayer;

void main() {
    vec3 cameraRelPos = chunkRel + aPos;
    TexCoord = aTexCoord;
    TexLayer = aTexLayer;
    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // Same clip plane as lighting.vert — the prepass must clip identically
    // to the color pass or GL_EQUAL fails along the water surface.
    gl_ClipDistance[0] = dot(vec4(chunkOriginWorld + aPos, 1.0), clipPlane);
}
