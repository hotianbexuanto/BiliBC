#version 330 core
/* Kawase downsample: 5-tap filter */
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uHalfPixel;  /* 0.5 / textureSize */

void main() {
    vec4 sum = texture(uTexture, vTexCoord) * 4.0;
    sum += texture(uTexture, vTexCoord - uHalfPixel);
    sum += texture(uTexture, vTexCoord + uHalfPixel);
    sum += texture(uTexture, vTexCoord + vec2(uHalfPixel.x, -uHalfPixel.y));
    sum += texture(uTexture, vTexCoord - vec2(uHalfPixel.x, -uHalfPixel.y));
    FragColor = sum / 8.0;
}
