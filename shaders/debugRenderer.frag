#version 460 core
out vec4 FragColor;
in  vec2 TexCoords;
  
uniform sampler2D fboAttachment;
uniform int isGrayscale; // 1 for single-channel textures like SSAO
  
void main()
{
    vec4 texColor = texture(fboAttachment, TexCoords);
    if (isGrayscale == 1) {
        float val = texColor.r;
        FragColor = vec4(val, val, val, 1.0);
    } else {
        FragColor = texColor;
    }
} 