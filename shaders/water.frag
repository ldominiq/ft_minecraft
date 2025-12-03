#version 460 core

in vec4 clipSpace;
in vec3 toCameraVector;
in vec2 textureCoords;
in vec3 fromLightVector;
in vec3 lightPos;

out vec4 FragColor;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform sampler2D refractionDepthTexture;

uniform float moveFactor;
uniform vec3 lightColor;
uniform float horizonY;           // horizon altitude (world Y) below which specular is disabled
uniform float twilightBand;       // half-width of the smooth fade zone around the horizon
uniform float nearPlane;
uniform float farPlane;

// Water properties
uniform float waveStrength;        // Distortion intensity
const float shineDamper = 20.0;
const float reflectivity = 0.5;

void main() {
    vec2 ndc = (clipSpace.xy/clipSpace.w) * 0.5 + 0.5;
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);

    float depth = texture(refractionDepthTexture, refractTexCoords).r;
    float floorDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depth - 1.0) * (farPlane - nearPlane));

    depth = gl_FragCoord.z;
    float waterDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depth - 1.0) * (farPlane - nearPlane));
    float waterDepth = floorDistance - waterDistance;

    vec2 distortedTexCoords = texture(dudvMap, vec2(textureCoords.x + moveFactor, textureCoords.y)).rg * 0.1;
    distortedTexCoords = textureCoords + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength * clamp(waterDepth/20.0, 0.0, 1.0);

    refractTexCoords += totalDistortion;
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

    reflectTexCoords += totalDistortion;

    vec4 waterColor = vec4(0.0, 0.3, 0.5, 1.0);
    vec4 murkyWaterColor = vec4(0.0, 0.5, 0.275, 1.0);

    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);
    refractColor = mix(refractColor, murkyWaterColor, clamp(waterDepth/60.0, 0.0, 1.0));

    // Normal calculations
    vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 3.0, normalMapColor.g * 2.0 - 1.0);
    normal = normalize(normal);

    // Fresnel calculation
    vec3 viewVector = normalize(toCameraVector);
    float refractiveFactor = dot(viewVector, normal);
    refractiveFactor = pow(refractiveFactor, 1.0); // The higer the value the more reflective when looking at an angle
    refractiveFactor = clamp(refractiveFactor, 0.001, 0.999);

    // Light reflection calculation
    vec3 reflectedLight = reflect(normalize(fromLightVector), normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, shineDamper);
    vec3 specularHighlights = lightColor * specular * reflectivity * clamp(waterDepth/5.0, 0.0, 1.0);

    // Smoothly fade specular highlights around the horizon
    // dayFactor = 0 when lightPosition.y <= horizonY - twilightBand
    // dayFactor = 1 when lightPosition.y >= horizonY + twilightBand
    float dayFactor = smoothstep(horizonY - twilightBand, horizonY + twilightBand, lightPos.y);
    specularHighlights *= dayFactor;

    FragColor = mix(reflectColor, refractColor, refractiveFactor);
    FragColor = mix(FragColor, waterColor, 0.2) + vec4(specularHighlights, 0.0); // water blue tint
    FragColor.a = clamp(waterDepth/5.0, 0.0, 1.0); // softens the edges of the water
//    FragColor = normalMapColor;
//    FragColor = vec4(waterDepth/50.0);

	if (!gl_FrontFacing) {
		// draw the inside with transparency
		FragColor.a *= 0.8;
}