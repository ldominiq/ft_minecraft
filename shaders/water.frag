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

// Distance fog (sky LUT blending)
uniform sampler2D skyLUT;
uniform float skyExposure;
uniform float fogStart;
uniform float fogEnd;
uniform float fogStrength;
uniform bool fogEnabled;
uniform vec3 sunDir;

#define M_PI 3.1415926535897932384626433832795
const float FOG_G = 0.76;

float fogRayleighPhase(float mu) {
    return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);
}

float fogMiePhase(float mu) {
    float g2 = FOG_G * FOG_G;
    float denom = pow(1.0 + g2 - 2.0 * FOG_G * mu, 1.5);
    return (3.0 / (8.0 * M_PI)) * (1.0 - g2) * (1.0 + mu * mu) / ((2.0 + g2) * denom);
}

vec3 fogUncharted2(vec3 color) {
    float A=0.15, B=0.50, C=0.10, D=0.20, E=0.02, F=0.30, W=11.2, gamma=2.2;
    color *= skyExposure;
    color = ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F)) - E/F;
    float white = ((W*(A*W+C*B)+D*E)/(W*(A*W+B)+D*F)) - E/F;
    color /= white;
    return pow(max(color, vec3(0.0)), vec3(1.0/gamma));
}

vec3 getWaterFogColor(vec3 viewDir) {
    vec2 lutUV  = vec2(viewDir.y * 0.5 + 0.5, sunDir.y * 0.5 + 0.5);
    vec4 scatter = texture(skyLUT, lutUV);
    float mu = dot(viewDir, sunDir);
    vec3 col = scatter.rgb * fogRayleighPhase(mu) + vec3(scatter.a) * fogMiePhase(mu);
    return fogUncharted2(col);
}

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

    if (fogEnabled) {
        float dist = length(toCameraVector);
        float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, dist), fogStrength);
        vec3 viewDir = normalize(-toCameraVector); // direction from camera toward water
        vec3 fogColor = getWaterFogColor(viewDir);
        FragColor.rgb = mix(fogColor, FragColor.rgb, fogFactor);
        // Also fade alpha so water edge softens into fog
        FragColor.a *= fogFactor;
    }
}