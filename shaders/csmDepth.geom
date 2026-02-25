#version 460 core

// Single-pass layered rendering: one invocation per cascade.
// With 2 split levels → 3 cascades.
layout(triangles, invocations = 3) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 lightSpaceMatrices[3];

void main()
{
    // gl_InvocationID selects which cascade we're rendering into.
    // gl_Layer routes the triangle to the correct texture array layer.
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = lightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}