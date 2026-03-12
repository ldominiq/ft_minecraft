#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
} fs_in;

out vec4 FragColor;

uniform sampler2DArray blockTextures;
uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

void main() {
    // Sample texture from array
    vec4 texColor = texture(blockTextures, vec3(fs_in.TexCoord, fs_in.TexLayer));

    // Discard fully transparent pixels (for cross-pattern vegetation)
    if (texColor.a < 0.1)
        discard;

    // Simple diffuse lighting
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDirNorm = normalize(-lightDir);
    float diff = max(dot(normal, lightDirNorm), 0.0);

    // Ambient + diffuse
    vec3 ambient = ambientColor * fs_in.SkyLight;
    vec3 diffuse = lightColor * diff * fs_in.SkyLight;

    vec3 result = (ambient + diffuse) * texColor.rgb;

    FragColor = vec4(result, texColor.a);
}
