#version 460 core

out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform int kernelSize;
uniform vec3 samples[64];
uniform mat4 projection;
uniform float bias;
uniform float radius;
uniform float power;

// Tile noise texture over screen based on screen dimensions / noise size
uniform vec2 noiseScale;

void main() {
    vec3 fragPos    = texture(gPosition, TexCoords).xyz;
    vec3 normal     = texture(gNormal, TexCoords).rgb;
    vec3 randomVec  = texture(texNoise, TexCoords * noiseScale).xyz;

    // As we set the tiling parameters of texNoise to GL_REPEAT, the random values will be repeated all over the screen.
    // Together with the fragPos and normal vector, we then have enough data to create a TBN matrix
    // that transforms any vector from tangent-space to view-space:
    vec3 tangent    = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent  = cross(normal, tangent);
    mat3 TBN        = mat3(tangent, bitangent, normal);

    int adaptiveKernelSize = clamp(int(mix(16.0, float(kernelSize), smoothstep(0.1, 5.0, abs(fragPos.z)))), 8, kernelSize);
    
    // Using a process called the Gramm-Schmidt process we create an orthogonal basis, each time slightly tilted based on the value of randomVec.
    // Next we iterate over each of the kernel samples, transform the samples from tangent to view-space,
    // add them to the current fragment position, and compare the fragment position's depth with the sample depth stored in the view-space position buffer.
    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        // get sample position
        vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        samplePos = fragPos + samplePos * radius;

        vec4 offset = vec4(samplePos, 1.0);
        offset      = projection * offset;      // from view to clip-space
        offset.xyz /= offset.w;                 // perspective divide
        offset.xyz  = offset.xyz * 0.5 + 0.5;   // transform to range 0.0 - 1.0

        float sampleDepth = texture(gPosition, offset.xy).z;

        // We introduce a range check that makes sure a fragment contributes to the occlusion factor if its depth values is within the sample's radius
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / kernelSize);
    FragColor = pow(occlusion, power);
}