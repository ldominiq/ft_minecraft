#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
    float IsUnderwater;
} fs_in;

out vec4 FragColor;

uniform sampler2DArray blockTextures;
uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

void main() {
    // Sample texture from array (premultiplied alpha)
    vec4 texColor = texture(blockTextures, vec3(fs_in.TexCoord, fs_in.TexLayer));

    // Discard transparent pixels — threshold raised to catch semi-transparent
    // mipmap edge pixels that would otherwise occlude terrain behind
    if (texColor.a < 0.5)
        discard;

    // Unpremultiply alpha to get original colors (only for semi-transparent pixels)
    // For opaque or nearly-opaque pixels (alpha > 0.95), skip to avoid precision issues
    if (texColor.a > 0.01 && texColor.a < 0.95) {
        texColor.rgb /= texColor.a;
    }

    // Use a fixed upward normal for vegetation to avoid angle-dependent
    // brightness from cross-pattern quad normals
    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 lightDirNorm = normalize(-lightDir);
    float diff = max(dot(normal, lightDirNorm), 0.0);

    // Ambient + diffuse, clamped to avoid overbright whites
    vec3 ambient = ambientColor * fs_in.SkyLight;
    vec3 diffuse = lightColor * diff * fs_in.SkyLight;

    vec3 result = min(ambient + diffuse, vec3(1.0)) * texColor.rgb;

    // Apply underwater tint — blue-green color absorption
    if (fs_in.IsUnderwater > 0.5) {
        vec3 waterTint = vec3(0.4, 0.7, 0.6);
        result *= waterTint;
    }

    FragColor = vec4(result, texColor.a);
}
