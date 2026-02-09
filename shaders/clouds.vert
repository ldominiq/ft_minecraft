#version 460 core

// Fullscreen triangle without VBO/attributes.
// Produces UV in [0,1] for convenience.
out vec2 vUV;

void main()
{
    // 3 verts: ( -1,-1 ), ( 3,-1 ), ( -1, 3 )
    vec2 pos;
    if (gl_VertexID == 0) pos = vec2(-1.0, -1.0);
    else if (gl_VertexID == 1) pos = vec2( 3.0, -1.0);
    else pos = vec2(-1.0,  3.0);

    vUV = 0.5 * (pos + 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);
}