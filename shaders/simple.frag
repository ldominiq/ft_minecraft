#version 460 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D atlas;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

uniform vec3 viewPos;

float near = 0.1;
float far  = 100.0;

float specularStrength = 0.5;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main() {
    vec4 texColor = texture(atlas, TexCoord);

    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 diffuse = diff * lightColor;

    

    // View direction vector and corresponding reflect vector along normal axis.
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    // Specular component calc
    // shininess value of the highlight
    int shininess = 32;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 lighting = texColor.rgb * (ambientColor + diffuse + specular);

    //FragColor = texColor;
    FragColor = vec4(lighting, texColor.a); // Lighting
    //FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0); // Visualize normals
    //FragColor = vec4(TexCoord, 0.0, 1.0); // Visualize texture coordinates

    //float depth = LinearizeDepth(gl_FragCoord.z) / far; // Visualize depth buffer
    //FragColor = vec4(vec3(depth), 1.0);
}