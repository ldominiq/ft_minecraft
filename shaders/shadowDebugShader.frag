#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform float near_plane;
uniform float far_plane;

// required when using a perspective projection matrix
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));	
}

void main()
{             
    float depthValue = texture(depthMap, TexCoords).r;

    // Debug: make depth more visible by remapping the range.
    // If the shadow map is written correctly, depthValue should be
    // somewhere in [0,1]. But it might be clustered in a tiny range
    // (e.g., 0.98-1.0), making everything look black/white.
    
    // Option 1: Raw depth (what you had)
    // FragColor = vec4(vec3(depthValue), 1.0);
    
    // Option 2: Amplify contrast — if depth is all near 1.0, this
    // will stretch the visible range
    FragColor = vec4(vec3(pow(depthValue, 100.0)), 1.0);
    
    // Option 3: Show red if ANY depth was written (sanity check)
    // Uncomment this to verify the texture has data at all:
     if (depthValue < 1.0)
         FragColor = vec4(1.0, 0.0, 0.0, 1.0);  // red = has geometry
     else
         FragColor = vec4(0.0, 0.0, 1.0, 1.0);  // blue = empty (depth=1.0)

    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / far_plane), 1.0); // perspective
    //FragColor = vec4(vec3(depthValue), 1.0); // orthographic
}

