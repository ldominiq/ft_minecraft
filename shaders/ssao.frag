#version 460 core

out float FragColor;

in vec2 TexCoords;

uniform sampler2D gDepth;    // Depth buffer (GL_DEPTH_COMPONENT)
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform int kernelSize;
uniform vec3 samples[64];
uniform mat4 projection;
uniform mat4 invProjection; // inverse projection to reconstruct view-space pos
uniform float bias;
uniform float radius;
uniform float power;

// Tile noise texture over screen based on screen dimensions / noise size
uniform vec2 noiseScale;

// Reconstruct view-space position from depth buffer + screen UV.
vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

void main() {
    float depth = texture(gDepth, TexCoords).r;

    // Sky early-out: depth at 1.0 means nothing was drawn (far plane / cleared).
    if (depth >= 1.0) {
        FragColor = 1.0;
        return;
    }

    vec3 fragPos    = reconstructViewPos(TexCoords, depth);
    vec3 normal     = texture(gNormal, TexCoords).rgb;
    vec3 randomVec  = texture(texNoise, TexCoords * noiseScale).xyz;
    // As we set the tiling parameters of texNoise to GL_REPEAT, the random values will be repeated all over the screen.
    // Together with the fragPos and normal vector, we then have enough data to create a TBN matrix
    // that transforms any vector from tangent-space to view-space:
    vec3 tangent    = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent  = cross(normal, tangent);
    mat3 TBN        = mat3(tangent, bitangent, normal);

    // Adaptive kernel: fewer samples for distant geometry
    float distFactor = smoothstep(5.0, 50.0, abs(fragPos.z));
    int minSamples = max(4, kernelSize / 4);
    int adaptiveKernelSize = clamp(int(mix(float(kernelSize), float(minSamples), distFactor)), minSamples, kernelSize);

    float occlusion = 0.0;
    for (int i = 0; i < adaptiveKernelSize; ++i) {
        vec3 samplePos = TBN * samples[i];
        samplePos = fragPos + samplePos * radius;

        // Project sample to screen space
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        // Sample depth at the projected position
        float sampleDepth = texture(gDepth, offset.xy).r;
        float sampleZ = reconstructViewPos(offset.xy, sampleDepth).z;

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleZ));
        occlusion += (sampleZ >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / adaptiveKernelSize);
    FragColor = pow(occlusion, power);
}