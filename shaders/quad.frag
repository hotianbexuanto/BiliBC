#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uAlpha;

void main() {
    FragColor = texture(uTexture, vTexCoord);
    FragColor.a *= uAlpha;
}
