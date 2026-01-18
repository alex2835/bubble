#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uTintColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);

    // Discard fully transparent pixels
    if (texColor.a < 0.01)
        discard;

    FragColor = texColor * uTintColor;
}
