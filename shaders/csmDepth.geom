#version 460 core

// One invocation per cascade. With 3 split levels → 4 cascades.
layout(triangles, invocations = 4) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 lightSpaceMatrices[4];

void main()
{
    // gl_InvocationID tells us which cascade we're rendering into.
    // gl_Layer selects which layer of the texture array to write to.
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = lightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}