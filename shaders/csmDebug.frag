#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2DArray depthMaps;
uniform int   layer;
uniform float nearPlane;
uniform float farPlane;

void main()
{
    float depthValue = texture(depthMaps, vec3(TexCoords, float(layer))).r;

    // Cascade color tint for easy identification
    vec3 cascadeColor;
    if      (layer == 0) cascadeColor = vec3(1.0, 0.4, 0.4); // red
    else if (layer == 1) cascadeColor = vec3(0.4, 1.0, 0.4); // green
    else if (layer == 2) cascadeColor = vec3(0.4, 0.4, 1.0); // blue
    else if (layer == 3) cascadeColor = vec3(1.0, 1.0, 0.4); // yellow
    else                 cascadeColor = vec3(1.0, 0.4, 1.0); // magenta

    // Empty (depth == 1.0 or very close): dark background
    if (depthValue > 0.9999)
    {
        FragColor = vec4(vec3(0.08), 1.0);
        return;
    }

    // Orthographic shadow maps store depth linearly in [0,1].
    // Remap to [0,1] with near=white, far=dark so terrain is clearly visible.
    // Invert so closer geometry is brighter.
    float brightness = 1.0 - depthValue;

    // Boost contrast a bit so the terrain pops out
    brightness = clamp(brightness * 2.0, 0.0, 1.0);

    // Apply cascade tint
    vec3 color = cascadeColor * (0.15 + 0.85 * brightness);

    FragColor = vec4(color, 1.0);
}
